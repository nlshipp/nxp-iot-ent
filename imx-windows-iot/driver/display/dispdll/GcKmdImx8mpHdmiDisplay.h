/* Copyright (c) Microsoft Corporation.
 * Copyright 2022-2023 NXP
   Licensed under the MIT License. */

#pragma once

#include "GcKmdBaseDisplay.h"
#include "GcKmdDwHdmiTransmitter.h"

extern "C" {
#include "linux/platform_device.h"
#include "clk/clk_imx8mp.h"
}

class GcKmImx8mpHdmiDisplay : public GcKmBaseDisplay
{
public:

    GcKmImx8mpHdmiDisplay()
    {
        m_FrontBufferSegmentOffset.QuadPart = 0L;
        m_InterruptData = {};
        m_pTransmitter = &m_DwHdmiTransmitter;
    }

    virtual NTSTATUS HwStart(DXGKRNL_INTERFACE* pDxgkInterface) override;

    virtual NTSTATUS HwStop(
        DXGK_DISPLAY_INFORMATION   *pFwDisplayInfo, BOOLEAN DoCommitFwFb) override;

    virtual void HwStopScanning(D3DDDI_VIDEO_PRESENT_TARGET_ID TargetId) override;

    virtual NTSTATUS QueryInterface(
        IN_PQUERY_INTERFACE pQueryInterface) override;

    virtual void HwSetPowerState(
        IN_ULONG                DeviceUid,
        IN_DEVICE_POWER_STATE   DevicePowerState,
        IN_POWER_ACTION         ActionType) override;

    virtual NTSTATUS SetVidPnSourceAddress(
        IN_CONST_PDXGKARG_SETVIDPNSOURCEADDRESS pSetVidPnSourceAddress) override;

    virtual NTSTATUS HwCommitVidPn(
        const D3DKMDT_VIDPN_SOURCE_MODE* pSourceMode,
        const D3DKMDT_VIDPN_TARGET_MODE* pTargetMode,
        IN_CONST_PDXGKARG_COMMITVIDPN_CONST pCommitVidPn) override;

    virtual NTSTATUS RecommendMonitorModes(
        IN_CONST_PDXGKARG_RECOMMENDMONITORMODES_CONST   pRecommendMonitorMode) override;

    virtual BOOLEAN InterruptRoutine(
        UINT    MessageNumber) override;

    virtual NTSTATUS ControlInterrupt(
        IN_CONST_DXGK_INTERRUPT_TYPE    InterruptType,
        IN_BOOLEAN  EnableInterrupt) override;

    virtual NTSTATUS SetVidPnSourceAddressWithMultiPlaneOverlay3(
        IN_OUT_PDXGKARG_SETVIDPNSOURCEADDRESSWITHMULTIPLANEOVERLAY3 pSetVidPnSourceAddressWithMpo3) override;

    virtual bool SupportStandardModes() override
    {
        return true;
    }

private:

    static int AudioSetHwParams(
        IN GcKmImx8mpHdmiDisplay* pThis,
        IN struct imx8mp_hdmi_audio_params* pAudioParams);

    static int AudioMuteStream(
        IN GcKmImx8mpHdmiDisplay* pThis,
        IN BOOLEAN  Enable);

    static int AudioGetContainerId(
        IN GcKmImx8mpHdmiDisplay* pThis,
        OUT DXGK_CHILD_CONTAINER_ID* pContainerId);

    DwHdmiTransmitter m_DwHdmiTransmitter;

    DXGKARGCB_NOTIFY_INTERRUPT_DATA m_InterruptData;

    PHYSICAL_ADDRESS    m_FrontBufferSegmentOffset;

    struct platform_device lcdif_pdev;
    struct platform_device lcdif_crtc_pdev;
    imx8mp_clk_device *clk_tree;
    struct platform_device irqsteer_pdev;

};

