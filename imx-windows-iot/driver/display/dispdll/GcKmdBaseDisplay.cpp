/* Copyright (c) Microsoft Corporation.
 * Copyright 2022-2024 NXP
   Licensed under the MIT License. */

#include "precomp.h"

#include "GcKmdLogging.h"
#include "GcKmdBaseDisplay.tmh"

#include "GcKmdBaseDisplay.h"
#include "GcKmdGlobal.h"

#include "GcKmdUtil.h"
#include "GcKmdGuard.h"
#include "GcKmdErroHandling.h"
#include "getresrc.h"
#include "pwm.h"


GC_PAGED_SEGMENT_BEGIN; //======================================================

#define USE_PREVIOUS_POST_DISPLAY_INFO

#define printk(x, ...) DbgPrintEx(DPFLTR_IHVVIDEO_ID, DPFLTR_ERROR_LEVEL, x, __VA_ARGS__)

//#define MP_DISPLAY_DEBUG
#ifdef MP_DISPLAY_DEBUG
#define printk_debug printk
#else
#define printk_debug
#endif


extern BOOLEAN  g_bUsePreviousPostDisplayInfo;

NTSTATUS
GcKmBaseDisplay::Start(
    DXGKRNL_INTERFACE  *pDxgkInterface,
    UINT                LocalVidMemPhysicalBase,
    ULONG              *pNumberOfVideoPresentSources,
    ULONG              *pNumberOfChildren)
{
    if (RenderOnly == GcKmdGlobal::s_DriverMode)
    {
        return STATUS_SUCCESS;
    }

    m_pDxgkInterface = pDxgkInterface;
    m_LocalVidMemPhysicalBase = LocalVidMemPhysicalBase;

    //
    // GC 7000 Lite suppports 1 HDMI monitor
    //

    *pNumberOfVideoPresentSources = VIDEO_PRESENT_SOURCES_COUNT;
    *pNumberOfChildren = CHILD_COUNT;

    auto Status = SavePreviousPostDisplayInfo(pDxgkInterface);
    RETURN_ON_FAILURE(Status);

#ifndef SCAFFOLD_OFF
    //
    // DxgkCbAcquirePostDisplayOwnership only succeeds for POST adapter
    //

    if ((0 == m_PreviousPostDisplayInfo.Width))
    {
        m_PreviousPostDisplayInfo.Width = 0x500;
        m_PreviousPostDisplayInfo.Height = 0x2d0;
        m_PreviousPostDisplayInfo.Pitch = 0x1400;
        m_PreviousPostDisplayInfo.ColorFormat = D3DDDIFMT_X8R8G8B8;
        m_PreviousPostDisplayInfo.PhysicAddress.QuadPart = 0x40000000L;
        m_PreviousPostDisplayInfo.TargetId = 0xffffffff;
        m_PreviousPostDisplayInfo.AcpiId = 0x0;
    }
#endif

    NT_ASSERT(0 == m_PreviousPostDisplayInfo.PhysicAddress.HighPart);

    PHYSICAL_ADDRESS FbAddress;
    ULONG FbSize;
    ULONG PreviousPostDisplaySize =
        m_PreviousPostDisplayInfo.Pitch * m_PreviousPostDisplayInfo.Height;

    // Try read FB address and size from ACPI memory resources
    Status = GetFramebufferInfo(pDxgkInterface, &FbAddress, &FbSize);

    if (!NT_SUCCESS(Status) || FbAddress.QuadPart == 0 ||
            FbSize < PreviousPostDisplaySize)
    {
        // Reading FB address and size from ACPI resource failed,
        // use PreviousPostDisplayInfo values instead.
        Status = MapFrameBuffer(&m_FrameBuffer,
            m_PreviousPostDisplayInfo.PhysicAddress,
            PreviousPostDisplaySize);
        m_FbPhysicalAddr = m_PreviousPostDisplayInfo.PhysicAddress;
        m_FbSize = PreviousPostDisplaySize;
    }
    else
    {
        // Map whole area obtained from ACPI resource
        Status = MapFrameBuffer(&m_FrameBuffer, FbAddress, FbSize);
        m_FbPhysicalAddr = FbAddress;
        m_FbSize = FbSize;
    }

    // Full driver: Prepare the FB to flip to when "powering off" monitor.
    // Display only: Clean the FB to avoid boot image corruption
    // when setting different resolution than firmware.
    RtlZeroMemory(m_FrameBuffer.m_Address, m_FrameBuffer.m_Size);

    if (DisplayOnly == GcKmdGlobal::s_DriverMode)
    {
        RETURN_ON_FAILURE(Status);
    }
    else if (NT_SUCCESS(Status))
    {
        UnmapFrameBuffer(&m_FrameBuffer);
    }

   return HwStart(pDxgkInterface);
}

NTSTATUS
GcKmBaseDisplay::Stop()
{
    if (RenderOnly == GcKmdGlobal::s_DriverMode)
    {
        return STATUS_SUCCESS;
    }

    HwStop(&m_PreviousPostDisplayInfo, TRUE);

    if (DisplayOnly == GcKmdGlobal::s_DriverMode)
    {
        UnmapFrameBuffer(&m_FrameBuffer);
    }

    return STATUS_SUCCESS;
}

NTSTATUS
GcKmBaseDisplay::QueryAdapterInfo(
    const DXGKARG_QUERYADAPTERINFO* pQueryAdapterInfo)
{
    switch (pQueryAdapterInfo->Type)
    {
    case DXGKQAITYPE_DISPLAY_DRIVERCAPS_EXTENSION:
    {
        DXGK_DISPLAY_DRIVERCAPS_EXTENSION  *pDisplayCapExt = (DXGK_DISPLAY_DRIVERCAPS_EXTENSION *)pQueryAdapterInfo->pOutputData;

        pDisplayCapExt->VirtualModeSupport = 1; // Enable rotated mode

        //
        // TODO: Research DCSS's HDR and Secure Display Capability
        //

#ifdef DXGKDDI_INTERFACE_VERSION_WDDM2_6
#if (DXGKDDI_INTERFACE_VERSION > DXGKDDI_INTERFACE_VERSION_WDDM2_6)

        pDisplayCapExt->Hdr10MetadataSupport = 0;

#endif
#endif

#ifdef DXGKDDI_INTERFACE_VERSION_WDDM2_5
#if (DXGKDDI_INTERFACE_VERSION >= DXGKDDI_INTERFACE_VERSION_WDDM2_5)

        //
        // Only one of them should be set
        //

        pDisplayCapExt->HdrFP16ScanoutSupport = 0;
        pDisplayCapExt->HdrARGB10ScanoutSupport = 0;

#endif
#endif

        pDisplayCapExt->SecureDisplaySupport = 0;
    }
    break;

    case DXGKQAITYPE_DISPLAYID_DESCRIPTOR:
    default:
        GC_LOG_WARNING(
            "Unsupported query type. (pQueryAdapterInfo->Type=%d, pQueryAdapterInfo=0x%p)",
            pQueryAdapterInfo->Type,
            pQueryAdapterInfo);

        return STATUS_NOT_SUPPORTED;
    }

    return STATUS_SUCCESS;
}

NTSTATUS GcKmBaseDisplay::QueryInterface(
    IN_PQUERY_INTERFACE QueryInterface)
{
    return STATUS_NOT_SUPPORTED;
}

NTSTATUS
GcKmBaseDisplay::QueryChildStatus(
    IN_PDXGK_CHILD_STATUS   ChildStatus,
    IN_BOOLEAN              NonDestructiveOnly)
{
    UINT8 status;
    auto ret = m_pTransmitter->GetHotPlugDetectStatus(&status);
    if (ret == STATUS_SUCCESS)
    {
        ChildStatus->HotPlug.Connected = status;
    }

    return ret;
}

NTSTATUS
GcKmBaseDisplay::QueryDeviceDescriptor(
    IN_ULONG                        ChildUid,
    INOUT_PDXGK_DEVICE_DESCRIPTOR   pDeviceDescriptor)
{
    if (pDeviceDescriptor->DescriptorOffset == 0)
    {
        return m_pTransmitter->GetEdid(
            pDeviceDescriptor->DescriptorBuffer,
            pDeviceDescriptor->DescriptorLength,
            0, 0);
    }
    else
    {
        return STATUS_MONITOR_NO_MORE_DESCRIPTOR_DATA;
    }
}

NTSTATUS
GcKmBaseDisplay::QueryChildRelations(
    INOUT_PDXGK_CHILD_DESCRIPTOR ChildRelations,
    IN_ULONG ChildRelationsSizeInBytes
)
{
    PAGED_CODE();
    GC_ASSERT_MAX_IRQL(PASSIVE_LEVEL);

    // Enumerate the child devices of the adapter. There is a single child
    // to enumerate which is the HDMI connector. The number of children
    // is specified above in pNumberOfChildren

    const ULONG childRelationsCount =
        ChildRelationsSizeInBytes / sizeof(*ChildRelations);

    // The caller allocates and zeros one more entry that we specifed in
    // the pNumberOfChildren output parameter of StartDevice
    NT_ASSERT(childRelationsCount == (CHILD_COUNT + 1));

    // The following code assumes a single child descriptor
    NT_ASSERT(CHILD_COUNT == 1);

    m_pTransmitter->GetChildDescriptor(&ChildRelations[0]);

    GC_LOG_TRACE("Child relations reported successfully.");
    return STATUS_SUCCESS;
}

NTSTATUS
GcKmBaseDisplay::SetPowerState(
    IN_ULONG                DeviceUid,
    IN_DEVICE_POWER_STATE   DevicePowerState,
    IN_POWER_ACTION         ActionType)
{
    HwSetPowerState(DeviceUid, DevicePowerState, ActionType);

    return STATUS_SUCCESS;
}

NTSTATUS
GcKmBaseDisplay::SetPointerPosition(
    IN_CONST_PDXGKARG_SETPOINTERPOSITION    pSetPointerPosition)
{
    return STATUS_SUCCESS;
}

NTSTATUS
GcKmBaseDisplay::SetPointerShape(
    IN_CONST_PDXGKARG_SETPOINTERSHAPE   pSetPointerShape)
{
    return STATUS_SUCCESS;
}

NTSTATUS
GcKmBaseDisplay::IsSupportedVidPn(
    INOUT_PDXGKARG_ISSUPPORTEDVIDPN pIsSupportedVidPn)
{
    // For now, accept any VidPn topology.
    pIsSupportedVidPn->IsVidPnSupported = TRUE;
    return STATUS_SUCCESS;
}

NTSTATUS 
GcKmBaseDisplay::RecommendFunctionalVidPn(
    IN_CONST_PDXGKARG_RECOMMENDFUNCTIONALVIDPN_CONST    pRecommendFunctionalVidPn)
{
    return STATUS_SUCCESS;
}

NTSTATUS
GcKmBaseDisplay::SetVidPnSourceVisibility(
    IN_CONST_PDXGKARG_SETVIDPNSOURCEVISIBILITY  pSetVidPnSourceVisibility)
{
    return STATUS_SUCCESS;
}

NTSTATUS
GcKmBaseDisplay::CommitVidPn(
    IN_CONST_PDXGKARG_COMMITVIDPN_CONST pCommitVidPn)
{
    if (pCommitVidPn->Flags.PathPoweredOff)
    {
        return STATUS_SUCCESS;
    }

    const DXGK_VIDPN_INTERFACE* VidPnInterface;

    auto Status = m_pDxgkInterface->DxgkCbQueryVidPnInterface(
        pCommitVidPn->hFunctionalVidPn,
        DXGK_VIDPN_INTERFACE_VERSION_V1,
        &VidPnInterface);
    RETURN_ON_FAILURE(Status);

    // Get the topology
    D3DKMDT_HVIDPNTOPOLOGY Topology;
    const DXGK_VIDPNTOPOLOGY_INTERFACE* TopologyInterface;

    Status = VidPnInterface->pfnGetTopology(
        pCommitVidPn->hFunctionalVidPn,
        &Topology,
        &TopologyInterface);
    RETURN_ON_FAILURE(Status);

    const D3DKMDT_VIDPN_PRESENT_PATH* pPath = NULL;

    size_t numPaths = 0;
    Status = TopologyInterface->pfnGetNumPaths(Topology, &numPaths);
    RETURN_ON_FAILURE(Status);
    if (numPaths) {
        Status = TopologyInterface->pfnAcquireFirstPathInfo(Topology, &pPath);
        RETURN_ON_FAILURE(Status);
    }

    D3DKMDT_HVIDPNSOURCEMODESET hSourceModeSet = NULL;
    const DXGK_VIDPNSOURCEMODESET_INTERFACE* SourceModeSetInterface = NULL;
    const D3DKMDT_VIDPN_SOURCE_MODE* pSourceMode = NULL;

    D3DKMDT_HVIDPNTARGETMODESET hTargetModeSet = NULL;
    const DXGK_VIDPNTARGETMODESET_INTERFACE* TargetModeSetInterface = NULL;
    const D3DKMDT_VIDPN_TARGET_MODE* pTargetMode = NULL;

    if (pPath)
    {
        m_TargetId = 0;

        do
        {
            Status = VidPnInterface->pfnAcquireSourceModeSet(
                pCommitVidPn->hFunctionalVidPn,
                pPath->VidPnSourceId,
                &hSourceModeSet,
                &SourceModeSetInterface);
            if (!NT_SUCCESS(Status))
            {
                break;
            }

            Status = SourceModeSetInterface->pfnAcquirePinnedModeInfo(
                hSourceModeSet,
                &pSourceMode);
            if (!NT_SUCCESS(Status))
            {
                break;
            }

            Status = VidPnInterface->pfnAcquireTargetModeSet(
                pCommitVidPn->hFunctionalVidPn,
                pPath->VidPnTargetId,
                &hTargetModeSet,
                &TargetModeSetInterface);
            if (!NT_SUCCESS(Status))
            {
                break;
            }

            Status = TargetModeSetInterface->pfnAcquirePinnedModeInfo(
                hTargetModeSet,
                &pTargetMode);
            if (!NT_SUCCESS(Status))
            {
                break;
            }

        } while (FALSE);

        if (pSourceMode && pTargetMode)
        {
            Status = HwCommitVidPn(pSourceMode, pTargetMode, pCommitVidPn);
            if (NT_SUCCESS(Status))
            {
                m_TargetId = pPath->VidPnTargetId;

                PrepareScanlineEmulation(pTargetMode);
            }
        }
    } else {
        HwStopScanning(m_TargetId);

        PrepareScanlineEmulation(pTargetMode);
    }

    if (pPath)
    {
        TopologyInterface->pfnReleasePathInfo(Topology, pPath);
    }

    if (pSourceMode)
    {
        SourceModeSetInterface->pfnReleaseModeInfo(
            hSourceModeSet,
            pSourceMode);
    }

    if (hSourceModeSet)
    {
        VidPnInterface->pfnReleaseSourceModeSet(
            pCommitVidPn->hFunctionalVidPn,
            hSourceModeSet);
    }

    if (pTargetMode)
    {
        TargetModeSetInterface->pfnReleaseModeInfo(
            hTargetModeSet,
            pTargetMode);
    }

    if (hTargetModeSet)
    {
        VidPnInterface->pfnReleaseTargetModeSet(
            pCommitVidPn->hFunctionalVidPn,
            hTargetModeSet);
    }

    return Status;
}

NTSTATUS
GcKmBaseDisplay::UpdateActiveVidPnPresentPath(
    IN_CONST_PDXGKARG_UPDATEACTIVEVIDPNPRESENTPATH_CONST    pUpdateActiveVidPnPresentPath)
{
    return STATUS_SUCCESS;
}

NTSTATUS
GcKmBaseDisplay::RecommendMonitorModes(
    IN_CONST_PDXGKARG_RECOMMENDMONITORMODES_CONST   pRecommendMonitorMode)
{
    D3DKMDT_MONITOR_SOURCE_MODE const * pMode = nullptr;
    auto hModeSet = pRecommendMonitorMode->hMonitorSourceModeSet;
    auto Interface = pRecommendMonitorMode->pMonitorSourceModeSetInterface;

    auto Status = Interface->pfnAcquirePreferredModeInfo(hModeSet, &pMode);
    if (Status == STATUS_SUCCESS)
    {
        m_NativeMonitorMode = *pMode;
        m_bNativeMonitorModeSet = true;
    }
    else if (STATUS_GRAPHICS_NO_PREFERRED_MODE == Status)
    {
        Status = Interface->pfnAcquireFirstModeInfo(hModeSet, &pMode);
        if (Status == STATUS_SUCCESS)
        {
            m_NativeMonitorMode = *pMode;
            m_bNativeMonitorModeSet = true;
        }
        else
        {
            return STATUS_SUCCESS;
        }
    }

    if (pMode)
    {
        auto ReleaseStatus = Interface->pfnReleaseModeInfo(hModeSet, pMode);
        DEBUG_CHECK(ReleaseStatus);
    }

    return Status;
}

NTSTATUS
GcKmBaseDisplay::GetScanLine(
    INOUT_PDXGKARG_GETSCANLINE  pGetScanLine)
{
    if ((m_FrameInTick == 0) || (m_ScanlineInTick == 0))
    {
        return STATUS_SUCCESS;
    }

    LARGE_INTEGER   PerfCounter;
    LONGLONG    Scanlines;

    PerfCounter = KeQueryPerformanceCounter(NULL);
    Scanlines = PerfCounter.QuadPart%m_FrameInTick;

    pGetScanLine->ScanLine = (UINT)Scanlines/m_ScanlineInTick;
    pGetScanLine->InVerticalBlank = pGetScanLine->ScanLine > m_VActive ? 1 : 0;

    return STATUS_SUCCESS;
}

NTSTATUS
GcKmBaseDisplay::QueryVidPnHWCapability(
    INOUT_PDXGKARG_QUERYVIDPNHWCAPABILITY   pVidPnHWCaps)
{
    return STATUS_SUCCESS;
}

NTSTATUS
GcKmBaseDisplay::PresentDisplayOnly(
    IN_CONST_PDXGKARG_PRESENT_DISPLAYONLY  pPresentDisplayOnly)
{
    auto BytesPerPixel = pPresentDisplayOnly->BytesPerPixel;

    NT_ASSERT(BytesPerPixel == 4);
    NT_ASSERT(!pPresentDisplayOnly->Flags.Rotate);

    for (ULONG i = 0; i < pPresentDisplayOnly->NumDirtyRects; i++)
    {
        const auto& Rect = pPresentDisplayOnly->pDirtyRect[i];
        NT_ASSERT(Rect.left <= Rect.right);
        NT_ASSERT(Rect.top <= Rect.bottom);

        const auto BytesPerRow = (Rect.right - Rect.left) * BytesPerPixel;
        auto SrcPitch = pPresentDisplayOnly->Pitch;
        UINT DstPitch;

        NT_ASSERT(0 == pPresentDisplayOnly->VidPnSourceId);
        DstPitch = m_CurTargetModes[pPresentDisplayOnly->VidPnSourceId].VideoSignalInfo.ActiveSize.cx * 4;

        auto Src = static_cast<BYTE*>(pPresentDisplayOnly->pSource)
            + static_cast<LONGLONG>(Rect.top) * SrcPitch
            + static_cast<LONGLONG>(Rect.left) * BytesPerPixel;
        auto Dst = static_cast<BYTE*>(m_FrameBuffer.m_Address)
            + static_cast<LONGLONG>(Rect.top) * DstPitch
            + static_cast<LONGLONG>(Rect.left) * BytesPerPixel;

        auto Rows = Rect.bottom - Rect.top;
        while (Rows)
        {
            Rows--;
            RtlCopyMemory(Dst, Src, BytesPerRow);
            Src += SrcPitch;
            Dst += DstPitch;
        }
    }

    return STATUS_SUCCESS;
}

NTSTATUS
GcKmBaseDisplay::StopDeviceAndReleasePostDisplayOwnership(
    _In_ D3DDDI_VIDEO_PRESENT_TARGET_ID TargetId,
    _Out_ PDXGK_DISPLAY_INFORMATION     pDisplayInfo)
{
    if (!g_bUsePreviousPostDisplayInfo)
    {
        m_PreviousPostDisplayInfo.Width = m_NativeMonitorMode.VideoSignalInfo.ActiveSize.cx;
        m_PreviousPostDisplayInfo.Height = m_NativeMonitorMode.VideoSignalInfo.ActiveSize.cy;
        m_PreviousPostDisplayInfo.Pitch = m_NativeMonitorMode.VideoSignalInfo.ActiveSize.cx * 4;
        m_PreviousPostDisplayInfo.ColorFormat = D3DDDIFMT_A8R8G8B8;
    }
    m_PreviousPostDisplayInfo.PhysicAddress = m_FbPhysicalAddr;
    *pDisplayInfo = m_PreviousPostDisplayInfo;

    return STATUS_SUCCESS;
}

NTSTATUS
GcKmBaseDisplay::UpdateMonitorLinkInfo(
    INOUT_PDXGKARG_UPDATEMONITORLINKINFO    pUpdateMonitorLinkInfo)
{
    return STATUS_SUCCESS;
}

NTSTATUS
GcKmBaseDisplay::SavePreviousPostDisplayInfo(DXGKRNL_INTERFACE* pDxgkInterface)
{
    auto Status = pDxgkInterface->DxgkCbAcquirePostDisplayOwnership(
        pDxgkInterface->DeviceHandle,
        &m_PreviousPostDisplayInfo);
    return Status;
}

NTSTATUS
GcKmBaseDisplay::MapFrameBuffer(GcKmdFrameBuffer* pFrameBuffer, PHYSICAL_ADDRESS PhysicAddress, ULONG Size)
{
    auto Address = MmMapIoSpaceEx(
        PhysicAddress,
        Size,
        PAGE_READWRITE | PAGE_WRITECOMBINE);

    if (Address == nullptr)
    {
        Address = MmMapIoSpaceEx(
            PhysicAddress,
            Size,
            PAGE_READWRITE | PAGE_NOCACHE);

        if (Address == nullptr)
        {
            return STATUS_NO_MEMORY;
        }
    }

    pFrameBuffer->m_Address = Address;
    pFrameBuffer->m_Size = Size;

    return STATUS_SUCCESS;
}

void
GcKmBaseDisplay::UnmapFrameBuffer(GcKmdFrameBuffer* pFrameBuffer)
{
    if (pFrameBuffer->m_Address)
    {
        MmUnmapIoSpace(pFrameBuffer->m_Address, pFrameBuffer->m_Size);
        pFrameBuffer->m_Address = nullptr;
        pFrameBuffer->m_Size = 0;
    }
}

NTSTATUS GcKmBaseDisplay::GetFramebufferInfo(DXGKRNL_INTERFACE* pDxgkInterface,
    PHYSICAL_ADDRESS *pAddress, ULONG *pSize)
{
    NTSTATUS Status;
    WCHAR Buff[512];

    if (!pAddress || !pSize)
    {
        return STATUS_INVALID_PARAMETER;
    }

    pAddress->QuadPart = 0;
    *pSize = 0;

    Status = GetReslist(pDxgkInterface, (PWCHAR)&Buff,
                sizeof(Buff));
    if (!NT_SUCCESS(Status))
    {
        DbgPrintEx(DPFLTR_IHVVIDEO_ID, DPFLTR_ERROR_LEVEL,
            "Base Display: ERROR getting resource list\n");
        return Status;
    }

    // It is expected that Framebuffer memory address and size is
    // always the second memory resource in the ACPI ResourceTemplate.
    // The index is counted from zero.
    Status = ParseReslist((PCM_RESOURCE_LIST)&Buff,
        CmResourceTypeMemory, pAddress, pSize, 1, ResourceType_Memory);
    if (!NT_SUCCESS(Status))
    {
        DbgPrintEx(DPFLTR_IHVVIDEO_ID, DPFLTR_ERROR_LEVEL,
            "Base Display: ERROR parsing resource list\n");
        return Status;
    }

    return STATUS_SUCCESS;
}

void GcKmBaseDisplay::PrepareScanlineEmulation(const D3DKMDT_VIDPN_TARGET_MODE* pTargetMode)
{
    if (pTargetMode)
    {
        LARGE_INTEGER   PerfFreq;

        KeQueryPerformanceCounter(&PerfFreq);

        auto HTotal = pTargetMode->VideoSignalInfo.TotalSize.cx;
        auto VTotal = pTargetMode->VideoSignalInfo.TotalSize.cy;
        auto PixelClock = pTargetMode->VideoSignalInfo.PixelRate;

        m_FrameInTick = (UINT)(PerfFreq.QuadPart*HTotal*VTotal/PixelClock);
        m_ScanlineInTick = (UINT)(PerfFreq.QuadPart*HTotal/PixelClock);
        m_VActive = pTargetMode->VideoSignalInfo.ActiveSize.cy;
    }
    else
    {
        m_FrameInTick = 0;
        m_ScanlineInTick = 0;
    }
}

NTSTATUS GcKmBaseDisplay::BrightnessSet(
    _In_ UCHAR Brightness)
{
    return STATUS_SUCCESS;
}


NTSTATUS GcKmBaseDisplay::InitResources(DXGKRNL_INTERFACE* pDxgkInterface, UINT registryIndex)
{
    NTSTATUS Status = STATUS_SUCCESS;
    DXGK_DEVICE_INFO DeviceInfo;

    if ((!pDxgkInterface)||(NUM_OF_INTERFACES <=registryIndex))
    {
        return STATUS_INVALID_PARAMETER;
    }

    Status = pDxgkInterface->DxgkCbGetDeviceInformation(
        pDxgkInterface->DeviceHandle, &DeviceInfo);
    if (!NT_SUCCESS(Status))
    {
        return Status;
    }

    Status = Get_DsdAcpiResources(DeviceInfo.PhysicalDeviceObject, registryIndex);

    if (NT_SUCCESS(Status)) {
        if (NT_SUCCESS(Status)) {
            Status = STATUS_DEVICE_INSUFFICIENT_RESOURCES;
            Status = m_BrigthnessPWM.SetFile(m_PWMEndpoint);
        }
    }

    printk_debug("\tPWM Brightness: m_BrigthnessPWM.SetFile(%s) 0x%X\r\n", m_PWMEndpoint, Status);

    if (NT_SUCCESS(Status)) {
        __try {
            Status = m_BrigthnessPWM.Open();
            if (!NT_SUCCESS(Status)) {
                printk_debug("Failed to open PWM device. (0x%X)\r\n", Status);
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            printk_debug("Exception: Failed to open PWM device. (0x%X)\r\n", Status);
            Status = STATUS_INVALID_PARAMETER;
        }
    }

#if _DBG
    if (!NT_SUCCESS(Status)) {
        KdPrint(("\tGcKmBaseDisplay::InitResources: Failed (0x%x)\r\n", Status));
    }
#endif
    if (NT_SUCCESS(Status))
    {
        m_BrigthnessPWMInitialized = TRUE;
    }

    return Status;
}

NTSTATUS GcKmBaseDisplay::Get_DsdAcpiResources(PDEVICE_OBJECT PdoPtr, UINT registryIndex)
/*!
 * Load device resources and device specific data.
 *
 * @returns STATUS_SUCCESS or error code.
 */
{
    NTSTATUS status;
    UINT32 length = 0;
    AcpiDsdRes_t m_DsdRes(PdoPtr);

    status = m_DsdRes.LoadDsd();
    if (NT_SUCCESS(status)) {
        const AcpiDsdRes_t::_DSDVAL_GET_DESCRIPTOR ParamTable[] = {
        {
            "BrightnessPWM_0",
            (PUINT32) & (m_PWMEndpoint[0]),
            sizeof(m_PWMEndpoint),
            ACPI_METHOD_ARGUMENT_STRING,
        },
        {
            "BrightnessPWM_HID_0",
            (PUINT32) & (m_PWMEndpoint_HID[0]),
            sizeof(m_PWMEndpoint_HID),
            ACPI_METHOD_ARGUMENT_STRING,
        },
        {
            "BrightnessPWM_UID_0",
            (PUINT32) & (m_PWMEndpoint_UID[0]),
            sizeof(m_PWMEndpoint_UID),
            ACPI_METHOD_ARGUMENT_STRING,
        },
        {
            "BrightnessPWM_PIN_0",
            (PUINT32) & (m_PWMEndpoint_PIN[0]),
            sizeof(m_PWMEndpoint_PIN),
            ACPI_METHOD_ARGUMENT_STRING,
        },
        {
            "BrightnessPWM_1",
            (PUINT32) & (m_PWMEndpoint[0]),
            sizeof(m_PWMEndpoint),
            ACPI_METHOD_ARGUMENT_STRING,
        },
        {
            "BrightnessPWM_HID_1",
            (PUINT32) & (m_PWMEndpoint_HID[0]),
            sizeof(m_PWMEndpoint_HID),
            ACPI_METHOD_ARGUMENT_STRING,
        },
        {
            "BrightnessPWM_UID_1",
            (PUINT32) & (m_PWMEndpoint_UID[0]),
            sizeof(m_PWMEndpoint_UID),
            ACPI_METHOD_ARGUMENT_STRING,
        },
        {
            "BrightnessPWM_PIN_1",
            (PUINT32) & (m_PWMEndpoint_PIN[0]),
            sizeof(m_PWMEndpoint_PIN),
            ACPI_METHOD_ARGUMENT_STRING,
        },
        {
            "BrightnessPWM_2",
            (PUINT32) & (m_PWMEndpoint[0]),
            sizeof(m_PWMEndpoint),
            ACPI_METHOD_ARGUMENT_STRING,
        },
        {
            "BrightnessPWM_HID_2",
            (PUINT32) & (m_PWMEndpoint_HID[0]),
            sizeof(m_PWMEndpoint_HID),
            ACPI_METHOD_ARGUMENT_STRING,
        },
        {
            "BrightnessPWM_UID_2",
            (PUINT32) & (m_PWMEndpoint_UID[0]),
            sizeof(m_PWMEndpoint_UID),
            ACPI_METHOD_ARGUMENT_STRING,
        },
        {
            "BrightnessPWM_PIN_2",
            (PUINT32) & (m_PWMEndpoint_PIN[0]),
            sizeof(m_PWMEndpoint_PIN),
            ACPI_METHOD_ARGUMENT_STRING,
        },
        };
        printk_debug("\tregistryIndex     0x%d\r\n", registryIndex);
        status = m_DsdRes.GetDsdResources(&ParamTable[registryIndex * 4], 1);
        printk_debug("\tm_PWMEndpoint0     (%s) 0x%X\r\n", m_PWMEndpoint, status);
        if (!NT_SUCCESS(status))
        {
            status = m_DsdRes.GetDsdResources(&ParamTable[1 + registryIndex * 4], 3);
            printk_debug("\tm_PWMEndpoint0_HID  (%s), sizeof(m_PWMEndpoint_PIN) %d\r\n", m_PWMEndpoint_HID, sizeof(m_PWMEndpoint_PIN));
            printk_debug("\tm_PWMEndpoint0_UID (%s) 0x%X\r\n", m_PWMEndpoint_UID, status);
            printk_debug("\tm_PWMEndpoint0_PIN (%s) 0x%X\r\n", m_PWMEndpoint_PIN, status);
            // Example of path: "\\GLOBAL??\\ACPI#NXP010E#2#{60824b4c-eed1-4c9c-b49c-1b961461a819}\\0";
            strcat(m_PWMEndpoint, "\\GLOBAL??\\ACPI#");
            strcat(m_PWMEndpoint, m_PWMEndpoint_HID);
            strcat(m_PWMEndpoint, "#");
            strcat(m_PWMEndpoint, m_PWMEndpoint_UID);
            strcat(m_PWMEndpoint, "#{60824b4c-eed1-4c9c-b49c-1b961461a819}\\");
            strcat(m_PWMEndpoint, m_PWMEndpoint_PIN);
        }
    }
    return status;
}

NTSTATUS GcKmBaseDisplay::PWMSet(IN_UCHAR PWMValue)
{
    PAGED_CODE();
    NTSTATUS Status = STATUS_SUCCESS;
    PWM_PERCENTAGE Brightness_UL;

    if (m_BrigthnessPWMInitialized == TRUE)
    {
        NTSTATUS Status = STATUS_SUCCESS;
    }
    else
    {
        Status = STATUS_NOT_SUPPORTED;
    }


    if (NT_SUCCESS(Status)) {
        auto* sensor = m_BrigthnessPWM.getTarget();

        if (sensor == NULL) {
            Status = STATUS_INVALID_DEVICE_STATE;
            printk_debug("\tPWM Brightness: SendIrp failed STATUS_INVALID_DEVICE_STATE 0x%x\r\n", Status);
        }

        /* Set PWM */
        if (NT_SUCCESS(Status)) {

            Brightness_UL = (PWM_PERCENTAGE)(((PWMValue)) * (ULONGLONG_MAX / 100));

            PIRP LowIrp = IoBuildDeviceIoControlRequest(IOCTL_PWM_PIN_START, sensor->m_TargetDevicePtr, (PUCHAR)&Brightness_UL, sizeof(Brightness_UL), &Brightness_UL, sizeof(Brightness_UL), FALSE, &sensor->m_Event, &sensor->m_IoStatus);

            if (LowIrp) {
                Status = sensor->SendIrp(LowIrp, &sensor->m_Event);
                if (!NT_SUCCESS(Status)) {
                    printk_debug("\tPWM Brightness: SendIrp failed 0x%x\r\n", Status);
                    IoFreeIrp(LowIrp);
                }
            }
            else {
                Status = STATUS_MEMORY_NOT_ALLOCATED;
                printk_debug("\tPWM Brightness: Status = STATUS_MEMORY_NOT_ALLOCATED 0x%x\r\n", Status);
            }

            PIRP LowIrp2 = IoBuildDeviceIoControlRequest(IOCTL_PWM_PIN_SET_ACTIVE_DUTY_CYCLE_PERCENTAGE, sensor->m_TargetDevicePtr, (PUCHAR)&Brightness_UL, sizeof(Brightness_UL), &Brightness_UL, sizeof(Brightness_UL), FALSE, &sensor->m_Event, &sensor->m_IoStatus);

            if (LowIrp2) {
                Status = sensor->SendIrp(LowIrp2, &sensor->m_Event);
                if (!NT_SUCCESS(Status)) {
                    printk_debug("\tPWM Brightness: SendIrp failed 0x%x\r\n", Status);
                    IoFreeIrp(LowIrp2);
                }
            }
            else {
                Status = STATUS_MEMORY_NOT_ALLOCATED;
                printk_debug("\tPWM Brightness: Status = STATUS_MEMORY_NOT_ALLOCATED 0x%x\r\n", Status);
            }
            if (NT_SUCCESS(Status)) {
                Status = sensor->m_IoStatus.Status;
            }
        }
    }
    return Status;
}


GC_PAGED_SEGMENT_END; //========================================================
