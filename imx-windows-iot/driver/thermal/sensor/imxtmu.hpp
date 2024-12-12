// Copyright (c) Microsoft Corporation. All rights reserved.
// Copyright 2020, 2022 NXP
// Licensed under the MIT License.
//
// Module Name:
//    imxtmu.h
// Abstract:
//    This is the header file for the TMU sensor driver.
//

#ifndef __IMXTMU_H__
#define __IMXTMU_H__

#pragma once

#include <ntddk.h>
#include <wdf.h>
#include <ntstrsafe.h>
#include <initguid.h>
#include <wdmguid.h>
#include <poclass.h>
#include "imxtmuhw.h"

#define KELVIN273   273

enum TC_PLL_TYPE {
    TC_IMX8MQ_FRAC_PLL = 0x00,
    TC_IMX8MP_FRAC_PLL = 0x01,
    TC_UNKNOWN_PLL_TYPE = 0xFF
};

const CHAR* TC_PLL_NAMES[] = {
    "imx8mqFracPll",
    "imx8mpFracPll"
};

typedef struct _FDO_DATA_ {
    WDFQUEUE    PendingRequestQueue;
    WDFSPINLOCK QueueLock;
    WDFWORKITEM InterruptWorker;
    WDFINTERRUPT WdfInterrupt;
    WDFDEVICE WdfDevice;
    TC_PLL_TYPE         PllType;

    PHYSICAL_ADDRESS RegistersPhysicalAddress;
    ULONG RegistersIoSize;
    //this could perhaps be resolved by Union in case more SoCs with different TMUs are added 
    IMX8MQTMU_REGISTERS* Registers8MQPtr;
    IMX8MPTMU_REGISTERS* Registers8MPPtr;
} FDO_DATA, *PFDO_DATA;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(FDO_DATA, GetDeviceExtension);

typedef struct {
    LARGE_INTEGER ExpirationTime;
    ULONG HighTemperature;
    ULONG LowTemperature;
} READ_REQUEST_CONTEXT, *PREAD_REQUEST_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE(READ_REQUEST_CONTEXT);

#endif