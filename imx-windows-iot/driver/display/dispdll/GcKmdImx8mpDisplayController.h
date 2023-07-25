/* Copyright (c) Microsoft Corporation.
 * Copyright 2023 NXP
   Licensed under the MIT License. */

#pragma once

#include "GcKmdBaseDisplayController.h"
#include "GcKmdImx8mpDisplay.h"
#include "GcKmdImx8mpHdmiDisplay.h"


class GcKmImx8mpDisplayController : public GcKmBaseDisplayController
{
public:

    GcKmImx8mpDisplayController(
        DXGKRNL_INTERFACE*  pDxgkInterface);

private:

    GcKmImx8mpDisplay       m_LvdsDisplay;
    GcKmImx8mpHdmiDisplay   m_HdmiDisplay;
};
