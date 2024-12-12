/** @file
*
*  Copyright 2020, 2024 NXP
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

Device (TC) {
    Name (_HID, "NXP0115")
    Name (_UID, 1)

    Name (_CRS, ResourceTemplate () {
        // CCM_ANALOG base address
        MEMORY32FIXED (ReadWrite, 0x30360000, 0x10000, )

        // PWM_LED (H: 0.9V; L:1.0V) - GPIO1_IO13
        GpioIO (Shared, PullNone, 0, 0, IoRestrictionNone, "\\_SB.GPIO", 0, ResourceConsumer, , ) { 13 } // 0 * 32 + 13
    })

    Name (_DSD, Package () {
        ToUUID("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
        Package () {
            Package (2) {"ArmPll",
                Package () {
                    Package (2) {"PllType",  "imx8mpFracPll"},
                    Package (2) {"PllRefClock_hz",  24000000},
                    Package (2) {"PllFreqMax_hz", 1800000000},
                    Package (2) {"PllFreqMin_hz",  800000000}
                }
            },
            Package (2) {"CoreOppTable",
                Package () {
                    // Cortex-Ax: freq: Hz , power: uV
                    Package (2) { 800000000,  850000},
                    Package (2) {1000000000,  850000},
                    Package (2) {1200000000,  850000},
                    Package (2) {1400000000,  950000},
                    Package (2) {1600000000,  950000},
                    //uncomment for industrial applications
                    Package (2) {1800000000, 1000000}
                }
            }
        }
    })
}

ThermalZone(TZ0) {
    Name (_HID, "NXP010B")
    Name (_UID, 1)
    Name (_PSV, 3480)
    Name (_CRT, 3630)
    Name (_TSP, 30)
    Name (_TC1, 4)
    Name (_TC2, 2)
    Name (_TZD, Package () { \_SB.TC } )
    Name (_HRV, 0x0)

    Method (_STA) {
        Return (0xf)
    }

    Name (_CRS, ResourceTemplate () {
        MEMORY32FIXED (ReadWrite, 0x30260000, 0x10000, )
        Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive) { 81 }
    })

   Name (_DSD, Package () {
    ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
      Package () {
        Package (2) {"SocType", "imx8mp"}
      }
    })
}
