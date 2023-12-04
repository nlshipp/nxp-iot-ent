@echo off
:: Copyright 2020, 2022-2023 NXP
::  
:: Redistribution and use in source and binary forms, with or without modification, 
:: are permitted provided that the following conditions are met: 
::
:: Redistributions of source code must retain the above copyright notice, this list 
:: of conditions and the following disclaimer. 
::
:: Redistributions in binary form must reproduce the above copyright notice, this 
:: list of conditions and the following disclaimer in the documentation and/or 
:: other materials provided with the distribution. 
::
:: Neither the name of the copyright holder nor the names of its 
:: contributors may be used to endorse or promote products derived from this 
:: software without specific prior written permission. 
::
:: THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND 
:: ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
:: WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE 
:: DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR 
:: ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES 
:: (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; 
:: LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON 
:: ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
:: (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS 
:: SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
:: 

::
:: Run this script in order to prepare a WinPE image for boot on i.MX8 and i.MX9
::

setlocal enableextensions
setlocal enabledelayedexpansion

for /F "tokens=1,2 delims=#" %%a in ('"prompt #$H#$E# & echo on & for %%b in (1) do rem"') do ( @REM Loop conencted to single-line coloring of text
  set "DEL=%%a"
)


set CREATE_WIN_ENTERPRISE_IMAGE=1
set CREATE_WIN_PE_LAYOUT=1
set OUT_DIR_REL=out
set IMX_WIN_ENTERPRISE_IMAGE_FILE_NAME=imx_win_iot_install.wim
set IMX_WIN_ENTERPRISE_IMAGE=%OUT_DIR_REL%\%IMX_WIN_ENTERPRISE_IMAGE_FILE_NAME%
set IMX_WIN_ENTRPRISE_MOUNT_DIR=%OUT_DIR_REL%\mount_enterprise
set MSFT_WIN_ENTERPRISE_IMAGE=
set UNATTEND=no

set NO_PATCH_ENT=no
set NO_PATCH_PE=no

set CUMULATIVE_UPDATE=no
set CUMULATIVE_UPDATE_PATH=

set WIN_PE_SRC_DIR_ABS=%ProgramFiles(x86)%\Windows Kits\10\Assessment and Deployment Kit\Windows Preinstallation Environment\arm64
set WINPE_DRIVE_LETTER=O
set WIN_PE_MOUNT_DIR=%OUT_DIR_REL%\mount_winpe
set DISABLE_UPDATES=no
set DISABLE_TRANSPARENCY=yes
set SCRIPT_DIR=%~dp0
set WIN_PE_DST_DIR=%OUT_DIR_REL%\winpe-imx-sdcard-layout
set SWM_IMG=%WIN_PE_DST_DIR%\install.swm
set SPLIT_WIM_SIZE=4000
set SPLIT_WIM_SIZE_KBYTES=%SPLIT_WIM_SIZE%000
set TEST_SIGNING=no
set DISM_UNSIGNED_ARG=
set CLEAN=
set SPLIT_WIM=no

set WINPE_DEBUG_SERIAL=no
set WINPE_DEBUG_NET=no
set WIN_DEBUG_SERIAL=no
set WIN_DEBUG_NET=no
set DEBUG_IP=
set NET_DEBUG=
set KD_NET=no
set NO_UNATTEND=
set APPLY=0

set BINPATCH_DIR=%SCRIPT_DIR%binpatch
set KB_PATCH_DIR=kbpatch

set WIN_PE_STARTNET_CMD=%WIN_PE_DST_DIR%\startnet.cmd.txt
set WIN_ENTERPRISE_INSTALL_CMD=%WIN_PE_DST_DIR%\install.cmd.txt
set DRIVER_DIR=%SCRIPT_DIR%drivers


set ISO_FILE=
set MOUNT_DIR=C:\mount
set ISO_WIM_PATH=:\sources\install.wim
set ISO_LETTER=
set ISO_PATH=
set WIM_PATH=
set EXTRACT_WIM=no

:: Parse options
:GETOPTS
if /I "%~1" == "/?" ( goto USAGE
) else (if /I "%~1" == "/Help" ( goto USAGE
) else (if /I "%~1" == "/source_image" ( set MSFT_WIN_ENTERPRISE_IMAGE=%2& ; shift
) else (if /I "%~1" == "/cumulative_update" ( set CUMULATIVE_UPDATE=yes
                                              set CUMULATIVE_UPDATE_PATH=%2& shift 
                                              set SPLIT_WIM=yes
) else (if /I "%~1" == "/apply" ( set APPLY=1
                                  set DISK_NUM=%2& shift
) else (if /I "%~1" == "/device" ( shift
) else (if /I "%~1" == "/clean" ( set CLEAN=1
) else (if /I "%~1" == "/winpedebug_serial" ( set WINPE_DEBUG_SERIAL=yes
) else (if /I "%~1" == "/winpedebug_net" ( set WINPE_DEBUG_NET=yes
                                           set NET_DEBUG=1
) else (if /I "%~1" == "/windebug_serial" ( set WIN_DEBUG_SERIAL=yes
) else (if /I "%~1" == "/windebug_net" ( set WIN_DEBUG_NET=yes
                                         set NET_DEBUG=1
) else (if /I "%~1" == "/debug_ip" ( set DEBUG_IP=%2& shift
) else (if /I "%~1" == "/test_signing" ( set TEST_SIGNING=yes
) else (if /I "%~1" == "/unattend" (   break
) else (if /I "%~1" == "/no_unattend" ( set NO_UNATTEND=1
) else (if /I "%~1" == "/disable_updates" ( set DISABLE_UPDATES=yes
) else (if /I "%~1" == "/enable_transparency" ( set DISABLE_TRANSPARENCY=no
) else (if /I "%~1" == "/split_wim" ( set SPLIT_WIM=yes
) else ( if /I "%~1" == "/no_patch" ( set NO_PATCH_ENT=yes
                                      set NO_PATCH_PE=yes
) else ( if /I "%~1" == "/no_patch_ent" ( set NO_PATCH_ENT=yes
) else ( if /I "%~1" == "/no_patch_pe" ( set NO_PATCH_PE=yes
) else ( if /I "%~1" == "/iso" ( set EXTRACT_WIM=yes
) else (if /I "%~1" == "/?" ( goto USAGE
) else ( if /I "%~1" == "" ( break
) else (
   set ERR_MSG=Unsupported option: "%~1".
   goto PrErr
)
)))))))))))))))))))))))
shift
if not (%1)==() goto GETOPTS
:: Print newline
echo:

if not "%SCRIPT_DIR%" == "%CD%\" (
    echo Script needs to be run from directory it's located in^^!
    echo Please change your working directory to "%SCRIPT_DIR%" and run the script again.
    goto ErrExit
)

if not "%CLEAN%" == "" goto CLEAN

if not "%DISK_NUM%" == "" goto APPLY
if not "%APPLY%" == "0" (
    echo Option /apply needs to be used with disk number for applying Windows PE layout.
    echo Use /? for more help
    goto :ErrExit
)

@REM If test signing is enabled, DISM commands injecting drivers into images must contain /ForceUnsigned argument
if "%TEST_SIGNING%" == "yes" (
    set DISM_UNSIGNED_ARG=/ForceUnsigned
)

if %CUMULATIVE_UPDATE% == yes (
    if '%CUMULATIVE_UPDATE_PATH%' == '' (
        echo Cumulative update path need to be selected with option /cumulative_update
        goto ErrExit
    ) else (
        set CUMULATIVE_UPDATE_PATH=%CUMULATIVE_UPDATE_PATH:"=%
        if not exist "!CUMULATIVE_UPDATE_PATH!" (
            echo Cumulative update !CUMULATIVE_UPDATE_PATH! not found.
            goto ErrExit
        ) 
    )
) 

if not "%NET_DEBUG%" == "" (
    if "%DEBUG_IP%" == "" (
        echo IP for debugging over net is not specified.
        echo Use argument /debug_ip to specify.
        goto ErrExit
    ) else (
        echo %DEBUG_IP%| findstr /r /b /e "[0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*">nul
        if errorlevel 1 (
            echo IP for debugging is in incorrect format.
            goto ErrExit
        )
    )
)

if not exist "%WIN_PE_SRC_DIR_ABS%\Media" (
    echo You must install the Windows PE Add-on for the ADK
    echo https://docs.microsoft.com/en-us/windows-hardware/manufacture/desktop/download-winpe--windows-pe
    echo:
    goto err
)

if "%CREATE_WIN_ENTERPRISE_IMAGE%" == "1" (
    if '!MSFT_WIN_ENTERPRISE_IMAGE!' == '' (
        if not "%EXTRACT_WIM%" == "yes" (
            set WIM_COUNT=0
            for /f "tokens=*" %%i in ('dir /b *.wim') do (
                set /a WIM_COUNT+=1
            )
            if !WIM_COUNT! == 0 (
                echo No image file was specified by /source_image parameters and no image was found in the current directory.
                echo Copy install.wim into current directory.
                goto ErrExit
            )

            if !WIM_COUNT! NEQ 1 (
                set WIM_COUNT=0
                echo No image file was specified by /source_image parameters and more than one image was found in the current directory.
                echo Specify what image to use:
                for /f "tokens=*" %%i in ('dir /b *.wim') do (
                    set /a WIM_COUNT+=1
                    echo !WIM_COUNT!. %%i
                )
                set /p IMG_IDX=Enter index:
                echo index: !IMG_IDX!
                if !IMG_IDX! gtr !WIM_COUNT! (
                    echo Selected index is out of range
                    goto ErrExit
                )
                if !IMG_IDX! leq 0 (
                    echo Selected index is out of range
                    goto ErrExit
                )
            ) else (
                set IMG_IDX=1
            )
                
            set WIM_COUNT=0
            for /f "tokens=*" %%i in ('dir /b *.wim') do (
                set /a WIM_COUNT+=1
                if !WIM_COUNT! == !IMG_IDX! (
                    echo Selected: "%%i"
                    echo:
                    set MSFT_WIN_ENTERPRISE_IMAGE=%%i
                )
            )
        
        ) else (
            set ISO_COUNT=0
            for /f "tokens=*" %%i in ('dir /b *.iso') do (
                set /a ISO_COUNT+=1
            )
            if !ISO_COUNT! == 0 (
                echo No image file was specified by /source_image parameters and no image was found in the current directory.
                echo Copy Windows ISO file into current directory.
                goto ErrExit
            )

            if !ISO_COUNT! NEQ 1 (
                set ISO_COUNT=0
                echo No image file was specified by /source_image parameters and more than one image was found in the current directory.
                echo Specify what image to use:
                for /f "tokens=*" %%i in ('dir /b *.iso') do (
                    set /a ISO_COUNT+=1
                    echo !ISO_COUNT!. %%i
                )
                set /p IMG_IDX=Enter index:
                echo index: !IMG_IDX!
                if !IMG_IDX! gtr !ISO_COUNT! (
                    echo Selected index is out of range
                    goto ErrExit
                )
                if !IMG_IDX! leq 0 (
                    echo Selected index is out of range
                    goto ErrExit
                )
            ) else (
                set IMG_IDX=1
            )
                
            set ISO_COUNT=0
            for /f "tokens=*" %%i in ('dir /b *.iso') do (
                set /a ISO_COUNT+=1
                if !ISO_COUNT! == !IMG_IDX! (
                    echo Selected: "%%i"
                    echo:
                    set MSFT_WIN_ENTERPRISE_IMAGE=%%i
                )
            )
        )
    ) else (
        set MSFT_WIN_ENTERPRISE_IMAGE=%MSFT_WIN_ENTERPRISE_IMAGE:"=%
    )
)

if not "%NET_DEBUG%" == "" (
    set DLL_IDX=
    set DLL_COUNT=0
    
    echo The /winpedebug_net or /windebug_net option has been enabled. Debugging over net requires kd_8003_1fc9.dll extension present on the target. Script will check for it in this directory and try to provision the device. If you need this file, please contact Microsoft.
    
    for /f "tokens=*" %%i in ('dir /b *.dll') do (
        set /a DLL_COUNT+=1
    )
    
    if !DLL_COUNT! == 0 (
        Choice /M "Warning: The kd_8003_1fc9.dll file was not found in current directory. Do you want to continue?"
        If ErrorLevel 2 GoTo :ErrExit
        set DLL_IDX=0
    )

    if !DLL_COUNT! == 1 (
        if exist kd_8003_1fc9.dll (
            set DLL_IDX=1
        )
    )

    if "!DLL_IDX!" == "" (
        set DLL_COUNT=0
        echo These .dll files were found in current directory. Please specify, which is kd_8003_1fc9.dll, that will be used for debugging over ethernet.
        echo Specify what dll file to use:
        echo 0. Do not copy kd_8003_1fc9.dll into the images, only turn on debug in configuration. ^(kd_net extension dll will be still needed for debugging.^)
        for /f "tokens=*" %%i in ('dir /b *.dll') do (
            set /a DLL_COUNT+=1
            echo !DLL_COUNT!. %%i
        )
        set /p DLL_IDX=Enter index:
        echo index: !DLL_IDX!
        if !DLL_IDX! gtr !DLL_COUNT! (
            echo Selected index is out of range
            goto ErrExit
        )
        if !DLL_IDX! lss 0 (
            echo Selected index is out of range
            goto ErrExit
        )
    )

    if !DLL_IDX! neq 0 (
        set DLL_COUNT=0
        for /f "tokens=*" %%i in ('dir /b *.dll') do (
            set /a DLL_COUNT+=1
            if !DLL_COUNT! == !DLL_IDX! (
                echo Selected: "%%i"
                echo:
                set KD_NET=%%i
            )
        )
    )
)





if "%NO_UNATTEND%" == "" (
    if exist "%SCRIPT_DIR%\unattend.xml" (
        set UNATTEND=yes
    )
)

echo:
echo OPTIONS SUMMARY:
echo Source image:                          !MSFT_WIN_ENTERPRISE_IMAGE!
echo Test signing:                          %TEST_SIGNING%
echo Windows debug over ethernet:           %WIN_DEBUG_NET%, IP: %DEBUG_IP%
echo Windows PE debug over ethernet:        %WINPE_DEBUG_NET%, IP: %DEBUG_IP%
echo KD_NET file ^(for debug over net^):      !KD_NET!
echo Windows debug over serial:             %WIN_DEBUG_SERIAL%
echo Windows PE debug over serial:          %WINPE_DEBUG_SERIAL%
echo Unattended install answer file:        !UNATTEND!
echo Disable updates:                       %DISABLE_UPDATES%
echo Disable transparency:                  %DISABLE_TRANSPARENCY%
echo Split wim:                             %SPLIT_WIM%
echo Cumulative update:                     %CUMULATIVE_UPDATE%, path: !CUMULATIVE_UPDATE_PATH!
echo Skip updates to Windows Enterprise:    %NO_PATCH_ENT%
echo Skip updates to Windows PE:            %NO_PATCH_PE%
echo:


if not "%CREATE_WIN_ENTERPRISE_IMAGE%" == "1" (
    goto create_winpe_layout
)

echo *********************************************************************************************************************************************************
echo +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
echo *** Step 1 Creating i.MX Windows IoT Enterprise image: %IMX_WIN_ENTERPRISE_IMAGE%
echo *********************************************************************************************************************************************************
echo +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
echo:
if not exist !MSFT_WIN_ENTERPRISE_IMAGE! (
    echo Windows IoT Enterprise image file !MSFT_WIN_ENTERPRISE_IMAGE! not found.
    goto ErrExit
)
if not exist "%DRIVER_DIR%" (
    echo Driver directory "%DRIVER_DIR%" not found.
    goto ErrExit
)

REM Cleanup
if exist "%IMX_WIN_ENTRPRISE_MOUNT_DIR%" (    
    echo Cleaning up from previous run
    dir /b /s /a "%IMX_WIN_ENTRPRISE_MOUNT_DIR%" | findstr .>nul && (
        dism /unmount-wim /mountdir:"%IMX_WIN_ENTRPRISE_MOUNT_DIR%" /discard
    )
    rmdir /s /q "%IMX_WIN_ENTRPRISE_MOUNT_DIR%" 2> NUL
)

del /q "%IMX_WIN_ENTERPRISE_IMAGE%" 2> NUL

echo:
echo ---------------------------------------------------------------------------------------------------------------------------------------------------------
echo *** Step 1.1 Copying Windows Enterprise image !MSFT_WIN_ENTERPRISE_IMAGE! to %IMX_WIN_ENTERPRISE_IMAGE%
echo ---------------------------------------------------------------------------------------------------------------------------------------------------------

mkdir "%OUT_DIR_REL%"

if not "%EXTRACT_WIM%" == "yes" (    
    echo copy "!MSFT_WIN_ENTERPRISE_IMAGE!" "%IMX_WIN_ENTERPRISE_IMAGE%"
    copy "!MSFT_WIN_ENTERPRISE_IMAGE!" "%IMX_WIN_ENTERPRISE_IMAGE%" || goto ErrExit
) else (
    echo powershell -Command "(Mount-DiskImage -ImagePath '!SCRIPT_DIR!\!MSFT_WIN_ENTERPRISE_IMAGE!' | Get-Volume).DriveLetter"
    for /f "delims=" %%a in (' powershell -Command "(Mount-DiskImage -ImagePath '!SCRIPT_DIR!\!MSFT_WIN_ENTERPRISE_IMAGE!' | Get-Volume).DriveLetter" ') do set "ISO_LETTER=%%a"
    set WIM_PATH=!ISO_LETTER!%ISO_WIM_PATH%
    echo copy !WIM_PATH! "%IMX_WIN_ENTERPRISE_IMAGE%"
    copy !WIM_PATH! "%IMX_WIN_ENTERPRISE_IMAGE%"
    echo powershell -Command "Dismount-DiskImage !SCRIPT_DIR!\!MSFT_WIN_ENTERPRISE_IMAGE!"
    powershell -Command "Dismount-DiskImage !SCRIPT_DIR!\!MSFT_WIN_ENTERPRISE_IMAGE!" > nul
)


echo:
echo ---------------------------------------------------------------------------------------------------------------------------------------------------------
echo *** Step 1.2 Mounting i.MX Windows IoT Enterprise image at %IMX_WIN_ENTRPRISE_MOUNT_DIR%
echo ---------------------------------------------------------------------------------------------------------------------------------------------------------
mkdir "%IMX_WIN_ENTRPRISE_MOUNT_DIR%"
echo dism /mount-wim /wimfile:"%IMX_WIN_ENTERPRISE_IMAGE%" /mountdir:"%IMX_WIN_ENTRPRISE_MOUNT_DIR%" /index:2
dism /mount-wim /wimfile:"%IMX_WIN_ENTERPRISE_IMAGE%" /mountdir:"%IMX_WIN_ENTRPRISE_MOUNT_DIR%" /index:2 || goto ErrExit

echo:
echo ---------------------------------------------------------------------------------------------------------------------------------------------------------
echo *** Step 1.3 Injecting drivers from %DRIVER_DIR% into the i.MX Windows IoT Enterprise image
echo ---------------------------------------------------------------------------------------------------------------------------------------------------------
echo dism /Image:"%IMX_WIN_ENTRPRISE_MOUNT_DIR%" /Add-Driver /Driver:"%DRIVER_DIR%" /Recurse !DISM_UNSIGNED_ARG!
dism /Image:"%IMX_WIN_ENTRPRISE_MOUNT_DIR%" /Add-Driver /Driver:"%DRIVER_DIR%" /Recurse !DISM_UNSIGNED_ARG! || goto ErrExit

if not "!CUMULATIVE_UPDATE_PATH!" == "" (
    echo:
    echo ---------------------------------------------------------------------------------------------------------------------------------------------------------
    echo *** Installing cumulative updates from %CUMULATIVE_UPDATE_PATH% into the i.MX Windows IoT Enterprise image
    echo ---------------------------------------------------------------------------------------------------------------------------------------------------------
	echo dism /Image:"%IMX_WIN_ENTRPRISE_MOUNT_DIR%" /Add-Package /PackagePath="!CUMULATIVE_UPDATE_PATH!"
	dism /Image:"%IMX_WIN_ENTRPRISE_MOUNT_DIR%" /Add-Package /PackagePath="!CUMULATIVE_UPDATE_PATH!" || goto ErrExit
)

    echo ---------------------------------------------------------------------------------------------------------------------------------------------------------
    echo *** Applying cumulative updates from %KB_PATCH_DIR%\ directory to i.MX Windows IoT Enterprise image
    echo ---------------------------------------------------------------------------------------------------------------------------------------------------------
if %NO_PATCH_ENT% == no (
    set KB_COUNT=0
    for /f "tokens=*" %%i in ('dir /b %KB_PATCH_DIR%\*.msu') do (
        set /a KB_COUNT+=1
        echo Applying cumulative update %%i from %KB_PATCH_DIR%...
        call :clrEcho 02 "WARNING Depending on size of the update, this processs may take up to 30 minutes, do not stop the process even if it may appear to be frozen"
        echo:
        echo dism /Image:"%IMX_WIN_ENTRPRISE_MOUNT_DIR%" /Add-Package /PackagePath="%KB_PATCH_DIR%\%%i"
        dism /Image:"%IMX_WIN_ENTRPRISE_MOUNT_DIR%" /Add-Package /PackagePath="%KB_PATCH_DIR%\%%i" || goto ErrExit
    )
)

echo:
echo ---------------------------------------------------------------------------------------------------------------------------------------------------------
echo *** Step 1.4 Copying debug libraries into the i.MX Windows IoT Enterprise image
echo ---------------------------------------------------------------------------------------------------------------------------------------------------------




if "!UNATTEND!" == "yes" (
    echo:
    echo Copying unnatend into the i.MX Windows IoT Enterprise image
    echo md "%IMX_WIN_ENTRPRISE_MOUNT_DIR%\Windows\Panther" || goto ErrExit
    md "%IMX_WIN_ENTRPRISE_MOUNT_DIR%\Windows\Panther" || goto ErrExit
	echo xcopy /Y /R /F "%SCRIPT_DIR%\unattend.xml" "%IMX_WIN_ENTRPRISE_MOUNT_DIR%\Windows\Panther\*.xml"
	xcopy /Y /R /F "%SCRIPT_DIR%\unattend.xml" "%IMX_WIN_ENTRPRISE_MOUNT_DIR%\Windows\Panther\*.xml" || goto ErrExit
)



if "%WIN_DEBUG_NET%" == "yes" (
    if not "!KD_NET!" == "no" (
        echo:
        echo Copying kd_8003_1fc9.dll into the i.MX Windows IoT Enterprise image
        echo copy "%SCRIPT_DIR%\!KD_NET!" "%IMX_WIN_ENTRPRISE_MOUNT_DIR%\Windows\system32\kd_8003_1fc9.dll" || goto ErrExit
        copy "%SCRIPT_DIR%\!KD_NET!" "%IMX_WIN_ENTRPRISE_MOUNT_DIR%\Windows\system32\kd_8003_1fc9.dll" || goto ErrExit
    )
)

echo:
echo ---------------------------------------------------------------------------------------------------------------------------------------------------------
echo *** Step 1.5 Unmounting i.MX Windows IoT Enterprise image from: %IMX_WIN_ENTRPRISE_MOUNT_DIR%
echo ---------------------------------------------------------------------------------------------------------------------------------------------------------
echo dism /unmount-wim /mountdir:"%IMX_WIN_ENTRPRISE_MOUNT_DIR%" /commit
dism /unmount-wim /mountdir:"%IMX_WIN_ENTRPRISE_MOUNT_DIR%" /commit || goto ErrExit


rmdir /s /q "%IMX_WIN_ENTRPRISE_MOUNT_DIR%" 2> NUL
echo:
echo *********************************************************************************************************************************************************
echo *********************************************************************************************************************************************************
echo *** Step 1 DONE. i.MX Windows IoT Enterprise image succesfully created at %IMX_WIN_ENTERPRISE_IMAGE%.
echo *********************************************************************************************************************************************************
echo:

:create_winpe_layout

if not "%CREATE_WIN_PE_LAYOUT%" == "1" (
    exit /b 0
)

echo *********************************************************************************************************************************************************
echo *********************************************************************************************************************************************************
echo *** Step 2 Creating i.MX Windows PE layout at "%CD%%WIN_PE_DST_DIR%"
echo *********************************************************************************************************************************************************
echo *********************************************************************************************************************************************************

if exist "%WIN_PE_MOUNT_DIR%" (
    echo Cleaning up from previous run
    dir /b /s /a "%WIN_PE_MOUNT_DIR%" | findstr .>nul && (
        dism /unmount-wim /mountdir:"%WIN_PE_MOUNT_DIR%" /discard
    )
    rmdir /s /q "%WIN_PE_MOUNT_DIR%" 2> NUL
)
:: make temporary WinPE directory
rmdir /s /q "%WIN_PE_DST_DIR%" > NUL 2>&1
mkdir "%WIN_PE_DST_DIR%"  
mkdir "%WIN_PE_DST_DIR%\sources"


if not exist "%IMX_WIN_ENTERPRISE_IMAGE%" (
    echo File not found: "%IMX_WIN_ENTERPRISE_IMAGE%"
    goto err
) else (
    FOR /F "usebackq" %%A IN ('%IMX_WIN_ENTERPRISE_IMAGE%') DO set size=%%~zA

    set size=!size:~0,-3!


    if !size! GTR %SPLIT_WIM_SIZE_KBYTES% (
        call :clrEcho 02 "Image split enabled due to it's size being more than %SPLIT_WIM_SIZE_KBYTES%KB. Image size = !size!KB."
        set SPLIT_WIM=yes
    )

    if "!SPLIT_WIM!" == "yes" (
        echo:
        echo ---------------------------------------------------------------------------------------------------------------------------------------------------------
        echo *** Step 2.1 Splitting i.MX Windows IoT Enterprise image "%IMX_WIN_ENTERPRISE_IMAGE_FILE_NAME%" into "%WIN_PE_DST_DIR%"
        echo ---------------------------------------------------------------------------------------------------------------------------------------------------------
        echo dism /split-image /ImageFile:"%IMX_WIN_ENTERPRISE_IMAGE%" /SWMFile:"%SWM_IMG%" /FileSize:%SPLIT_WIM_SIZE%
        dism /split-image /ImageFile:"%IMX_WIN_ENTERPRISE_IMAGE%" /SWMFile:"%SWM_IMG%" /FileSize:%SPLIT_WIM_SIZE% || goto err
        echo del %IMX_WIN_ENTERPRISE_IMAGE%
        del %IMX_WIN_ENTERPRISE_IMAGE%
    ) else (
        echo:
        echo ---------------------------------------------------------------------------------------------------------------------------------------------------------
        echo *** Step 2.1 Copying i.MX Windows IoT Enterprise image "%IMX_WIN_ENTERPRISE_IMAGE_FILE_NAME%" from "%IMX_WIN_ENTERPRISE_IMAGE%" to "%WIN_PE_DST_DIR%"
        echo ---------------------------------------------------------------------------------------------------------------------------------------------------------
        echo xcopy /hrky "%IMX_WIN_ENTERPRISE_IMAGE%" "%WIN_PE_DST_DIR%\*"
        xcopy /hrky "%IMX_WIN_ENTERPRISE_IMAGE%" "%WIN_PE_DST_DIR%\*"
    )
)
echo ---------------------------------------------------------------------------------------------------------------------------------------------------------
echo *** Step 2.2 Copying Windows PE files from "%WIN_PE_SRC_DIR_ABS%" to "%WIN_PE_DST_DIR%"
echo ---------------------------------------------------------------------------------------------------------------------------------------------------------
echo:
if not exist "%WIN_PE_SRC_DIR_ABS%\Media" (
    echo You must install the Windows PE Add-on for the ADK
    echo https://docs.microsoft.com/en-us/windows-hardware/manufacture/desktop/download-winpe--windows-pe
    goto err
)

xcopy /herky "%WIN_PE_SRC_DIR_ABS%\Media\*"                             "%WIN_PE_DST_DIR%\"                    || goto err
xcopy /hrky  "%WIN_PE_SRC_DIR_ABS%\Media\bootmgr.efi"                   "%WIN_PE_DST_DIR%\EFI\Microsoft\Boot\" || goto err
xcopy /hrky  "%WIN_PE_SRC_DIR_ABS%\en-us\winpe.wim"                     "%WIN_PE_DST_DIR%\sources\boot.*"      || goto err
xcopy /hrky  "%SCRIPT_DIR%\scripts\diskpart_win_iot_enterprise.txt" "%WIN_PE_DST_DIR%"                     || goto err
del /q "%WIN_PE_DST_DIR%\bootmgr.efi"                                                                      || goto err

:: BCD
if "%WINPE_DEBUG_SERIAL%" == "yes" (
    echo *** Step 2.2.1 Enabling kernel debugging for WinPE over serial, store: "%WIN_PE_DST_DIR%\EFI\Microsoft\boot\bcd"
    bcdedit /store "%WIN_PE_DST_DIR%\EFI\Microsoft\boot\bcd" /dbgsettings SERIAL DEBUGPORT:1 BAUDRATE:921600 || goto err
    bcdedit /store "%WIN_PE_DST_DIR%\EFI\Microsoft\boot\bcd" /debug {default} on                             || goto err
)

if "%WINPE_DEBUG_NET%" == "yes" (
    echo *** Step 2.2.2 Enabling kernel debugging for WinPE over net, store: "%WIN_PE_DST_DIR%\EFI\Microsoft\boot\bcd"
    bcdedit /store "%WIN_PE_DST_DIR%\EFI\Microsoft\boot\bcd" /dbgsettings net hostip:%DEBUG_IP% port:50085 key:a.b.c.d || (echo Enabling WinPE debug failed. Check whether IP for debugging is valid. & goto err)
    bcdedit /store "%WIN_PE_DST_DIR%\EFI\Microsoft\boot\bcd" /debug {default} on                             || goto err
)

if "%TEST_SIGNING%" == "yes" (
    echo *** Step 2.2.3 Enabling boot test/flight signing for WinPE, store: "%WIN_PE_DST_DIR%\EFI\Microsoft\boot\bcd"
    bcdedit /store "%WIN_PE_DST_DIR%\EFI\Microsoft\boot\bcd" /set {bootmgr} flightsigning on || goto err
    bcdedit /store "%WIN_PE_DST_DIR%\EFI\Microsoft\boot\bcd" /set {bootmgr} testsigning on   || goto err
    
    echo *** Step 2.2.4 Enabling kernel test/flight signing for WinPE, store: "%WIN_PE_DST_DIR%\EFI\Microsoft\boot\bcd"
    bcdedit /store "%WIN_PE_DST_DIR%\EFI\Microsoft\boot\bcd" /set {default} testsigning on   || goto err
    bcdedit /store "%WIN_PE_DST_DIR%\EFI\Microsoft\boot\bcd" /set {default} flightsigning on || goto err
)
echo:
echo ---------------------------------------------------------------------------------------------------------------------------------------------------------
echo *** Step 2.3 Mounting i.MX Windows PE image at "%WIN_PE_MOUNT_DIR%"
echo ---------------------------------------------------------------------------------------------------------------------------------------------------------
:: make EinPe mount directory
mkdir "%WIN_PE_MOUNT_DIR%"
echo dism /mount-wim /wimfile:"%WIN_PE_DST_DIR%\sources\boot.wim" /mountdir:"%WIN_PE_MOUNT_DIR%" /index:1 || goto err
dism /mount-wim /wimfile:"%WIN_PE_DST_DIR%\sources\boot.wim" /mountdir:"%WIN_PE_MOUNT_DIR%" /index:1 || goto err
echo:
echo ---------------------------------------------------------------------------------------------------------------------------------------------------------
echo *** Step 2.4 Injecting drivers from "%DRIVER_DIR%" into i.MX Windows PE image
echo ---------------------------------------------------------------------------------------------------------------------------------------------------------
echo dism /Image:"%WIN_PE_MOUNT_DIR%" /Add-Driver /Driver:"%DRIVER_DIR%" /Recurse !DISM_UNSIGNED_ARG!
dism /Image:"%WIN_PE_MOUNT_DIR%" /Add-Driver /Driver:"%DRIVER_DIR%" /Recurse !DISM_UNSIGNED_ARG! || goto err
echo:

if %NO_PATCH_PE% == no (
    echo ---------------------------------------------------------------------------------------------------------------------------------------------------------
    echo *** Step 2.45 Applying patches to i.MX Windows PE image
    echo ---------------------------------------------------------------------------------------------------------------------------------------------------------
    for /f "tokens=*" %%i in ('dir /b %KB_PATCH_DIR%\*.msu') do (
        echo Applying cumulative update %%i from %KB_PATCH_DIR%...
        call :clrEcho 02 "WARNING Depending on size of the update, this processs may take up to 30 minutes, do not stop the process even if it may appear to be frozen"
        echo:
        echo dism /Image:"%WIN_PE_MOUNT_DIR%" /Add-Package /PackagePath="%KB_PATCH_DIR%\%%i"
        dism /Image:"%WIN_PE_MOUNT_DIR%" /Add-Package /PackagePath="%KB_PATCH_DIR%\%%i" || goto ErrExit
    )
)

echo ---------------------------------------------------------------------------------------------------------------------------------------------------------
echo *** Step 2.5 Creating the WinPE startnet.cmd script "%WIN_ENTERPRISE_INSTALL_CMD%"
echo ---------------------------------------------------------------------------------------------------------------------------------------------------------
echo:
echo echo Setting up drive letter %WINPE_DRIVE_LETTER% for WinPE >> "%WIN_PE_STARTNET_CMD%"
echo ^( echo sel disk 1 >> "%WIN_PE_STARTNET_CMD%"
echo echo sel partition 2 >> "%WIN_PE_STARTNET_CMD%"
echo echo assign letter=%WINPE_DRIVE_LETTER% ^) ^| diskpart >> "%WIN_PE_STARTNET_CMD%"
echo if not exist %WINPE_DRIVE_LETTER%:\install.cmd.txt ^( echo^: ^& echo install.cmd.txt not found. Check the installation SD card and make sure it is the only storage device connected to the board. ^& exit /b 1^) >> "%WIN_PE_STARTNET_CMD%"
echo copy %WINPE_DRIVE_LETTER%:\install.cmd.txt %WINPE_DRIVE_LETTER%:\install.cmd >> "%WIN_PE_STARTNET_CMD%"
echo call %WINPE_DRIVE_LETTER%:\install.cmd >> "%WIN_PE_STARTNET_CMD%"
echo del /f /q %WINPE_DRIVE_LETTER%:\install.cmd  >> "%WIN_PE_STARTNET_CMD%"
echo wpeutil reboot >> "%WIN_PE_STARTNET_CMD%"


echo:
echo ---------------------------------------------------------------------------------------------------------------------------------------------------------
echo *** Step 2.6 Applying cumulative update %CUMULATIVE_UPDATE_PATH%
echo ---------------------------------------------------------------------------------------------------------------------------------------------------------
echo:
if not "%CUMULATIVE_UPDATE_PATH%" == "" (
	echo Installing updates from "%CUMULATIVE_UPDATE_PATH%" into Enterprise WIM
	dism /Image:"%WIN_PE_MOUNT_DIR%" /Add-Package /PackagePath="%CUMULATIVE_UPDATE_PATH%" || goto err
) else (
    echo No update found.
)

echo:
echo copy "%WIN_PE_STARTNET_CMD%" "%WIN_PE_MOUNT_DIR%\Windows\System32\startnet.cmd"
copy "%WIN_PE_STARTNET_CMD%" "%WIN_PE_MOUNT_DIR%\Windows\System32\startnet.cmd"
echo del /f /q "%WIN_PE_STARTNET_CMD%"
del /f /q "%WIN_PE_STARTNET_CMD%"

if "%WINPE_DEBUG_NET%" == "yes" (
    if not "!KD_NET!" == "no" (
        echo:
        echo Copying kd_8003_1fc9.dll into the i.MX Windows PE image
        echo copy /Y /R /F "%SCRIPT_DIR%\!KD_NET!" "%WIN_PE_MOUNT_DIR%\Windows\system32\kd_8003_1fc9.dll" || goto ErrExit
        copy "%SCRIPT_DIR%\!KD_NET!" "%WIN_PE_MOUNT_DIR%\Windows\system32\kd_8003_1fc9.dll" || goto ErrExit
    )
)

echo echo Partitioning the drive >> "%WIN_ENTERPRISE_INSTALL_CMD%"
echo diskpart /s %WINPE_DRIVE_LETTER%:\diskpart_win_iot_enterprise.txt >> "%WIN_ENTERPRISE_INSTALL_CMD%"

echo echo Deploying IoT Enterprise to eMMC >> "%WIN_ENTERPRISE_INSTALL_CMD%"

if "!SPLIT_WIM!" == "yes" (
    echo dism /Apply-Image /ImageFile:"%WINPE_DRIVE_LETTER%:\install.swm" /SWMFile:"%WINPE_DRIVE_LETTER%:\install*.swm" /Index:2 /ApplyDir:W:\ /Compact >> "%WIN_ENTERPRISE_INSTALL_CMD%"
) else (
    echo dism /Apply-Image /ImageFile:"%WINPE_DRIVE_LETTER%:\%IMX_WIN_ENTERPRISE_IMAGE_FILE_NAME%" /Index:2 /ApplyDir:W:\ /Compact >> "%WIN_ENTERPRISE_INSTALL_CMD%"
)

echo echo Setting up BCD >> "%WIN_ENTERPRISE_INSTALL_CMD%"
echo W:\Windows\System32\bcdboot.exe W:\Windows /s S: >> "%WIN_ENTERPRISE_INSTALL_CMD%"
if "%TEST_SIGNING%" == "yes" (
    echo W:\Windows\System32\bcdedit.exe /store S:\EFI\microsoft\Boot\BCD /set {default} testsigning on >> "%WIN_ENTERPRISE_INSTALL_CMD%"
)

if "%WIN_DEBUG_SERIAL%" == "yes" (
    echo echo Enabling kernel debugging for Windows over serial >> "%WIN_ENTERPRISE_INSTALL_CMD%"
    echo W:\Windows\System32\bcdedit.exe /store S:\EFI\microsoft\Boot\BCD /dbgsettings SERIAL DEBUGPORT:1 BAUDRATE:921600 >> "%WIN_ENTERPRISE_INSTALL_CMD%"
    echo W:\Windows\System32\bcdedit.exe /store S:\EFI\microsoft\Boot\BCD /debug {default} on >> "%WIN_ENTERPRISE_INSTALL_CMD%"
)

if "%WIN_DEBUG_NET%" == "yes" (
    echo echo Enabling kernel debugging for Windows IoT Enterprise over net>> "%WIN_ENTERPRISE_INSTALL_CMD%"
    echo W:\Windows\System32\bcdedit.exe /store S:\EFI\microsoft\Boot\BCD /debug {default} on >> "%WIN_ENTERPRISE_INSTALL_CMD%"
    echo W:\Windows\System32\bcdedit.exe /store S:\EFI\microsoft\Boot\BCD /dbgsettings net hostip:%debug_ip% port:50085 key:a.b.c.d >> "%WIN_ENTERPRISE_INSTALL_CMD%"
)

echo echo Disabling Fast Startup >> "%WIN_ENTERPRISE_INSTALL_CMD%"
echo REG LOAD HKLM\OFFLINE_SYSTEM W:\Windows\System32\config\SYSTEM >> "%WIN_ENTERPRISE_INSTALL_CMD%"
echo REG ADD "HKLM\OFFLINE_SYSTEM\ControlSet001\Control\Session Manager\Power" /V HiberbootEnabled /T REG_DWORD /D 0 /F >> "%WIN_ENTERPRISE_INSTALL_CMD%"
echo REG UNLOAD HKLM\OFFLINE_SYSTEM >> "%WIN_ENTERPRISE_INSTALL_CMD%"

:: HKLM\OFFLINE_SOFTWARE registry update section
echo REG LOAD HKLM\OFFLINE_SOFTWARE W:\Windows\System32\config\SOFTWARE >> "%WIN_ENTERPRISE_INSTALL_CMD%"

echo echo Show Sleep button >> "%WIN_ENTERPRISE_INSTALL_CMD%"
echo REG ADD "HKLM\OFFLINE_SOFTWARE\Microsoft\Windows\CurrentVersion\Explorer\FlyoutMenuSettings" /V ShowSleepOption /T REG_DWORD /D 1 /F >> "%WIN_ENTERPRISE_INSTALL_CMD%"

REM ****************************
REM S - VPU driver configuration
REM ****************************
REM VPU driver definitions
echo set VPU_HANTRO_FF_STR="NXP0109\1" >> "%WIN_ENTERPRISE_INSTALL_CMD%"
echo set VPU_HANTRO_LF_STR="NXP0109\2" >> "%WIN_ENTERPRISE_INSTALL_CMD%"
echo set VPU_MALONE_STR="NXP0224\0" >> "%WIN_ENTERPRISE_INSTALL_CMD%"
echo set VPU_HANTRO_FF_ID=1 >> "%WIN_ENTERPRISE_INSTALL_CMD%"
echo set VPU_HANTRO_LF_ID=2 >> "%WIN_ENTERPRISE_INSTALL_CMD%"
echo set VPU_MALONE_ID=3 >> "%WIN_ENTERPRISE_INSTALL_CMD%"
echo set VPU_NONE=0 >> "%WIN_ENTERPRISE_INSTALL_CMD%"
echo set VPU_HANTRO=false >> "%WIN_ENTERPRISE_INSTALL_CMD%"
echo set VPU_MALONE=false >> "%WIN_ENTERPRISE_INSTALL_CMD%"
echo set vpu_variant=%%VPU_NONE%% >> "%WIN_ENTERPRISE_INSTALL_CMD%"
REM ******************
REM VPU find compatible HW
REM ******************
echo pnputil /enum-devices ^| find %%VPU_HANTRO_FF_STR%% ^> nul >> "%WIN_ENTERPRISE_INSTALL_CMD%"
echo if %%errorlevel%% == 0 ^( set vpu_variant=%%VPU_HANTRO_FF_ID%% ^& goto vpu_found ^) >> "%WIN_ENTERPRISE_INSTALL_CMD%"
echo pnputil /enum-devices ^| find %%VPU_HANTRO_LF_STR%% ^> nul >> "%WIN_ENTERPRISE_INSTALL_CMD%"
echo if %%errorlevel%% == 0 ^( set vpu_variant=%%VPU_HANTRO_LF_ID%% ^& goto vpu_found ^) >> "%WIN_ENTERPRISE_INSTALL_CMD%"
echo pnputil /enum-devices ^| find %%VPU_MALONE_STR%% ^> nul >> "%WIN_ENTERPRISE_INSTALL_CMD%"
echo if %%errorlevel%% == 0 ^( set vpu_variant=%%VPU_MALONE_ID%% ^& goto vpu_found ^) >> "%WIN_ENTERPRISE_INSTALL_CMD%"
REM ******************
REM VPU configuration
REM ******************
echo :vpu_found >> "%WIN_ENTERPRISE_INSTALL_CMD%"
echo if %%vpu_variant%% == %%VPU_HANTRO_FF_ID%% set VPU_HANTRO=true >> "%WIN_ENTERPRISE_INSTALL_CMD%"
echo if %%vpu_variant%% == %%VPU_HANTRO_LF_ID%% set VPU_HANTRO=true >> "%WIN_ENTERPRISE_INSTALL_CMD%"
echo if %%vpu_variant%% == %%VPU_MALONE_ID%% set VPU_MALONE=true >> "%WIN_ENTERPRISE_INSTALL_CMD%"
REM ******************
REM *** VPU MALONE ***
REM ******************
echo if %%VPU_MALONE%% == true ^( >> "%WIN_ENTERPRISE_INSTALL_CMD%"
echo   REG ADD "HKLM\OFFLINE_SOFTWARE\Microsoft\Windows\CurrentVersion\DeviceAccess\Classes\{40d466e9-e5fd-45a1-8707-e5059211c021}" /V Policy /T REG_DWORD /D 1 /F >> "%WIN_ENTERPRISE_INSTALL_CMD%"
REM H.264
echo   REG ADD "HKLM\OFFLINE_SOFTWARE\Classes\MediaFoundation\Transforms\Preferred" /V {34363248-0000-0010-8000-00AA00389B71} /T REG_SZ /D {16977919-9727-4e04-ABD6-7D04C3828977} /F >> "%WIN_ENTERPRISE_INSTALL_CMD%"
REM H.265
echo   REG ADD "HKLM\OFFLINE_SOFTWARE\Classes\MediaFoundation\Transforms\Preferred" /V {35363248-0000-0010-8000-00AA00389B71} /T REG_SZ /D {16977919-9727-4e04-ABD6-7D04C3828977} /F >> "%WIN_ENTERPRISE_INSTALL_CMD%"
REM HEVC
echo   REG ADD "HKLM\OFFLINE_SOFTWARE\Classes\MediaFoundation\Transforms\Preferred" /V {43564548-0000-0010-8000-00AA00389B71} /T REG_SZ /D {16977919-9727-4e04-ABD6-7D04C3828977} /F >> "%WIN_ENTERPRISE_INSTALL_CMD%"
REM VP80
echo   REG ADD "HKLM\OFFLINE_SOFTWARE\Classes\MediaFoundation\Transforms\Preferred" /V {30385056-0000-0010-8000-00AA00389B71} /T REG_SZ /D {16977919-9727-4e04-ABD6-7D04C3828977} /F >> "%WIN_ENTERPRISE_INSTALL_CMD%"
REM MPEG-4 Simple Profile
echo   REG ADD "HKLM\OFFLINE_SOFTWARE\Classes\MediaFoundation\Transforms\Preferred" /V {5334504D-0000-0010-8000-00AA00389B71} /T REG_SZ /D {16977919-9727-4e04-ABD6-7D04C3828977} /F >> "%WIN_ENTERPRISE_INSTALL_CMD%"
REM MPEG-4 ASP
echo   REG ADD "HKLM\OFFLINE_SOFTWARE\Classes\MediaFoundation\Transforms\Preferred" /V {3253344D-0000-0010-8000-00AA00389B71} /T REG_SZ /D {16977919-9727-4e04-ABD6-7D04C3828977} /F >> "%WIN_ENTERPRISE_INSTALL_CMD%"
REM MPEG-4 Part 2
echo   REG ADD "HKLM\OFFLINE_SOFTWARE\Classes\MediaFoundation\Transforms\Preferred" /V {5634504D-0000-0010-8000-00AA00389B71} /T REG_SZ /D {16977919-9727-4e04-ABD6-7D04C3828977} /F >> "%WIN_ENTERPRISE_INSTALL_CMD%"
REM MPEG-2
echo   REG ADD "HKLM\OFFLINE_SOFTWARE\Classes\MediaFoundation\Transforms\Preferred" /V {E06D8026-DB46-11CF-B4D1-00805F6CBBEA} /T REG_SZ /D {16977919-9727-4e04-ABD6-7D04C3828977} /F >> "%WIN_ENTERPRISE_INSTALL_CMD%"
echo   REG ADD "HKLM\OFFLINE_SOFTWARE\Classes\MediaFoundation\Transforms\62ce7e72-4c71-4d20-b15d-452831a87d9d" /V MFTFLAGS /T REG_DWORD /D 00000008 /F >> "%WIN_ENTERPRISE_INSTALL_CMD%"
REM register MaloneMFT.dll
echo   REG ADD "HKLM\OFFLINE_SOFTWARE\Classes\CLSID\{16977919-9727-4e04-ABD6-7D04C3828977}" /D "i.MX8QM/QXP Malone VPU MFT hardware accelerator" /F >> "%WIN_ENTERPRISE_INSTALL_CMD%"
echo   REG ADD "HKLM\OFFLINE_SOFTWARE\Classes\CLSID\{16977919-9727-4e04-ABD6-7D04C3828977}\InprocServer32" /T REG_EXPAND_SZ /D "%%SystemRoot%%\System32\malonemft.dll" /F >> "%WIN_ENTERPRISE_INSTALL_CMD%"
echo   REG ADD "HKLM\OFFLINE_SOFTWARE\Classes\CLSID\{16977919-9727-4e04-ABD6-7D04C3828977}\InprocServer32" /V ThreadingModel /D "Both" /F >> "%WIN_ENTERPRISE_INSTALL_CMD%"
echo   REG ADD "HKLM\OFFLINE_SOFTWARE\Classes\MediaFoundation\Transforms\16977919-9727-4e04-ABD6-7D04C3828977" /D "i.MX8QM/QXP Malone VPU MFT hardware accelerator" /F >> "%WIN_ENTERPRISE_INSTALL_CMD%"
REM H264, H265, VP80,  MPEG-4 SP/ASP/Part2, MPEG-2 codecs are registered currently
echo   REG ADD "HKLM\OFFLINE_SOFTWARE\Classes\MediaFoundation\Transforms\16977919-9727-4e04-ABD6-7D04C3828977" /V InputTypes /T REG_BINARY /D 7669647300001000800000AA00389B714832363400001000800000AA00389B717669647300001000800000AA00389B714832363500001000800000AA00389B717669647300001000800000AA00389B714845564300001000800000AA00389B717669647300001000800000AA00389B715650383000001000800000AA00389B717669647300001000800000AA00389B7126806DE046DBCF11B4D100805F6CBBEA7669647300001000800000AA00389B714D50345300001000800000AA00389B717669647300001000800000AA00389B714D34533200001000800000AA00389B717669647300001000800000AA00389B714D50345600001000800000AA00389B71 /F >> "%WIN_ENTERPRISE_INSTALL_CMD%"
REM RGBA32 output is registered currently
echo   REG ADD "HKLM\OFFLINE_SOFTWARE\Classes\MediaFoundation\Transforms\16977919-9727-4e04-ABD6-7D04C3828977" /V OutputTypes /T REG_BINARY /D 7669647300001000800000AA00389B711500000000001000800000AA00389B71 /F >> "%WIN_ENTERPRISE_INSTALL_CMD%"echo   REG ADD "HKLM\OFFLINE_SOFTWARE\Classes\MediaFoundation\Transforms\16977919-9727-4e04-ABD6-7D04C3828977" /V MFTFlags /T REG_DWORD /D 0x00000002 /F >> "%WIN_ENTERPRISE_INSTALL_CMD%"
echo   REG ADD "HKLM\OFFLINE_SOFTWARE\Classes\MediaFoundation\Transforms\16977919-9727-4e04-ABD6-7D04C3828977" /V MFTFlags /T REG_DWORD /D 0x00000002 /F >> "%WIN_ENTERPRISE_INSTALL_CMD%"
echo   REG ADD "HKLM\OFFLINE_SOFTWARE\Classes\MediaFoundation\Transforms\Categories\d6c02d4b-6833-45b4-971a-05a4b04bab91\16977919-9727-4e04-ABD6-7D04C3828977" /F >> "%WIN_ENTERPRISE_INSTALL_CMD%"
echo ^) >> "%WIN_ENTERPRISE_INSTALL_CMD%"
REM ******************
REM *** VPU HANTRO ***
REM ******************
echo if %%VPU_HANTRO%% == true ^( >> "%WIN_ENTERPRISE_INSTALL_CMD%"
echo   REG ADD "HKLM\OFFLINE_SOFTWARE\Microsoft\Windows\CurrentVersion\DeviceAccess\Classes\{ADA9253B-628C-40CE-B2C1-19F489A0F3DA}" /V Policy /T REG_DWORD /D 1 /F >> "%WIN_ENTERPRISE_INSTALL_CMD%"
REM H.264
echo   REG ADD "HKLM\OFFLINE_SOFTWARE\Classes\MediaFoundation\Transforms\Preferred" /V {34363248-0000-0010-8000-00AA00389B71} /T REG_SZ /D {8A12D5A9-69EC-4FE2-BF16-7B4C857D0DC0} /F >> "%WIN_ENTERPRISE_INSTALL_CMD%"
REM H.265
echo   REG ADD "HKLM\OFFLINE_SOFTWARE\Classes\MediaFoundation\Transforms\Preferred" /V {35363248-0000-0010-8000-00AA00389B71} /T REG_SZ /D {8A12D5A9-69EC-4FE2-BF16-7B4C857D0DC0} /F >> "%WIN_ENTERPRISE_INSTALL_CMD%"
REM HEVC
echo   REG ADD "HKLM\OFFLINE_SOFTWARE\Classes\MediaFoundation\Transforms\Preferred" /V {43564548-0000-0010-8000-00AA00389B71} /T REG_SZ /D {8A12D5A9-69EC-4FE2-BF16-7B4C857D0DC0} /F >> "%WIN_ENTERPRISE_INSTALL_CMD%"
REM VP80
echo   REG ADD "HKLM\OFFLINE_SOFTWARE\Classes\MediaFoundation\Transforms\Preferred" /V {30385056-0000-0010-8000-00AA00389B71} /T REG_SZ /D {8A12D5A9-69EC-4FE2-BF16-7B4C857D0DC0} /F >> "%WIN_ENTERPRISE_INSTALL_CMD%"
REM VP90
echo   REG ADD "HKLM\OFFLINE_SOFTWARE\Classes\MediaFoundation\Transforms\Preferred" /V {30395056-0000-0010-8000-00AA00389B71} /T REG_SZ /D {8A12D5A9-69EC-4FE2-BF16-7B4C857D0DC0} /F >> "%WIN_ENTERPRISE_INSTALL_CMD%"
echo   REG ADD "HKLM\OFFLINE_SOFTWARE\Classes\MediaFoundation\Transforms\62ce7e72-4c71-4d20-b15d-452831a87d9d" /V MFTFLAGS /T REG_DWORD /D 00000008 /F >> "%WIN_ENTERPRISE_INSTALL_CMD%"
REM register MFTDLL
echo   REG ADD "HKLM\OFFLINE_SOFTWARE\Classes\CLSID\{8A12D5A9-69EC-4FE2-BF16-7B4C857D0DC0}" /D "i.MX VPU MFT hardware accelerator" /F >> "%WIN_ENTERPRISE_INSTALL_CMD%"
echo   REG ADD "HKLM\OFFLINE_SOFTWARE\Classes\CLSID\{8A12D5A9-69EC-4FE2-BF16-7B4C857D0DC0}\InprocServer32" /T REG_EXPAND_SZ /D "%%SystemRoot%%\System32\imxvpumft.dll" /F >> "%WIN_ENTERPRISE_INSTALL_CMD%"
echo   REG ADD "HKLM\OFFLINE_SOFTWARE\Classes\CLSID\{8A12D5A9-69EC-4FE2-BF16-7B4C857D0DC0}\InprocServer32" /V ThreadingModel /D "Both" /F >> "%WIN_ENTERPRISE_INSTALL_CMD%"
echo   REG ADD "HKLM\OFFLINE_SOFTWARE\Classes\MediaFoundation\Transforms\8a12d5a9-69ec-4fe2-bf16-7b4c857d0dc0" /D "i.MX VPU MFT hardware accelerator" /F >> "%WIN_ENTERPRISE_INSTALL_CMD%"
echo   REG ADD "HKLM\OFFLINE_SOFTWARE\Classes\MediaFoundation\Transforms\8a12d5a9-69ec-4fe2-bf16-7b4c857d0dc0" /V OutputTypes /T REG_BINARY /D 7669647300001000800000AA00389B714E56313200001000800000AA00389B71 /F >> "%WIN_ENTERPRISE_INSTALL_CMD%"
echo   REG ADD "HKLM\OFFLINE_SOFTWARE\Classes\MediaFoundation\Transforms\8a12d5a9-69ec-4fe2-bf16-7b4c857d0dc0" /V MFTFlags /T REG_DWORD /D 0x00000002 /F >> "%WIN_ENTERPRISE_INSTALL_CMD%"
echo   REG ADD "HKLM\OFFLINE_SOFTWARE\Classes\MediaFoundation\Transforms\Categories\d6c02d4b-6833-45b4-971a-05a4b04bab91\8a12d5a9-69ec-4fe2-bf16-7b4c857d0dc0" /F >> "%WIN_ENTERPRISE_INSTALL_CMD%"
echo ^) >> "%WIN_ENTERPRISE_INSTALL_CMD%"
echo if %%vpu_variant%% == %%VPU_HANTRO_FF_ID%% ^( >> "%WIN_ENTERPRISE_INSTALL_CMD%"
REM MPEG-4 Simple Profile
echo   REG ADD "HKLM\OFFLINE_SOFTWARE\Classes\MediaFoundation\Transforms\Preferred" /V {5334504D-0000-0010-8000-00AA00389B71} /T REG_SZ /D {8A12D5A9-69EC-4FE2-BF16-7B4C857D0DC0} /F >> "%WIN_ENTERPRISE_INSTALL_CMD%"
REM MPEG-4 ASP
echo   REG ADD "HKLM\OFFLINE_SOFTWARE\Classes\MediaFoundation\Transforms\Preferred" /V {3253344D-0000-0010-8000-00AA00389B71} /T REG_SZ /D {8A12D5A9-69EC-4FE2-BF16-7B4C857D0DC0} /F >> "%WIN_ENTERPRISE_INSTALL_CMD%"
REM MPEG-4 Part 2
echo   REG ADD "HKLM\OFFLINE_SOFTWARE\Classes\MediaFoundation\Transforms\Preferred" /V {5634504D-0000-0010-8000-00AA00389B71} /T REG_SZ /D {8A12D5A9-69EC-4FE2-BF16-7B4C857D0DC0} /F >> "%WIN_ENTERPRISE_INSTALL_CMD%"
REM MPEG-2
echo   REG ADD "HKLM\OFFLINE_SOFTWARE\Classes\MediaFoundation\Transforms\Preferred" /V {E06D8026-DB46-11CF-B4D1-00805F6CBBEA} /T REG_SZ /D {8A12D5A9-69EC-4FE2-BF16-7B4C857D0DC0} /F >> "%WIN_ENTERPRISE_INSTALL_CMD%"
REM register MFTDLL
echo   REG ADD "HKLM\OFFLINE_SOFTWARE\Classes\MediaFoundation\Transforms\8a12d5a9-69ec-4fe2-bf16-7b4c857d0dc0" /V InputTypes /T REG_BINARY /D 7669647300001000800000AA00389B714832363400001000800000AA00389B717669647300001000800000AA00389B714832363500001000800000AA00389B717669647300001000800000AA00389B714845564300001000800000AA00389B717669647300001000800000AA00389B715650383000001000800000AA00389B717669647300001000800000AA00389B715650393000001000800000AA00389B717669647300001000800000AA00389B7126806DE046DBCF11B4D100805F6CBBEA7669647300001000800000AA00389B714D50345300001000800000AA00389B717669647300001000800000AA00389B714D34533200001000800000AA00389B717669647300001000800000AA00389B714D50345600001000800000AA00389B71 /F >> "%WIN_ENTERPRISE_INSTALL_CMD%"
echo ^) >> "%WIN_ENTERPRISE_INSTALL_CMD%"
echo if %%vpu_variant%% == %%VPU_HANTRO_LF_ID%% ^( >> "%WIN_ENTERPRISE_INSTALL_CMD%"
echo   REG ADD "HKLM\OFFLINE_SOFTWARE\Classes\MediaFoundation\Transforms\8a12d5a9-69ec-4fe2-bf16-7b4c857d0dc0" /V InputTypes /T REG_BINARY /D 7669647300001000800000AA00389B714832363400001000800000AA00389B717669647300001000800000AA00389B714832363500001000800000AA00389B717669647300001000800000AA00389B714845564300001000800000AA00389B717669647300001000800000AA00389B715650383000001000800000AA00389B717669647300001000800000AA00389B715650393000001000800000AA00389B71 /F >> "%WIN_ENTERPRISE_INSTALL_CMD%"
echo ^) >> "%WIN_ENTERPRISE_INSTALL_CMD%"
REM ****************************
REM E - VPU driver configuration
REM ****************************
echo echo Remove OneDrive >>  "%WIN_ENTERPRISE_INSTALL_CMD%"
echo del /f /q W:\windows\SysWOW64\OneDriveSetup.exe >>  "%WIN_ENTERPRISE_INSTALL_CMD%"

if "%DISABLE_UPDATES%" == "yes" (
    echo echo Disabling automatic updates >> "%WIN_ENTERPRISE_INSTALL_CMD%"
    echo REG ADD "HKLM\OFFLINE_SOFTWARE\Policies\Microsoft\Windows\WindowsUpdate\AU " /V NoAutoUpdate /T REG_DWORD /D 1 /F >> "%WIN_ENTERPRISE_INSTALL_CMD%"
)

echo REG UNLOAD HKLM\OFFLINE_SOFTWARE >> "%WIN_ENTERPRISE_INSTALL_CMD%"

if "%DISABLE_TRANSPARENCY%" == "yes" (
    echo echo Disabling transparency >> "%WIN_ENTERPRISE_INSTALL_CMD%"
    echo REG LOAD HKLM\TEMP W:\Users\Default\NTUSER.DAT >> "%WIN_ENTERPRISE_INSTALL_CMD%"
    echo REG ADD "HKLM\TEMP\Software\Microsoft\windows\CurrentVersion\Themes\Personalize" /V EnableTransparency /T REG_DWORD /D 0 /F >> "%WIN_ENTERPRISE_INSTALL_CMD%"
    echo REG UNLOAD HKLM\TEMP >> "%WIN_ENTERPRISE_INSTALL_CMD%"
)

echo echo Renaming the WinPE EFI partition >> "%WIN_ENTERPRISE_INSTALL_CMD%"
echo ren %WINPE_DRIVE_LETTER%:\efi _efi >> "%WIN_ENTERPRISE_INSTALL_CMD%"

echo echo Successfully flashed new image to eMMC. Restarting the device... >> "%WIN_ENTERPRISE_INSTALL_CMD%"
echo timeout /t 10 >> "%WIN_ENTERPRISE_INSTALL_CMD%"


echo:
echo ---------------------------------------------------------------------------------------------------------------------------------------------------------
echo *** Step 2.7 Unmounting i.MX Windows PE image from: "%WIN_PE_MOUNT_DIR%"
echo ---------------------------------------------------------------------------------------------------------------------------------------------------------
dism /unmount-wim /mountdir:"%WIN_PE_MOUNT_DIR%" /commit || goto err
rmdir /s /q "%WIN_PE_MOUNT_DIR%" 2> NUL
echo:
echo *********************************************************************************************************************************************************
echo *********************************************************************************************************************************************************
echo *** Step 2 DONE. i.MX Windows PE layout succesfully created at "%WIN_PE_DST_DIR%".
echo *********************************************************************************************************************************************************
echo *********************************************************************************************************************************************************


exit /b 0

:APPLY

    REM Check size of selected drive and determine whether it is an SD card to protect hard-drives from overwriting
    REM save size of selected disk in bytes into DISKSIZE
    for /f "usebackq skip=1 tokens=*" %%i in (`wmic diskdrive %DISK_NUM% get size ^| findstr /r /v "^$"`) do set DISKSIZE=%%i
    
    if "%DISKSIZE%" == "" (
        echo Selected disk^(%DISK_NUM%^) is invalid. Select a valid disk using option /apply.
        exit /b 1
    )

    REM remove spaces from DISKSIZE
    set DISKSIZE=%DISKSIZE: =%
    REM convert DISKSIZE fro B to GB
    set /A GB=%DISKSIZE:~0,-3%/1024/1024
    REM Check the size of selected drive, if larger than 64GB, it's probably a hard drive, so the script will end
    REM conversion is not 100% precise due to limitations of batch scripts, so comparing to 65 instead of 64
    if !GB! GEQ 65 (
        echo Selected disk is too big. Make sure to choose a SD card as this operation would clear the selected disk.
        exit /b 1
    )

    if %DISK_NUM% == 0 (
        echo Disk number is zero.
        exit /b 1    
    )
    echo Applying image at "%WIN_PE_DST_DIR%" to physical disk %DISK_NUM%
    if not exist "%WIN_PE_DST_DIR%" (
        echo No WinPE media directory found at %WIN_PE_DST_DIR%. Run the first form of this script to prepare WinPE image layout.
        exit /b 1
    )

    echo select disk %DISK_NUM% > diskpart.txt
    echo clean >> diskpart.txt
    echo convert gpt NOERR >> diskpart.txt

    :: EFI partition
    echo create partition primary id=c12a7328-f81f-11d2-ba4b-00a0c93ec93b size=32 align=8192 >> diskpart.txt
    echo create partition primary >> diskpart.txt
    echo format fs=fat32 label="WinPE" quick >> diskpart.txt
    echo assign >> diskpart.txt
    echo assign mount="%CD%\%WIN_PE_MOUNT_DIR%" >> diskpart.txt

    mkdir "%WIN_PE_MOUNT_DIR%"

    echo Formatting disk %DISK_NUM% and mounting to %WIN_PE_MOUNT_DIR%...
    diskpart /s diskpart.txt || exit /b 1

    echo Copying files from %WIN_PE_DST_DIR% to %WIN_PE_MOUNT_DIR%
    xcopy /herky "%WIN_PE_DST_DIR%\*.*" "%WIN_PE_MOUNT_DIR%\" || exit /b 1

    mountvol "%WIN_PE_MOUNT_DIR%" /d
    timeout /t 10 /nobreak

    echo Success
    exit /b 0

:CLEAN
    echo Cleaning up from previous run
    if exist "%IMX_WIN_ENTRPRISE_MOUNT_DIR%" (    
        dism /unmount-wim /mountdir:"%IMX_WIN_ENTRPRISE_MOUNT_DIR%" /discard
        rmdir /s /q "%IMX_WIN_ENTRPRISE_MOUNT_DIR%" 2> NUL
    )
    if exist "%WIN_PE_MOUNT_DIR%" (    
        dism /unmount-wim /mountdir:"%WIN_PE_MOUNT_DIR%" /discard
        rmdir /s /q "%WIN_PE_MOUNT_DIR%" 2> NUL
        rmdir /s /q "%WIN_PE_DST_DIR%" 2> NUL
    )


    del diskpart.txt
    exit /b 0

:USAGE
    echo Usage:
    echo make-winpe.cmd
    echo make-winpe.cmd /apply disk_number
    echo make-winpe.cmd /clean
    echo.
    echo Creates a WinPE image for i.MX
    echo Options:
    echo    /source_image                    Pass the path to the image containing Windows IOT Enterprise. If not specified, current directory will be searched for all images. 
                                             By default, script works with install.wim manually extracted from an ISO file. Using option /iso, tou can pass ISO file to this option and the install.wim will be extracted automatically.
    echo    /iso                             Set ISO file as source of image with Windows IOT Enterprise. Script will automatically mount the ISO image and extract install.wim.
    echo    /winpedebug_serial               Optionally enable debugging over serial for WinPE.
    echo    /windebug_serial                 Optionally enable debugging over serial for Windows.
    echo    /winpedebug_net                  Optionally enable debugging over net for WinPE. Option /debug_ip is needed for specifying IP address of host machine. File kd_8003_1fc9.dll must be in same directory as this script.
    echo    /windebug_net                    Optionally enable debugging over net for Windows. Option /debug_ip is needed for specifying IP address of host machine. File kd_8003_1fc9.dll must be in same directory as this script.
    echo    /debug_ip                        Specify IP address of debugger machine. Necessary for options /winpedebug_net and /windebug_net.
    echo    /test_signing                    Optionally enable driver test signing.
    echo    /split_wim                       Split wim file for install.wim ^> 4GB. This option is no longer needed, as image split is enabled automatically for big images.
    echo    /no_patch_ent                    Disable application of updates from /kbpatch directory to the Windows enterprise image. Use this parameter when the image is already in desired version.
    echo    /no_patch_pe                     Disable application of updates from /kbpatch directory to the Windows PE image. Use this parameter when the image is already in desired version.
    echo    /no_patch                        Disable application of updates from /kbpatch directory to both Windows Enterprise image and Windows PE image.
    echo    /disable_updates                 Disable automatic updates to save disk space.
    echo    /enable_transparency             Enable window transparency. Window transparency is disabled by default to improve UI responsivness and stability.
    echo    /clean                           Clean up artifacts from a previous run.
    echo    /apply disk_number               Apply WinPE image to physical disk.
    echo    /?                               Help
    echo.
    echo Examples:
    echo.
    echo Create a WinPE image that deploys a Windows IOT Enterprise image to MMC.
    echo.
    echo    make-winpe.cmd /disable_updates
    echo.
    echo Apply the WinPE image to an SD card (Physical Disk 7, use diskpart
    echo to find the disk number)
    echo.
    echo    make-winpe.cmd /apply 7
    echo.
    echo Clean up artifacts from a previous run of this script
    echo.
    echo    make-winpe.cmd /clean
    echo.
    exit /b 0

:ErrExit
    exit /b 1
:PrErr
    echo ERROR: %ERR_MSG%
    echo use make-winpe.cmd /? for help
    exit /b 0

:err
    echo Script failed! Cleaning up
    dism /unmount-wim /mountdir:"%IMX_WIN_ENTRPRISE_MOUNT_DIR%" /discard
    dism /unmount-wim /mountdir:"%WIN_PE_MOUNT_DIR%" /discard
    rmdir /s /q "%WIN_PE_MOUNT_DIR%" 2> NUL
    rmdir /s /q "%WIN_PE_DST_DIR%" 2> NUL
    rmdir /s /q "%IMX_WIN_ENTRPRISE_MOUNT_DIR%" 2> NUL
    del diskpart.txt
    exit /b 1

:clrEcho @REM Method for coloring single line of text
    echo off
    <nul set /p ".=%DEL%" > "%~2"
    findstr /v /a:%1 /R "^$" "%~2" nul
    del "%~2" > nul 2>&1i
    exit /b 0