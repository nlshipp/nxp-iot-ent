/** @file
*
*  Copyright (c) 2018, Microsoft Corporation. All rights reserved.
*  Copyright 2020 NXP
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

#include <Library/ArmPlatformLib.h>
#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <Library/PcdLib.h>
#include <Library/IoLib.h>
#include <Library/MemoryAllocationLib.h>

#include "iMX8.h"

#define MEMORY_ATTRIBUTES_PCDCACHEENABLE    -1

ARM_MEMORY_REGION_DESCRIPTOR iMX8MemoryDescriptor[] =
{
  // Main memory
  {
    FixedPcdGet64 (PcdSystemMemoryBase),
    FixedPcdGet64 (PcdSystemMemoryBase),
    FixedPcdGet64 (PcdSystemMemorySize),
    MEMORY_ATTRIBUTES_PCDCACHEENABLE,
  },
#ifndef CONFIG_HEADLESS
  // Frame buffer
  {
    FixedPcdGet64 (PcdArmLcdDdrFrameBufferBase),
    FixedPcdGet64 (PcdArmLcdDdrFrameBufferBase),
    FixedPcdGet64 (PcdArmLcdDdrFrameBufferSize),
    ARM_MEMORY_REGION_ATTRIBUTE_UNCACHED_UNBUFFERED,
  },
#endif // CONFIG_HEADLESS
#ifdef CONFIG_OPTEE
  {
    FixedPcdGet64 (PcdTrustZoneSharedMemoryBase),
    FixedPcdGet64 (PcdTrustZoneSharedMemoryBase),
    FixedPcdGet64 (PcdTrustZoneSharedMemorySize),
    MEMORY_ATTRIBUTES_PCDCACHEENABLE,
  },
#elif FixedPcdGet32(PcdBank1MemorySize) > 0
  // Main memory behind FrameBuffer
  {
    FixedPcdGet64 (PcdBank1MemoryBase),
    FixedPcdGet64 (PcdBank1MemoryBase),
    FixedPcdGet64 (PcdBank1MemorySize),
    MEMORY_ATTRIBUTES_PCDCACHEENABLE,
  },
#endif //<-/ CONFIG_OPTEE

/* Flexible Serial Peripheral Interface */
#define FLEX_SPI_REG_REGISTER_PHYSICAL       0x28000000
#define FLEX_SPI_REG_REGISTER_LENGTH         0x26310000
  {
    FLEX_SPI_REG_REGISTER_PHYSICAL,
    FLEX_SPI_REG_REGISTER_PHYSICAL,
    FLEX_SPI_REG_REGISTER_LENGTH,
    ARM_MEMORY_REGION_ATTRIBUTE_DEVICE,
  },
  {
    //
    // End of table
    //
    0,
    0,
    0,
    0,
  },
};

#define MAX_VIRTUAL_MEMORY_MAP_DESCRIPTORS (sizeof(iMX8MemoryDescriptor) / sizeof(iMX8MemoryDescriptor[0]))

// DDR attributes
#define DDR_ATTRIBUTES_CACHED           ARM_MEMORY_REGION_ATTRIBUTE_WRITE_BACK
#define DDR_ATTRIBUTES_UNCACHED         ARM_MEMORY_REGION_ATTRIBUTE_UNCACHED_UNBUFFERED

/**
  Return the Virtual Memory Map of your platform

  This Virtual Memory Map is used by MemoryInitPei Module to initialize the MMU on your platform.

  @param[out]   VirtualMemoryMap    Array of ARM_MEMORY_REGION_DESCRIPTOR describing a Physical-to-
                                    Virtual Memory mapping. This array must be ended by a zero-filled
                                    entry

**/
VOID
ArmPlatformGetVirtualMemoryMap (
  IN ARM_MEMORY_REGION_DESCRIPTOR **VirtualMemoryMap
  )
{
  ARM_MEMORY_REGION_ATTRIBUTES cacheAttributes;
  UINTN index;
  ARM_MEMORY_REGION_DESCRIPTOR *virtualMemoryTable;
  EFI_RESOURCE_ATTRIBUTE_TYPE  ResourceAttributes;

  ASSERT (VirtualMemoryMap != NULL);

  DEBUG ((EFI_D_VERBOSE, "Enter: ArmPlatformGetVirtualMemoryMap\n"));

  ResourceAttributes = (
                       EFI_RESOURCE_ATTRIBUTE_PRESENT |
                       EFI_RESOURCE_ATTRIBUTE_INITIALIZED |
                       EFI_RESOURCE_ATTRIBUTE_WRITE_COMBINEABLE |
                       EFI_RESOURCE_ATTRIBUTE_WRITE_THROUGH_CACHEABLE |
                       EFI_RESOURCE_ATTRIBUTE_WRITE_BACK_CACHEABLE |
                       EFI_RESOURCE_ATTRIBUTE_TESTED
                       );

  virtualMemoryTable = (ARM_MEMORY_REGION_DESCRIPTOR *)AllocatePages (EFI_SIZE_TO_PAGES (sizeof(ARM_MEMORY_REGION_DESCRIPTOR) * MAX_VIRTUAL_MEMORY_MAP_DESCRIPTORS));
  if (virtualMemoryTable == NULL) {
    return;
  }

  cacheAttributes = ARM_MEMORY_REGION_ATTRIBUTE_WRITE_BACK;

  DEBUG ((EFI_D_VERBOSE, "cacheAttributes=0x%d\n", cacheAttributes));

  for (index = 0; index < MAX_VIRTUAL_MEMORY_MAP_DESCRIPTORS; index++) {

    virtualMemoryTable[index].PhysicalBase = iMX8MemoryDescriptor[index].PhysicalBase;
    virtualMemoryTable[index].VirtualBase = iMX8MemoryDescriptor[index].VirtualBase;
    virtualMemoryTable[index].Length = iMX8MemoryDescriptor[index].Length;

    if (iMX8MemoryDescriptor[index].Attributes == MEMORY_ATTRIBUTES_PCDCACHEENABLE) {
      virtualMemoryTable[index].Attributes = cacheAttributes;
    } else {
      virtualMemoryTable[index].Attributes = iMX8MemoryDescriptor[index].Attributes;
    }
  }

  ASSERT ((index) <= MAX_VIRTUAL_MEMORY_MAP_DESCRIPTORS);


  // Reserve Global Data area
  BuildResourceDescriptorHob (
    EFI_RESOURCE_MEMORY_RESERVED,
    EFI_RESOURCE_ATTRIBUTE_PRESENT |
    EFI_RESOURCE_ATTRIBUTE_INITIALIZED |
    EFI_RESOURCE_ATTRIBUTE_TESTED,
    FixedPcdGet32 (PcdGlobalDataBaseAddress),
    FixedPcdGet32 (PcdGlobalDataSize)
    );

  // Reserve TPM2 Control Area
  BuildResourceDescriptorHob (
    EFI_RESOURCE_MEMORY_RESERVED,
    EFI_RESOURCE_ATTRIBUTE_PRESENT |
    EFI_RESOURCE_ATTRIBUTE_INITIALIZED |
    EFI_RESOURCE_ATTRIBUTE_TESTED,
    FixedPcdGet32 (PcdTpm2AcpiBufferBase),
    FixedPcdGet32 (PcdTpm2AcpiBufferSize)
    );

  // Declare main system memory
  BuildResourceDescriptorHob (
    EFI_RESOURCE_SYSTEM_MEMORY,
    ResourceAttributes,
    FixedPcdGet64 (PcdSystemMemoryBase),
    FixedPcdGet64 (PcdSystemMemorySize)
    );

#ifndef CONFIG_HEADLESS
  // Reserve frame buffer
  BuildResourceDescriptorHob (
    EFI_RESOURCE_MEMORY_RESERVED,
    EFI_RESOURCE_ATTRIBUTE_PRESENT |
    EFI_RESOURCE_ATTRIBUTE_INITIALIZED |
    EFI_RESOURCE_ATTRIBUTE_TESTED,
    FixedPcdGet64 (PcdArmLcdDdrFrameBufferBase),
    FixedPcdGet64 (PcdArmLcdDdrFrameBufferSize));
#endif

#ifdef CONFIG_OPTEE
  // Reserve OP-TEE private memory
  BuildResourceDescriptorHob (
    EFI_RESOURCE_MEMORY_RESERVED,
    EFI_RESOURCE_ATTRIBUTE_PRESENT |
    EFI_RESOURCE_ATTRIBUTE_INITIALIZED |
    EFI_RESOURCE_ATTRIBUTE_TESTED,
    FixedPcdGet32 (PcdTrustZonePrivateMemoryBase),
    FixedPcdGet32 (PcdTrustZonePrivateMemorySize)
    );
#elif FixedPcdGet64 (PcdBank1MemorySize) > 0
  // Declare system memory behind FrameBuffer
  BuildResourceDescriptorHob (
    EFI_RESOURCE_SYSTEM_MEMORY,
    ResourceAttributes,
    FixedPcdGet64 (PcdBank1MemoryBase),
    FixedPcdGet64 (PcdBank1MemorySize)
    );
#endif

  *VirtualMemoryMap = virtualMemoryTable;
}
