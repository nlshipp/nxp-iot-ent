/* Copyright (c) Microsoft Corporation.
 * Copyright 2023-2024 NXP
   Licensed under the MIT License. */

#include "precomp.h"

#include "GcKmdBaseDisplay.h"
#include "GcKmdBaseDisplayController.h"
#include "GcKmdErroHandling.h"
#include "GcKmdGuard.h"
#include "GcKmdUtil.h"
#include "GcKmdGuard.h"
#include "GcKmdErroHandling.h"
#include <wdm.h>
#define DISPLAY_CONTROLLER_DEBUG
#define printk(x, ...) DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, x, __VA_ARGS__)

#ifdef DISPLAY_CONTROLLER_DEBUG
#define printk_debug printk
#else
#define printk_debug
#endif

//
// GcKmDisplay interface implementation
//
NTSTATUS
GcKmBaseDisplayController::Start(
    DXGKRNL_INTERFACE*  pDxgkInterface,
    UINT                LocalVidMemPhysicalBase,
    ULONG*              pNumberOfVideoPresentSources,
    ULONG*              pNumberOfChildren)
{
    NTSTATUS    Status = STATUS_INTERNAL_ERROR;

    *pNumberOfVideoPresentSources = 0;
    *pNumberOfChildren = 0;

    m_pDxgkInterface = pDxgkInterface;

    for (UINT i = 0; i < m_NumPipelines; i++)
    {
        Status = m_Pipelines[i].m_pDisplayPipeline->Start(
                                                        pDxgkInterface,
                                                        LocalVidMemPhysicalBase,
                                                        &m_Pipelines[i].m_NumSources,
                                                        &m_Pipelines[i].m_NumChildren);
        if (!NT_SUCCESS(Status))
        {
            break;
        }

        m_Pipelines[i].m_bStarted = true;

        *pNumberOfVideoPresentSources += m_Pipelines[i].m_NumSources;
        *pNumberOfChildren            += m_Pipelines[i].m_NumChildren;
    }

    if (!NT_SUCCESS(Status))
    {
        *pNumberOfVideoPresentSources = 0;
        *pNumberOfChildren = 0;
    }
    else
    {
        m_NumTargets = *pNumberOfChildren;
    }

    return Status;
}

NTSTATUS
GcKmBaseDisplayController::Stop()
{
    //
    // StopDeviceAndReleasePostDisplayOwnership() has stopped all pipelines
    //

    return STATUS_SUCCESS;
}

NTSTATUS
GcKmBaseDisplayController::QueryAdapterInfo(
    const DXGKARG_QUERYADAPTERINFO* pQueryAdapterInfo)
{
    NTSTATUS    Status;

    switch (pQueryAdapterInfo->Type)
    {
    case DXGKQAITYPE_DISPLAY_DRIVERCAPS_EXTENSION:
    {
        DXGK_DISPLAY_DRIVERCAPS_EXTENSION* pDisplayCapExt = (DXGK_DISPLAY_DRIVERCAPS_EXTENSION*)pQueryAdapterInfo->pOutputData;

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

        Status = STATUS_SUCCESS;
    }
    break;

    case DXGKQAITYPE_DISPLAYID_DESCRIPTOR:
    {
        DXGK_QUERYDISPLAYIDIN*  pQueryDisplayIdIn = (DXGK_QUERYDISPLAYIDIN*)pQueryAdapterInfo->pInputData;
        GcKmDisplay*    pDisplayPipeline = FindDisplayPipeline(pQueryDisplayIdIn->TargetId);

        if (!pDisplayPipeline)
        {
            Status = STATUS_INVALID_PARAMETER;
            break;
        }

        Status = pDisplayPipeline->QueryAdapterInfo(pQueryAdapterInfo);
    }
    break;

    default:
        Status = STATUS_NOT_SUPPORTED;
        break;
    }

    return Status;
}

NTSTATUS
GcKmBaseDisplayController::QueryChildRelations(
    INOUT_PDXGK_CHILD_DESCRIPTOR    ChildRelations,
    IN_ULONG                        ChildRelationsSize)
{
    NTSTATUS    Status = STATUS_INTERNAL_ERROR;
    DXGK_CHILD_DESCRIPTOR*  pCurChildRelations = ChildRelations;
    ULONG                   CurChildRelationsSize;
    UINT                    CurTarget = 0;

    for (UINT i = 0; i < m_NumPipelines; i++)
    {
        CurChildRelationsSize = (m_Pipelines[i].m_NumChildren + 1)*sizeof(DXGK_CHILD_DESCRIPTOR);

        Status = m_Pipelines[i].m_pDisplayPipeline->QueryChildRelations(pCurChildRelations, CurChildRelationsSize);
        if (!NT_SUCCESS(Status))
        {
            break;
        }

        for (UINT j = 0; j < m_Pipelines[i].m_NumChildren; j++)
        {
            m_Targets[CurTarget + j].m_pDisplayPipeline = m_Pipelines[i].m_pDisplayPipeline;
            m_Targets[CurTarget + j].m_ChildDescriptor = pCurChildRelations[j];
        }

        CurTarget           += m_Pipelines[i].m_NumChildren;
        pCurChildRelations  += m_Pipelines[i].m_NumChildren;
    }

    return Status;
}


GC_PAGED_SEGMENT_BEGIN;
VOID DeviceInterfaceReference(_In_ PVOID Context);/*
{
    UNREFERENCED_PARAMETER(Context);
}*/

VOID DeviceInterfaceDereference(_In_ PVOID Context);/*
{
    UNREFERENCED_PARAMETER(Context);
}*/

#pragma code_seg("PAGE")
NTSTATUS DxgkBrightnessGetPossible(
   PVOID Context,
   ULONG BufferSize,
   PUCHAR LevelCount,
   PUCHAR BrightnessLevels
)
{
    UINT MaxLevels = 100;
    NTSTATUS Status = STATUS_FLT_BUFFER_TOO_SMALL;

    printk_debug("GcKmBaseDisplayController::DxgkBrightnessGetPossible: BufferSize value: %d.\n", BufferSize);

    if (103 <= BufferSize)
    {

        /* - The first brightness level value is the brightness level that the BIOS uses when the computer runs on AC power.
           - The second brightness level value is the brightness level that the BIOS uses when the computer runs on DC power.
           - The remaining brightness level values are hardware-supported brightness levels. */

        *LevelCount = 101;
        BrightnessLevels[0] = 100;
        BrightnessLevels[1] = 50;
        //if (MaxLevels > BufferSize) MaxLevels = BufferSize;
        for (UCHAR i = 0; i <= MaxLevels; i++)
        {
            BrightnessLevels[i + 2] = i;
        }
        Status = STATUS_SUCCESS;
    }
    return Status;
}


#pragma code_seg("PAGE")
NTSTATUS DxgkBrightnessSet(
    PVOID Context,
    UCHAR Brightness
)
{
    ((GcKmBaseDisplayController*)Context)->BrightnessSet(Brightness);
    printk_debug("GcKmBaseDisplayController::DxgkBrightnessSet: Brightness value set: %d.\n", Brightness);
    return STATUS_SUCCESS;
}

#pragma code_seg("PAGE")
NTSTATUS DxgkBrightnessGet(
    PVOID Context,
    PUCHAR Brightness
)
{
    return ((GcKmBaseDisplayController*)Context)->BrightnessGet(Brightness);
}

#pragma code_seg("PAGE")
NTSTATUS DxgkBrightnessGetCaps(
    PVOID Context,
    DXGK_BRIGHTNESS_CAPS* BrightnessCaps
)
{
    BrightnessCaps->SmoothBrightness = 0;
    BrightnessCaps->AdaptiveBrightness = 0;
    BrightnessCaps->NitsBrightness = 0;
    return STATUS_SUCCESS;
}

#pragma code_seg("PAGE")
NTSTATUS DxgkBrightnessSetState(
    PVOID Context,
    DXGK_BRIGHTNESS_STATE* BrightnessState
)
{
    //BrightnessState->SmoothBrightness;
    return STATUS_SUCCESS;
}

#pragma code_seg("PAGE")
NTSTATUS DxgkBrightnessSetBacklightOptimization(
    PVOID Context,
    DXGK_BACKLIGHT_OPTIMIZATION_LEVEL OptimizationLevel
)
{
    return STATUS_SUCCESS;
}

#pragma code_seg("PAGE")
NTSTATUS DxgkBrightnessGetBacklightReduction(
     PVOID Context,
     DXGK_BACKLIGHT_INFO* BacklightInfo
)
{
    return STATUS_SUCCESS;
}
GC_PAGED_SEGMENT_END;

NTSTATUS GcKmBaseDisplayController::QueryInterface(
    IN_PQUERY_INTERFACE QueryInterface)
{
    NTSTATUS Status = STATUS_NOT_SUPPORTED;
        if (RtlEqualMemory(
            QueryInterface->InterfaceType,
            &GUID_DEVINTERFACE_BRIGHTNESS_2,
            sizeof(GUID)) &&
            (QueryInterface->Version == DXGK_BRIGHTNESS_INTERFACE_VERSION_2) &&
            (QueryInterface->Size == sizeof(DXGK_BRIGHTNESS_INTERFACE_2)) &&
            NT_SUCCESS(AnyBrigthnessIFExists()))
        {
            PDXGK_BRIGHTNESS_INTERFACE_2 pIfBrightness = (PDXGK_BRIGHTNESS_INTERFACE_2)QueryInterface->Interface;

            //NT_ASSERT(QueryInterface->DeviceUid == BaseTransmitter::HDMI_CHILD_UID);

            pIfBrightness->Size = sizeof(DXGK_BRIGHTNESS_INTERFACE_2);
            pIfBrightness->Version = DXGK_BRIGHTNESS_INTERFACE_VERSION_2;
            pIfBrightness->InterfaceReference = (PINTERFACE_REFERENCE)&DeviceInterfaceReference;
            pIfBrightness->InterfaceDereference = (PINTERFACE_DEREFERENCE)&DeviceInterfaceDereference;
            pIfBrightness->Context = this;

            pIfBrightness->GetPossibleBrightness = (DXGK_BRIGHTNESS_GET_POSSIBLE)&DxgkBrightnessGetPossible;
            pIfBrightness->SetBrightness = (DXGK_BRIGHTNESS_SET)&DxgkBrightnessSet;
            pIfBrightness->GetBrightness = (DXGK_BRIGHTNESS_GET)&DxgkBrightnessGet;
            pIfBrightness->GetBrightnessCaps = (DXGK_BRIGHTNESS_GET_CAPS)&DxgkBrightnessGetCaps;
            pIfBrightness->SetBrightnessState = (DXGK_BRIGHTNESS_SET_STATE)&DxgkBrightnessSetState;
            pIfBrightness->SetBacklightOptimization = (DXGK_BRIGHTNESS_SET_BACKLIGHT_OPTIMIZATION)&DxgkBrightnessSetBacklightOptimization;
            pIfBrightness->GetBacklightReduction = (DXGK_BRIGHTNESS_GET_BACKLIGHT_REDUCTION)&DxgkBrightnessGetBacklightReduction;

            printk_debug("GcKmBaseDisplayController::QueryInterface: interface GUID_DEVINTERFACE_BRIGHTNESS successfull queried.\n");

            Status = STATUS_SUCCESS;
        }

    if (!NT_SUCCESS(Status))
    {
        GcKmDisplay* pDisplayPipeline = FindDisplayPipeline(QueryInterface->DeviceUid);

        if (pDisplayPipeline) {
            Status = pDisplayPipeline->QueryInterface(QueryInterface);
        }
    }
    return Status;
}

NTSTATUS
GcKmBaseDisplayController::BrightnessSet(
    //PVOID Context,
    _In_ UCHAR Brightness
)
{
    m_Brightness = Brightness;
    for (UINT i = 0; i < m_NumTargets; i++)
    {
        m_Targets[i].m_pDisplayPipeline->BrightnessSet(Brightness);        
        printk_debug("GcKmBaseDisplayController::BrightnessSet: Brightness value set: %d on m_pDisplayPipeline %d.\n", Brightness, i);
    }
    return STATUS_SUCCESS;
}


NTSTATUS
GcKmBaseDisplayController::AnyBrigthnessIFExists()
{
    NTSTATUS Status = STATUS_NOT_FOUND;

    for (UINT i = 0; i < m_NumTargets; i++)
    {
        if (NT_SUCCESS(m_Targets[i].m_pDisplayPipeline->GetBrigthnessIFExists()))
        {
            Status = STATUS_SUCCESS;
            break;
        }
    }
    return Status;
}


NTSTATUS
GcKmBaseDisplayController::BrightnessGet(
    _Out_ PUCHAR pBrightness)
{
    *pBrightness = m_Brightness;
    return STATUS_SUCCESS;
}


NTSTATUS
GcKmBaseDisplayController::QueryChildStatus(
    IN_PDXGK_CHILD_STATUS   ChildStatus,
    IN_BOOLEAN              NonDestructiveOnly)
{
    GcKmDisplay*    pDisplayPipeline = FindDisplayPipeline(ChildStatus->ChildUid);

    return pDisplayPipeline->QueryChildStatus(ChildStatus, NonDestructiveOnly);
}

NTSTATUS
GcKmBaseDisplayController::QueryDeviceDescriptor(
    IN_ULONG                        ChildUid,
    INOUT_PDXGK_DEVICE_DESCRIPTOR   pDeviceDescriptor)
{
    GcKmDisplay*    pDisplayPipeline = FindDisplayPipeline(ChildUid);

    return pDisplayPipeline->QueryDeviceDescriptor(ChildUid, pDeviceDescriptor);
}

NTSTATUS
GcKmBaseDisplayController::SetPowerState(
    IN_ULONG                DeviceUid,
    IN_DEVICE_POWER_STATE   DevicePowerState,
    IN_POWER_ACTION         ActionType)
{
    GcKmDisplay*    pDisplayPipeline = FindDisplayPipeline(DeviceUid);

    return pDisplayPipeline->SetPowerState(DeviceUid, DevicePowerState, ActionType);
}

NTSTATUS
GcKmBaseDisplayController::SetPointerPosition(
    IN_CONST_PDXGKARG_SETPOINTERPOSITION    pSetPointerPosition)
{
    NTSTATUS    Status = STATUS_SUCCESS;

    for (UINT ClonedTarget = 0; ClonedTarget < GC_KM_MAX_CLONES; ClonedTarget++)
    {
        auto pActivePathInfo = &m_Paths[pSetPointerPosition->VidPnSourceId][ClonedTarget];
        if (0 == pActivePathInfo->m_TargetId)
        {
            continue;
        }

        auto SubStatus = pActivePathInfo->m_pDisplayPipeline->SetPointerPosition(pSetPointerPosition);
        if (!NT_SUCCESS(SubStatus))
        {
            Status = SubStatus;
        }
    }

    return Status;
}

NTSTATUS
GcKmBaseDisplayController::SetPointerShape(
    IN_CONST_PDXGKARG_SETPOINTERSHAPE   pSetPointerShape)
{
    NTSTATUS    Status = STATUS_SUCCESS;

    for (UINT ClonedTarget = 0; ClonedTarget < GC_KM_MAX_CLONES; ClonedTarget++)
    {
        auto pActivePathInfo = &m_Paths[pSetPointerShape->VidPnSourceId][ClonedTarget];
        if (0 == pActivePathInfo->m_TargetId)
        {
            continue;
        }

        auto SubStatus = pActivePathInfo->m_pDisplayPipeline->SetPointerShape(pSetPointerShape);
        if (!NT_SUCCESS(SubStatus))
        {
            Status = SubStatus;
        }
    }

    return Status;
}

NTSTATUS
GcKmBaseDisplayController::IsSupportedVidPn(
    INOUT_PDXGKARG_ISSUPPORTEDVIDPN pIsSupportedVidPn)
{
    pIsSupportedVidPn->IsVidPnSupported = TRUE;

    return STATUS_SUCCESS;
}

NTSTATUS
GcKmBaseDisplayController::RecommendFunctionalVidPn(
    IN_CONST_PDXGKARG_RECOMMENDFUNCTIONALVIDPN_CONST    pRecommendFunctionalVidPn)
{
    return STATUS_SUCCESS;
}

NTSTATUS
GcKmBaseDisplayController::EnumVidPnCofuncModality(
    IN_CONST_PDXGKARG_ENUMVIDPNCOFUNCMODALITY_CONST pEnumCofuncModality)
{
    // Get VidPN interface
    const DXGK_VIDPN_INTERFACE* pVidPnInterface;
    auto Status = m_pDxgkInterface->DxgkCbQueryVidPnInterface(
                                        pEnumCofuncModality->hConstrainingVidPn,
                                        DXGK_VIDPN_INTERFACE_VERSION_V1,
                                        &pVidPnInterface);
    RETURN_ON_FAILURE(Status);

    // Get the topology
    D3DKMDT_HVIDPNTOPOLOGY  hVidPnTopology;
    DXGK_VIDPNTOPOLOGY_INTERFACE const* pTopologyInterface;
    Status = pVidPnInterface->pfnGetTopology(
                                pEnumCofuncModality->hConstrainingVidPn,
                                &hVidPnTopology,
                                &pTopologyInterface);
    RETURN_ON_FAILURE(Status);

    const D3DKMDT_VIDPN_PRESENT_PATH*   pPath;
    Status = pTopologyInterface->pfnAcquireFirstPathInfo(hVidPnTopology, &pPath);
    RETURN_ON_FAILURE(Status);

    while (NT_SUCCESS(Status))
    {
        NT_ASSERT(pPath);
        auto PathGuard = MakeScopeExitGuard([&, pPath]()
        {
            auto Status = pTopologyInterface->pfnReleasePathInfo(hVidPnTopology, pPath);
            DEBUG_CHECK(Status);
        });

        GcKmBaseDisplay*    pPipeline = FindDisplayPipeline(pPath->VidPnTargetId);

        Status = pPipeline->ProcessSourceModeSet(pEnumCofuncModality, pVidPnInterface, pPath);
        RETURN_ON_FAILURE(Status);

        Status = pPipeline->ProcessTargetModeSet(pEnumCofuncModality, pVidPnInterface, pPath);
        RETURN_ON_FAILURE(Status);

        Status = pPipeline->ProcessVidPnPathAttributes(pEnumCofuncModality, pPath, hVidPnTopology, pTopologyInterface);
        RETURN_ON_FAILURE(Status);

        const D3DKMDT_VIDPN_PRESENT_PATH*   pNextPath;
        Status = pTopologyInterface->pfnAcquireNextPathInfo(hVidPnTopology, pPath, &pNextPath);
        if (STATUS_GRAPHICS_NO_MORE_ELEMENTS_IN_DATASET == Status)
        {
            // No more paths to process.
            break;
        }
        RETURN_ON_FAILURE(Status);

        pPath = pNextPath;
    }

    return STATUS_SUCCESS;
}

NTSTATUS
GcKmBaseDisplayController::SetVidPnSourceAddress(
    IN_CONST_PDXGKARG_SETVIDPNSOURCEADDRESS pSetVidPnSourceAddress)
{
    NTSTATUS    Status = STATUS_SUCCESS;

    for (UINT ClonedTarget = 0; ClonedTarget < GC_KM_MAX_CLONES; ClonedTarget++)
    {
        auto pActivePathInfo = &m_Paths[pSetVidPnSourceAddress->VidPnSourceId][ClonedTarget];
        if (0 == pActivePathInfo->m_TargetId)
        {
            continue;
        }

        auto SubStatus = pActivePathInfo->m_pDisplayPipeline->SetVidPnSourceAddress(pSetVidPnSourceAddress);
        if (!NT_SUCCESS(SubStatus))
        {
            Status = SubStatus;
        }
    }

    return Status;
}

NTSTATUS
GcKmBaseDisplayController::SetVidPnSourceVisibility(
    IN_CONST_PDXGKARG_SETVIDPNSOURCEVISIBILITY  pSetVidPnSourceVisibility)
{
    NTSTATUS    Status = STATUS_SUCCESS;

    for (UINT ClonedTarget = 0; ClonedTarget < GC_KM_MAX_CLONES; ClonedTarget++)
    {
        auto pActivePathInfo = &m_Paths[pSetVidPnSourceVisibility->VidPnSourceId][ClonedTarget];
        if (0 == pActivePathInfo->m_TargetId)
        {
            continue;
        }

        auto SubStatus = pActivePathInfo->m_pDisplayPipeline->SetVidPnSourceVisibility(pSetVidPnSourceVisibility);
        if (!NT_SUCCESS(SubStatus))
        {
            Status = SubStatus;
        }
    }

    return Status;
}

NTSTATUS
GcKmBaseDisplayController::CommitVidPn(
    IN_CONST_PDXGKARG_COMMITVIDPN_CONST pCommitVidPn)
{
    if (pCommitVidPn->Flags.PathPoweredOff)
    {
        return STATUS_SUCCESS;
    }

    const DXGK_VIDPN_INTERFACE* pVidPnInterface;

    auto Status = m_pDxgkInterface->DxgkCbQueryVidPnInterface(
                                        pCommitVidPn->hFunctionalVidPn,
                                        DXGK_VIDPN_INTERFACE_VERSION_V1,
                                        &pVidPnInterface);
    RETURN_ON_FAILURE(Status);

    // Get the topology
    D3DKMDT_HVIDPNTOPOLOGY hVidPnTopology;
    const DXGK_VIDPNTOPOLOGY_INTERFACE* pTopologyInterface;

    Status = pVidPnInterface->pfnGetTopology(
                                pCommitVidPn->hFunctionalVidPn,
                                &hVidPnTopology,
                                &pTopologyInterface);
    RETURN_ON_FAILURE(Status);

    const D3DKMDT_VIDPN_PRESENT_PATH*   pPath = NULL;
    BOOLEAN bAffectedSourceFound = FALSE;

    Status = pTopologyInterface->pfnAcquireFirstPathInfo(hVidPnTopology, &pPath);

    while (nullptr != pPath)
    {
        if (pPath->VidPnSourceId == pCommitVidPn->AffectedVidPnSourceId)
        {
            D3DKMDT_HVIDPNSOURCEMODESET                 hSourceModeSet = NULL;
            const DXGK_VIDPNSOURCEMODESET_INTERFACE*    pSourceModeSetInterface = NULL;
            const D3DKMDT_VIDPN_SOURCE_MODE*            pSourceMode = NULL;

            D3DKMDT_HVIDPNTARGETMODESET                 hTargetModeSet = NULL;
            const DXGK_VIDPNTARGETMODESET_INTERFACE*    pTargetModeSetInterface = NULL;
            const D3DKMDT_VIDPN_TARGET_MODE*            pTargetMode = NULL;

            do
            {
                Status = pVidPnInterface->pfnAcquireSourceModeSet(
                                            pCommitVidPn->hFunctionalVidPn,
                                            pPath->VidPnSourceId,
                                            &hSourceModeSet,
                                            &pSourceModeSetInterface);
                if (!NT_SUCCESS(Status))
                {
                    break;
                }

                Status = pSourceModeSetInterface->pfnAcquirePinnedModeInfo(
                                                    hSourceModeSet,
                                                    &pSourceMode);
                if (!NT_SUCCESS(Status))
                {
                    break;
                }

                Status = pVidPnInterface->pfnAcquireTargetModeSet(
                                            pCommitVidPn->hFunctionalVidPn,
                                            pPath->VidPnTargetId,
                                            &hTargetModeSet,
                                            &pTargetModeSetInterface);
                if (!NT_SUCCESS(Status))
                {
                    break;
                }

                Status = pTargetModeSetInterface->pfnAcquirePinnedModeInfo(
                                                    hTargetModeSet,
                                                    &pTargetMode);
                if (!NT_SUCCESS(Status))
                {
                    break;
                }

            } while (FALSE);

            if (pSourceMode && pTargetMode)
            {
                GcKmBaseDisplay*    pPipeline = FindDisplayPipeline(pPath->VidPnTargetId);

                Status = pPipeline->HwCommitVidPn(pSourceMode, pTargetMode, pCommitVidPn);

                if (STATUS_SUCCESS == Status)
                {
                    auto SourceId = pCommitVidPn->AffectedVidPnSourceId;
                    GcKmDisplayPathInfo*    pNewPathInfo = NULL;

                    for (UINT ClonedTarget = 0; ClonedTarget < GC_KM_MAX_CLONES; ClonedTarget++)
                    {
                        pNewPathInfo = &m_Paths[SourceId][ClonedTarget];

                        if ((pPath->VidPnTargetId == pNewPathInfo->m_TargetId) || (0 == pNewPathInfo->m_TargetId))
                        {
                            break;
                        }
                    }

                    pNewPathInfo->m_pDisplayPipeline = pPipeline;
                    pNewPathInfo->m_TargetId = pPath->VidPnTargetId;
                    pNewPathInfo->m_Importance = pPath->ImportanceOrdinal;
                    pNewPathInfo->m_CurSourceMode = *pSourceMode;
                    pNewPathInfo->m_CurTargetMode = *pTargetMode;

                    if ((SourceId + 1) > m_NumSources)
                    {
                        m_NumSources = SourceId + 1;
                    }

                    pPipeline->PrepareScanlineEmulation(pTargetMode);
                }

                bAffectedSourceFound = TRUE;
            }

            if (pSourceMode)
            {
                pSourceModeSetInterface->pfnReleaseModeInfo(
                                            hSourceModeSet,
                                            pSourceMode);
            }

            if (hSourceModeSet)
            {
                pVidPnInterface->pfnReleaseSourceModeSet(
                                    pCommitVidPn->hFunctionalVidPn,
                                    hSourceModeSet);
            }

            if (pTargetMode)
            {
                pTargetModeSetInterface->pfnReleaseModeInfo(
                                            hTargetModeSet,
                                            pTargetMode);
            }

            if (hTargetModeSet)
            {
                pVidPnInterface->pfnReleaseTargetModeSet(
                                    pCommitVidPn->hFunctionalVidPn,
                                    hTargetModeSet);
            }
        }

        const D3DKMDT_VIDPN_PRESENT_PATH*   pNextPath;
        Status = pTopologyInterface->pfnAcquireNextPathInfo(hVidPnTopology, pPath, &pNextPath);

        pTopologyInterface->pfnReleasePathInfo(hVidPnTopology, pPath);

        pPath = pNextPath;
    }

    if (bAffectedSourceFound)
    {
        //
        // In cloned mode, disable VSync notification from secondary monitors
        //

        auto SourceId = pCommitVidPn->AffectedVidPnSourceId;
        UINT    NumActiveTargets = 0;
        D3DKMDT_VIDPN_PRESENT_PATH_IMPORTANCE   MostImorptant = D3DKMDT_VPPI_DENARY;

        for (UINT ClonedTarget = 0; ClonedTarget < GC_KM_MAX_CLONES; ClonedTarget++)
        {
            if (0 == m_Paths[SourceId][ClonedTarget].m_TargetId)
            {
                continue;
            }

            NumActiveTargets++;
            if (m_Paths[SourceId][ClonedTarget].m_Importance < MostImorptant)
            {
                MostImorptant = m_Paths[SourceId][ClonedTarget].m_Importance;
            }
        }

        if (NumActiveTargets > 1)
        {
            for (UINT ClonedTarget = 0; ClonedTarget < GC_KM_MAX_CLONES; ClonedTarget++)
            {
                if (0 == m_Paths[SourceId][ClonedTarget].m_TargetId)
                {
                    continue;
                }

                if (m_Paths[SourceId][ClonedTarget].m_Importance > MostImorptant)
                {
                    m_Paths[SourceId][ClonedTarget].m_pDisplayPipeline->DisableVSyncNotification();
                }
            }
        }
    }
    else
    {
        auto SourceId = pCommitVidPn->AffectedVidPnSourceId;

        //
        // Assumption: All cloned targets for a source are disabled together
        //

        for (UINT ClonedTarget = 0; ClonedTarget < GC_KM_MAX_CLONES; ClonedTarget++)
        {
            auto pActivePathInfo = &m_Paths[SourceId][ClonedTarget];

            if (0 == m_Paths[SourceId][ClonedTarget].m_TargetId)
            {
                continue;
            }

            pActivePathInfo->m_pDisplayPipeline->HwStopScanning(pActivePathInfo->m_TargetId);

            pActivePathInfo->m_TargetId = 0;
            memset(&pActivePathInfo->m_CurSourceMode, 0, sizeof(pActivePathInfo->m_CurSourceMode));
            memset(&pActivePathInfo->m_CurTargetMode, 0, sizeof(pActivePathInfo->m_CurTargetMode));

            pActivePathInfo->m_pDisplayPipeline->PrepareScanlineEmulation(nullptr);
        }

        if ((SourceId + 1) == m_NumSources)
        {
            m_NumSources--;
        }
    }

    return Status;
}

NTSTATUS
GcKmBaseDisplayController::UpdateActiveVidPnPresentPath(
    IN_CONST_PDXGKARG_UPDATEACTIVEVIDPNPRESENTPATH_CONST    pUpdateActiveVidPnPresentPath)
{
    auto    pRtPathInfo = &pUpdateActiveVidPnPresentPath->VidPnPresentPathInfo;

    for (UINT ClonedTarget = 0; ClonedTarget < GC_KM_MAX_CLONES; ClonedTarget++)
    {
        auto pActivePathInfo = &m_Paths[pRtPathInfo->VidPnSourceId][ClonedTarget];
        if (pRtPathInfo->VidPnTargetId != pActivePathInfo->m_TargetId)
        {
            continue;
        }

        return pActivePathInfo->m_pDisplayPipeline->UpdateActiveVidPnPresentPath(pUpdateActiveVidPnPresentPath);
    }

    return STATUS_INVALID_PARAMETER;
}

NTSTATUS
GcKmBaseDisplayController::RecommendMonitorModes(
    IN_CONST_PDXGKARG_RECOMMENDMONITORMODES_CONST   pRecommendMonitorMode)
{
    GcKmBaseDisplay* pPipeline = FindDisplayPipeline(pRecommendMonitorMode->VideoPresentTargetId);

    return pPipeline->RecommendMonitorModes(pRecommendMonitorMode);
}

NTSTATUS
GcKmBaseDisplayController::GetScanLine(
    INOUT_PDXGKARG_GETSCANLINE  pGetScanLine)
{
    GcKmDisplay* pDisplayPipeline = FindDisplayPipeline(pGetScanLine->VidPnTargetId);

    return pDisplayPipeline->GetScanLine(pGetScanLine);
}

BOOLEAN
GcKmBaseDisplayController::InterruptRoutine(
    UINT    MessageNumber)
{
    BOOLEAN bRet = FALSE;

    for (UINT i = 0; i < m_NumPipelines; i++)
    {
        if (m_Pipelines[i].m_bInterruptForStarting ||
            m_Pipelines[i].m_bStarted)
        {
            bRet |= m_Pipelines[i].m_pDisplayPipeline->InterruptRoutine(MessageNumber);
        }
    }

    return bRet;
}

NTSTATUS
GcKmBaseDisplayController::ControlInterrupt(
    IN_CONST_DXGK_INTERRUPT_TYPE    InterruptType,
    IN_BOOLEAN                      EnableInterrupt)
{
    for (UINT i = 0; i < m_NumPipelines; i++) {
        if (m_Pipelines[i].m_pDisplayPipeline) {
            m_Pipelines[i].m_pDisplayPipeline->ControlInterrupt(InterruptType, EnableInterrupt);
        }
    }

    return STATUS_SUCCESS;
}

NTSTATUS
GcKmBaseDisplayController::QueryVidPnHWCapability(
    INOUT_PDXGKARG_QUERYVIDPNHWCAPABILITY   pVidPnHWCaps)
{
    GcKmBaseDisplay* pPipeline = FindDisplayPipeline(pVidPnHWCaps->TargetId);

    return pPipeline->QueryVidPnHWCapability(pVidPnHWCaps);
}

//
// TODO: Supporting multi-mon in Display Only mode requires driver to allocate the frame buffer
//

NTSTATUS
GcKmBaseDisplayController::PresentDisplayOnly(
    IN_CONST_PDXGKARG_PRESENT_DISPLAYONLY  pPresentDisplayOnly)
{
    NTSTATUS    Status = STATUS_SUCCESS;

    for (UINT ClonedTarget = 0; ClonedTarget < GC_KM_MAX_CLONES; ClonedTarget++)
    {
        auto pActivePathInfo = &m_Paths[pPresentDisplayOnly->VidPnSourceId][ClonedTarget];
        if (0 == pActivePathInfo->m_TargetId)
        {
            continue;
        }

        auto SubStatus = pActivePathInfo->m_pDisplayPipeline->PresentDisplayOnly(pPresentDisplayOnly);
        if (!NT_SUCCESS(SubStatus))
        {
            Status = SubStatus;
        }
    }

    return Status;
}

NTSTATUS
GcKmBaseDisplayController::StopDeviceAndReleasePostDisplayOwnership(
    _In_ D3DDDI_VIDEO_PRESENT_TARGET_ID TargetId,
    _Out_ PDXGK_DISPLAY_INFORMATION     pDisplayInfo)
{
    GcKmDisplay* pPOSTdisplayPipeline = FindDisplayPipeline(TargetId);

    for (UINT i = 0; i < m_NumPipelines; i++) {
        if (m_Pipelines[i].m_pDisplayPipeline == nullptr) {
            continue;
        }
        if (m_Pipelines[i].m_pDisplayPipeline == pPOSTdisplayPipeline) {
            m_Pipelines[i].m_pDisplayPipeline->StopDeviceAndReleasePostDisplayOwnership(TargetId, pDisplayInfo);
            m_Pipelines[i].m_pDisplayPipeline->HwStop(pDisplayInfo, TRUE);
        }
        else {
            m_Pipelines[i].m_pDisplayPipeline->HwStop(pDisplayInfo, FALSE);
        }
        m_Pipelines[i].m_bStarted = false;
    }

    return STATUS_SUCCESS;
}

NTSTATUS
GcKmBaseDisplayController::SetVidPnSourceAddressWithMultiPlaneOverlay3(
    IN_OUT_PDXGKARG_SETVIDPNSOURCEADDRESSWITHMULTIPLANEOVERLAY3 pSetVidPnSourceAddressWithMpo3)
{
    NTSTATUS    Status = STATUS_SUCCESS;

    for (UINT ClonedTarget = 0; ClonedTarget < GC_KM_MAX_CLONES; ClonedTarget++)
    {
        auto pActivePathInfo = &m_Paths[pSetVidPnSourceAddressWithMpo3->VidPnSourceId][ClonedTarget];
        if (0 == pActivePathInfo->m_TargetId)
        {
            continue;
        }

        auto SubStatus = pActivePathInfo->m_pDisplayPipeline->SetVidPnSourceAddressWithMultiPlaneOverlay3(pSetVidPnSourceAddressWithMpo3);
        if (!NT_SUCCESS(SubStatus))
        {
            Status = SubStatus;
        }
    }

    return Status;
}

NTSTATUS
GcKmBaseDisplayController::UpdateMonitorLinkInfo(
    INOUT_PDXGKARG_UPDATEMONITORLINKINFO    pUpdateMonitorLinkInfo)
{
    GcKmDisplay*    pDisplayPipeline = FindDisplayPipeline(pUpdateMonitorLinkInfo->VideoPresentTargetId);

    return pDisplayPipeline->UpdateMonitorLinkInfo(pUpdateMonitorLinkInfo);
}

NTSTATUS
GcKmBaseDisplayController::GetChildContainerId(
    _In_ ULONG  ChildUid,
    _Inout_ PDXGK_CHILD_CONTAINER_ID    pChildChildContainId)
{
    GcKmDisplay* pDisplayPipeline = FindDisplayPipeline(ChildUid);

    return pDisplayPipeline->GetChildContainerId(ChildUid, pChildChildContainId);
}

//
// Display controller member functions
//

NTSTATUS
GcKmBaseDisplayController::RegisterDisplayPipeline(
    GcKmBaseDisplay*    pDisplay,
    BOOLEAN             bInterruptForStarting)
{
    m_Pipelines[m_NumPipelines].m_pDisplayPipeline = pDisplay;
    m_Pipelines[m_NumPipelines].m_bInterruptForStarting = bInterruptForStarting;

    m_NumPipelines++;

    return STATUS_SUCCESS;
}

