/** @file
*
*  Copyright (c) 2018 Microsoft Corporation. All rights reserved.
*  Copyright 2019 NXP
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

// Description: Graphics Processing Unit (GPU)
Device (GPU0)
{
  Name (_HID, "VERI7005")
  Name (_CID, "VERI7005")
  Name (_UID, 0)

  Method (_STA) {
    Return (0xf)
  }

  Name (_CRS, ResourceTemplate () {
    // First memory block must be GPU registers area
    MEMORY32FIXED( ReadWrite, 0x38000000, 0x10000, )
    // Second memory block must be Framebuffer allocated.
    MEMORY32FIXED( ReadWrite, FixedPcdGet32(PcdArmLcdDdrFrameBufferBase), FixedPcdGet32(PcdArmLcdDdrFrameBufferSize), )
    // LCDIF reg
    MEMORY32FIXED (ReadWrite, 0x32E00000, 0x10000, )
    // MIPI-DSI reg
    MEMORY32FIXED (ReadWrite, 0x32E10000, 0x10000, )
    // First must be GPU interrupt
    Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive) { 35 }
    // LCDIF interrupt
    Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive) { 37 }
    // TODO: MIPI-DSI I2C interface for the HDMI converter.
  })

  Method (_ADR, 0) {
    return(0x0100)             // device ID for this CRT
  }

  Method (_BCL, 0) {
                             // List of supported brightness levels
   Return (Package(7){
      80,                    // level when machine has full power
      50,                    // level when machine is on batteries
                             // other supported levels:
      20, 40, 60, 80, 100})
  }
  /* ADP5585 SlaveAddress 0x34 I2C2*/
  Scope(\_SB.I2C2)                  //OpRegion declaration must appear under the controller
  {
    OperationRegion(TOP1, GenericSerialBus, 0x2F, 5)  
    Field(TOP1, BufferAcc, NoLock, Preserve)
    {
      /* Connection(I2CSerialBusV2(0x34,,400000,,"\\_SB.I2C2",,,,,)),*/
      Connection(I2CSerialBus(0x34, ControllerInitiated, 400000, AddressingMode7Bit, "\\_SB.I2C2")),
        //Connection to the controller for the following field accesses
      AccessAs(BufferAcc, AttribByte),  //AccessProtocol for the following field(s)
      OFTL, 8,  // PWM_OFFT_LOW Register  0x2F
      OFTH, 8,  // PWM_OFFT_HIGH Register 0x30
      ONTL, 8,  // PWM_ONT_LOW Register   0x31
      ONTH, 8,  // PWM_ONT_HIGH Register  0x32
      PWMC, 8   // PWM_CFG Register       0x33
    }                                 // End of Field
  } 

  Method (_BCM, 1) {                    // Set the requested level 

    Name(BUFF, Buffer(4){})             // Create SerialBus data buffer as BUFF
    CreateByteField(BUFF, 0x00, STAT)   // STAT = Status (Byte)
    CreateWordField(BUFF, 0x02, DATA)   // DATA = Data (Byte)

      // Verify that SPB OpRegion is available for this access
    /*If(LNotEqual(\_SB.I2C2.AVBL, 1))
    {
       Return(0)
    }*/

    Name(BUF2,Buffer(0x05){})         // Create buffer to hold the expander PWM structure
                                      // as BUF2
    CreateWordField(BUF2, 0x0,OFFT)   // PWM OFF time
    CreateWordField(BUF2, 0x5,ONT)    // PWM ON time
    CreateByteField(BUF2, 0x4,PWMC)   // PWM config register

    CreateByteField(BUF2, 0x0,OFTL)   // PWM_OFFT_LOW Register 
    CreateByteField(BUF2, 0x1,OFTH)   // PWM_OFFT_HIGH Register
    CreateByteField(BUF2, 0x2,ONTL)   // PWM_ONT_LOW Register  
    CreateByteField(BUF2, 0x3,ONTH)   // PWM_ONT_HIGH Register 

    // set PWM ON and OFF time according given level
    local0 = 1000  // set PWM period [2us steps] according ADP5585 manual
    local1 = (arg0 * local0) / 100
    ONT = local1
    OFFT = local0 - local1
    PWMC = 1   /* PWM_EN bit 0 to Enable PWM generator */ 
                                        // Store each input member into the hardware,
                                        // and set the transaction status into BUFF
    //Store(Store(OFTL, \\_SB.I2C2.OFTL), BUFF)
    If(LEqual(STAT, 0x00))              // transaction was *NOT* successful
    {
      Return(0xFFFFFFFF)
    }
    //Store(Store(OFTH, \\_SB.I2C2.OFTH), BUFF)
    If(LEqual(STAT, 0x00))              // Transaction was \_NOT_successful
    {
      Return(0xFFFFFFFF)
    } 
    //Store(Store(ONTL, \\_SB.I2C2.ONTL), BUFF)
    If(LEqual(STAT, 0x00))              // Transaction was \_NOT_successful
    {
      Return(0xFFFFFFFF)
    } 
    //Store(Store(ONTH, \\_SB.I2C2.ONTH), BUFF)
    If(LEqual(STAT, 0x00))              // Transaction was \_NOT_successful
    {
      Return(0xFFFFFFFF)
    } 
    //Store(Store(PWMC, \\_SB.I2C2.PWMC), BUFF)
    If(LEqual(STAT, 0x00))              // Transaction was \_NOT_successful
    {
      Return(0xFFFFFFFF)
    } 
  }
 
  Method (_BQC, 0) {
    return (60)
  }
  Name(_DEP, Package() {\_SB.I2C2})  // Identify the OpRegion dependency for
                                        // this device
}
