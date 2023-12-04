/* Copyright (c) Microsoft Corporation.
 * Copyright 2023 NXP
   Licensed under the MIT License. */

#pragma once

#include "GcKmdImx8Display.h"
#include "GcKmdBaseDisplayController.h"
#include "GcKmdImx8qxpDisplay.h"


class GcKmImx8qxpDisplayController : public GcKmBaseDisplayController
{
public:

    GcKmImx8qxpDisplayController(
        DXGKRNL_INTERFACE*  pDxgkInterface);

    ~GcKmImx8qxpDisplayController();

private:

    static const INT QXP_MAX_MULTIPLE_DISPLAYS = 3;
    static const INT QXP_MAX_LVDS_DISPLAYS = 2;
    GcKmImx8qxpDisplay* m_p_LvdsDisplay[QXP_MAX_LVDS_DISPLAYS];

    struct Imx8qxpDisplayPDEVs m_dpu0_pdevs;
    struct platform_device m_dpu0_prg_pdev[PRG_CNT];
    struct platform_device m_dpu0_dprc_pdev[DPRC_CNT];
    struct platform_device m_dpu0_dpu_pdev;
    struct platform_device m_dpu0_client_devices[CLIENT_DEVICE_CNT];
    struct platform_device m_dpu0_irqsteer_pdev;
    UINT m_dpu0_refCount;

};
