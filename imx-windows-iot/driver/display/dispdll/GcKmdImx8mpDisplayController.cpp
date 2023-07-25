/* Copyright (c) Microsoft Corporation.
 * Copyright 2023 NXP
   Licensed under the MIT License. */

#include "precomp.h"

#include "GcKmdImx8mpDisplayController.h"
#include "GcKmdImx8Display.h"

#include "getresrc.h"

GcKmImx8mpDisplayController::GcKmImx8mpDisplayController(
    DXGKRNL_INTERFACE*  pDxgkInterface)
    : GcKmBaseDisplayController(pDxgkInterface)
{
    BOOLEAN UseDisplays[DISP_INTERFACE_PARALLEL_LCD + 1] = {};
    WCHAR   szDisplayInterface[] = L"Display0Interface";
    NTSTATUS    Status;
    ULONG   InterfaceIndex;

    for (UINT i = 1; i <= DISP_INTERFACE_PARALLEL_LCD; i++)
    {
        InterfaceIndex = 0;
        Status = GetDwordRegistryParam(pDxgkInterface, szDisplayInterface, &InterfaceIndex);
        if (NT_SUCCESS(Status) &&
            (InterfaceIndex <= DISP_INTERFACE_PARALLEL_LCD))
        {
            UseDisplays[InterfaceIndex] = TRUE;
        }

        //
        // Increase the display interface index
        //
        szDisplayInterface[7]++;
    }

    BOOLEAN bMonitorsEnabled = FALSE;

    for (UINT i = 1; i <= DISP_INTERFACE_PARALLEL_LCD; i++)
    {
        if (UseDisplays[i])
        {
            bMonitorsEnabled = TRUE;
        }
    }

    if (FALSE == bMonitorsEnabled)
    {
        UseDisplays[DISP_INTERFACE_HDMI] = TRUE;
    }

    if (UseDisplays[DISP_INTERFACE_LVDS0] || 
        UseDisplays[DISP_INTERFACE_LVDS1] || 
        UseDisplays[DISP_INTERFACE_LVDS_DUAL0])
    {
        RegisterDisplayPipeline(&m_LvdsDisplay);
    }

    if (UseDisplays[DISP_INTERFACE_HDMI])
    {
        RegisterDisplayPipeline(&m_HdmiDisplay, true);
    }
}

