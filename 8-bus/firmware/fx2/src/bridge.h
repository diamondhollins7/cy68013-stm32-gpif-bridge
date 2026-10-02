/*
 * bridge.h - FX2LP <-> STM32F103 8-bit GPIF bridge: shared definitions
 *
 * Built on the Cypress EZ-USB FX2LP firmware framework (fw.c, EZUSB.LIB,
 * USBJmpTb.OBJ, fx2.h, fx2regs.h, syncdly.h from the CY3684 DVK).
 */
#ifndef BRIDGE_H
#define BRIDGE_H

#include "fx2.h"
#include "fx2regs.h"
#include "syncdly.h"

#define FW_VERSION_MAJOR    2       /* 2.x = 8-bit bus version */
#define FW_VERSION_MINOR    0

/* ---- Vendor requests (also used by host/CyBridgeTest) ----------------- */
#define VR_GET_STATUS       0xB0    /* IN, 8 bytes bridge status                */
#define VR_RESET_BRIDGE     0xB1    /* OUT, wValue bit0 = 1 -> also reset STM32 */

/* ---- Port A usage ------------------------------------------------------
 * PA0 : STM32 NRST (open-drain emulation: IOA.0 = 0, toggle OEA.0)
 * PA1 : LED D2, active low
 * PA2..PA7 : unused, driven low
 * Port D is plain GPIO in 8-bit mode (all WORDWIDE = 0): driven low.
 */
#define PA_STM_RST          0x01
#define PA_LED              0x02

/* ---- GPIF signal mapping (see docs/GPIF_interface.md) ----------------
 * FD[7:0] (port B) <-> STM32 PE[7:0]
 * CTL0 -> nWR    CTL1 -> nRD    CTL2 -> nSYNC
 * RDY0 <- ACK    RDY1 <- DAV
 */
#define CTL_IDLE            0x07    /* all CTL high               */
#define CTL_SYNC_ASSERTED   0x03    /* CTL2 low, CTL1/CTL0 high   */

/* register bits used here (prefixed to avoid clashes with fx2regs.h) */
#define BR_GPIF_DONE        0x80    /* GPIFTRIG.7                 */
#define BR_GPIF_READ        0x04    /* GPIFTRIG.2                 */
#define BR_GPIF_EP2         0x00
#define BR_GPIF_EP6         0x02
#define BR_RDY1             0x02    /* GPIFREADYSTAT.1 = DAV      */
#define BR_EP2EF            0x02    /* EP24FIFOFLGS.1             */
#define BR_EP6FF            0x01    /* EP68FIFOFLGS.0             */
#define BR_HSM              0x80    /* USBCS.7                    */

/* gpif.c */
void gpif_init(void);
void gpif_start_write_ep2(WORD bytes);
void gpif_start_read_ep6(WORD bytes);
void gpif_abort(void);
void gpif_pulse_sync(void);

#endif /* BRIDGE_H */
