/** @file
*
*  Copyright (c) 2018 Microsoft Corporation. All rights reserved.
*  Copyright 2020, 2022-2023 NXP
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

#ifndef __IMX_DISPLAY_H__
#define __IMX_DISPLAY_H__

#define IMX_EDID_MIN_SIZE       128
#define IMX_EDID_I2C_ADDRESS    0x50

// The first DTD is the preferred timing, refer to 3.1 VESA EDID spec.
#define IMX_EDID_DTD_1_OFFSET   0x36
#define IMX_EDID_DTD_2_OFFSET   0x48
#define IMX_EDID_DTD_3_OFFSET   0x5A
#define IMX_EDID_DTD_4_OFFSET   0x6C
#define IMX_EDID_DTD_SIZE       18

#define IMX_DISPLAY_TIMING_NO_FLAGS 0

/* Display interfaces */
typedef enum {
 imxNativeHdmi = 0,
 imxMipiDsi = 1, /* If more mipi-dsi ports, imxMipiDsi relates to port with index 0 */
 imxLvds0 = 2,
 imxLvds1 = 3,
 imxLvds0dual = 4,
 imxRgb = 5,
 imxMipiDsi1 = 6,
 imxInterfaceUnknown,
} imxDisplayInterfaceType;

typedef enum {
  PIXEL_FORMAT_ARGB32,
  PIXEL_FORMAT_BGRA32,
} IMX_PIXEL_FORMAT;

typedef enum {
  DISP_TIMING_INVERT_HSYNC,
  DISP_TIMING_INVERT_VSYNC,
  DISP_TIMING_INVERT_DATA_EN,
  DISP_TIMING_INVERT_CLOCK,
} DISP_TIMING_FLAGS;

typedef struct _IMX_DISPLAY_TIMING {
  UINT32 PixelClock;
  UINT32 HActive;
  UINT32 HBlank;
  UINT32 VActive;
  UINT32 VBlank;
  UINT32 HSync;
  UINT32 VSync;
  UINT32 HSyncOffset;
  UINT32 VSyncOffset;
  UINT32 HImageSize;
  UINT32 VImageSize;
  UINT32 HBorder;
  UINT32 VBorder;
  UINT32 EdidFlags;
  UINT32 Flags;
  UINT32 PixelRepetition;
  UINT32 Bpp;
  IMX_PIXEL_FORMAT PixelFormat;
} IMX_DISPLAY_TIMING;

typedef struct _IMX_DETAILED_TIMING_DESCRIPTOR {
  UINT8 PixelClock[2];
  UINT8 HActive;
  UINT8 HBlank;
  UINT8 HActiveBlank;
  UINT8 VActive;
  UINT8 VBlank;
  UINT8 VActiveBlank;
  UINT8 HSyncOffset;
  UINT8 HSyncWidth;
  UINT8 VSyncOffsetWidth;
  UINT8 HVOffsetWidth;
  UINT8 HImageSize;
  UINT8 VImageSize;
  UINT8 HVImageSize;
  UINT8 HBorder;
  UINT8 VBorder;
  UINT8 EdidFlags;
} IMX_DETAILED_TIMING_DESCRIPTOR;

/**
  Convert detailed timing descriptor to display timing format

  @param[in]    DTDPtr            Pointer to detailed timing descriptor.
  @param[out]   DisplayTimingPtr  Pointer to display timing structure.

  @retval   EFI_SUCCESS   Detailed timing descriptor data was converted.

**/
EFI_STATUS
ImxConvertDTDToDisplayTiming (
  IN IMX_DETAILED_TIMING_DESCRIPTOR   *DTDPtr,
  OUT IMX_DISPLAY_TIMING              *DisplayTimingPtr
  );

/**
  Debug dump of Display Timing structure

  @param[in]    DisplayTimingNamePtr  Name of display timing structure.
  @param[in]    DisplayTimingPtr      Pointer to display timing structure.
**/
VOID
ImxPrintDisplayTiming (
  IN CHAR8                *DisplayTimingNamePtr,
  IN IMX_DISPLAY_TIMING   *DisplayTimingPtr
  );

/**
  Check if EDID is valid

  @param[in]    EdidDataPtr  Pointer to EDID data.

  @retval   EFI_SUCCESS             EDID data is a valid EDID.
  @retval   EFI_INVALID_PARAMETER   EDID data is invalid.

**/
EFI_STATUS
ImxValidateEdidData (
  IN UINT8 *EdidDataPtr
  );

#endif // __IMX_DISPLAY_H__
