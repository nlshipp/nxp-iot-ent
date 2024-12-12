// Copyright (c) Microsoft Corporation. All rights reserved.
// Copyright 2022, 2024 NXP
// Licensed under the MIT License.
//
// imxtmutil
//
//   Simple utility to dump temperature from the thermal unit.
//

#include <windows.h>
#include <winioctl.h>
#include <strsafe.h>
#include <cfgmgr32.h>

#include <stdio.h>
#include <string>
#include <iostream>
#include <sstream>
#include <memory>

#include <wrl.h>

#include <initguid.h>
#include "thermalioctl.h"
#include "util.h"
#include <poclass.h>

using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;

std::wstring GetInterfacePath (const GUID& InterfaceGuid)
{
    ULONG length;
    CONFIGRET cr = CM_Get_Device_Interface_List_SizeW(
            &length,
            const_cast<GUID*>(&InterfaceGuid),
            nullptr,        // pDeviceID
            CM_GET_DEVICE_INTERFACE_LIST_PRESENT);

    if (cr != CR_SUCCESS) {
        throw wexception::make(
            HRESULT_FROM_WIN32(CM_MapCrToWin32Err(cr, ERROR_NOT_FOUND)),
            L"Failed to get size of device interface list. (cr = 0x%x)",
            cr);
    }

    if (length < 2) {
        throw wexception::make(
            HRESULT_FROM_WIN32(CM_MapCrToWin32Err(cr, ERROR_NOT_FOUND)),
            L"The IMXTMU device was not found on this system. (cr = 0x%x), length = %d",
            cr, length);
    }

    std::unique_ptr<WCHAR[]> buf(new WCHAR[length]);
    cr = CM_Get_Device_Interface_ListW(
            const_cast<GUID*>(&InterfaceGuid),
            nullptr,        // pDeviceID
            buf.get(),
            length,
            CM_GET_DEVICE_INTERFACE_LIST_PRESENT);

    //uncomment to see ACPI records connected to the interface
    //for (ULONG i = 0; i < length; i++) {
    //    std::wcout << buf.get() << std::endl;
    //}
    

    if (cr != CR_SUCCESS) {
        throw wexception::make(
            HRESULT_FROM_WIN32(CM_MapCrToWin32Err(cr, ERROR_NOT_FOUND)),
            L"Failed to get device interface list. (cr = 0x%x)",
            cr);
    }

    // Return the first string in the multistring
    return std::wstring(buf.get());
}

FileHandle OpenImxTmuHandle ()
{
    auto interfacePath = GetInterfacePath(GUID_DEVINTERFACE_TMU);

    FileHandle fileHandle(CreateFile(
            interfacePath.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            0,          // dwShareMode
            nullptr,    // lpSecurityAttributes
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr));  // hTemplateFile


    if (!fileHandle.IsValid()) {
        if (GetLastError() == ERROR_ACCESS_DENIED) {
            // Try opening read-only
            fileHandle.Attach(CreateFile(
                interfacePath.c_str(),
                GENERIC_READ,
                0,          // dwShareMode
                nullptr,    // lpSecurityAttributes
                OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL,
                nullptr));  // hTemplateFile

            if (fileHandle.IsValid()) {
                return fileHandle;
            }
        }

        throw wexception::make(
            HRESULT_FROM_WIN32(GetLastError()),
            L"Failed to open a handle to the imxtmu device. "
            L"(GetLastError() = 0x%x, interfacePath = %s)",
            GetLastError(),
            interfacePath.c_str());
    }

    //std::wcout << "Open tmu filehandle end." << std::endl;

    return fileHandle;
}

#define IOCTL_THERMAL_READ_TEMPERATURE\
        CTL_CODE(FILE_DEVICE_BATTERY, 0x24, METHOD_BUFFERED, FILE_READ_ACCESS)

//This IOCTL is not "allowed" in the normal IOCTL sequence, it is due to easier
//debugging as the IOCTL above is also used by the thermal client.
//In release BSP, this will be removed and above will be used for both.
//This will remain commented for further debugging if required.
#define IOCTL_THERMAL_READ_TEMP2\
        CTL_CODE(FILE_DEVICE_BATTERY, 0x26, METHOD_BUFFERED, FILE_READ_ACCESS)

int HandleDumpCommand (int argc, _In_reads_(argc) wchar_t * /*argv*/ [])
{
    // imxtmuutil dump
    if (argc > 2) {
        fwprintf(stderr, L"Too many arguments to 'dump' command\n");
        return 1;
    }

    auto tmuHandle = OpenImxTmuHandle();

    //std::wcout << "Dump begin." << std::endl;

    PTHERMAL_WAIT_READ input = new THERMAL_WAIT_READ;
    input->Timeout = 0;
    input->LowTemperature = 0;
    input->HighTemperature = 0xb7c;
    DWORD output;
    DWORD information;
    if (!DeviceIoControl(
            tmuHandle.Get(),
            //IOCTL_THERMAL_READ_TEMP2,
            IOCTL_THERMAL_READ_TEMPERATURE,
            &input,
            sizeof(THERMAL_WAIT_READ),
            &output,
            sizeof(ULONG),
            &information,
            nullptr) || (information != sizeof(ULONG))) {

        std::wcout << "Throwing error." << std::endl;
        throw wexception::make(
            HRESULT_FROM_WIN32(GetLastError()),
            L"IOCTL_THERMAL_READ_TEMPERATURE failed. "
            L"(GetLastError() = 0x%x, information = %d)",
            GetLastError(),
            information);
    }

    double celsius = output / 10 - 273.15;

    std::wcout << "Temperature is: " << celsius << '\370' << "C" << std::endl;

    
    return 0;


}


void PrintUsage ()
{
    PCWSTR Usage =
L"thermalutil: IMX Thermal Utility\n"
L"Usage: thermalutil [dump]\n"
L"\n"
L" dump                        Dump current CPU temperature\n"
L"\n"
L"Examples:\n"
L"  Dump temperature values:\n"
L"    thermalutil dump\n"
L"\n";

    wprintf(Usage);
}

int mainexcpt (_In_ int argc, _In_reads_(argc) wchar_t* argv[])
{
    if (argc < 2) {
        fwprintf(
            stderr,
            L"Missing required parameter 'command'. Run 'thermalutil /?' for usage.\n");

        return 1;
    }

    PCWSTR command = argv[1];
    if (!_wcsicmp(command, L"-h") || !_wcsicmp(command, L"/h") ||
        !_wcsicmp(command, L"-?") || !_wcsicmp(command, L"/?")) {

        PrintUsage();
        return 0;
    }

    if (!_wcsicmp(command, L"dump")) {
        return HandleDumpCommand(argc, argv);
    } else {
        fwprintf(
            stderr,
            L"Unrecognized command: %s. Type '%s /?' for usage.\n",
            command,
            argv[0]);

        return 1;
    }
}

int __cdecl wmain (_In_ int argc, _In_reads_(argc) wchar_t* argv[])
{
    try {
        return mainexcpt(argc, argv);
    } catch (const wexception& ex) {
        fwprintf(stderr, L"Error: %s\n", ex.wwhat());
        return 1;
    }
}
