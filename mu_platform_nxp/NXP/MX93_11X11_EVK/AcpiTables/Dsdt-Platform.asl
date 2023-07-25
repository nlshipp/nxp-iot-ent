/*
*  iMX9 Processor Devices
*
*  Copyright (c) 2018 Microsoft Corporation. All rights reserved.
*  Copyright 2022-2023 NXP
*
*  This program and the accompanying materials
*  are licensed and made available under the terms and conditions of the BSD License
*  which accompanies this distribution.  The full text of the license may be found at
*  http://opensource.org/licenses/bsd-license.php
*
*  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
*  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
*
**/

OperationRegion(FUSE,SystemMemory,0x47518000,0x1000)
Field(FUSE, AnyAcc, Nolock, Preserve)
{
  Offset(0x4EC),
  MC15, 8,          // NET1 MAC address bytes
  MC14, 8,          // NET1 MAC address bytes
  MC13, 8,          // NET1 MAC address bytes
  MC12, 8,          // NET1 MAC address bytes

  Offset(0x4F0),
  MC11, 8,          // NET1 MAC address bytes
  MC10, 8,          // NET1 MAC address bytes
  MC21, 8,          // NET2 MAC address bytes
  MC20, 8,          // NET2 MAC address bytes

  Offset(0x4F4),
  MC25, 8,          // NET2 MAC address bytes,
  MC24, 8,          // NET2 MAC address bytes,
  MC23, 8,          // NET2 MAC address bytes,
  MC22, 8,          // NET2 MAC address bytes,
}

//
// Description: This is a Processor #0 Device
//
Device (CPU0)
{
    Name (_HID, "ACPI0007")
    Name (_UID, 0x0)
    Method (_STA)
    {
        Return(0xf)
    }
}

//
// Description: This is a Processor #1 Device
//
#if FixedPcdGet32(PcdCoreCount) > 1
Device (CPU1)
{
    Name (_HID, "ACPI0007")
    Name (_UID, 0x1)
    Method (_STA)
    {
        Return(0xf)
    }
}
#endif


// Power Engine Plugin
Device (PEP0)
{
  Name (_HID, "NXP0122")
  Name (_UID, 0x0)

  Method (_STA) {
    Return (0xf)
  }
}

