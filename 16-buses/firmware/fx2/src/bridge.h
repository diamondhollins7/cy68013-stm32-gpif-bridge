/*
 * bridge.h - FX2LP <-> STM32F103 GPIF bridge: shared configuration
 */
#ifndef BRIDGE_H
#define BRIDGE_H

#include "fx2regs.h"

/* ---- USB identity (development values - replace VID/PID for a product) */
#define USB_VID             0x04B4
#define USB_PID             0xF103
#define USB_BCD_DEVICE      0x0100
#define USB_MAX_POWER_MA    250

#define FW_VERSION_MAJOR    1
#define FW_VERSION_MINOR    0

/* ---- Vendor requests ------------------------------------------------- */
#define VR_MS_OS_VENDOR     0x20    /* Microsoft OS 1.0 descriptor vendor code  */
#define VR_GET_STATUS       0xB0    /* IN, 8 bytes bridge status                */
#define VR_RESET_BRIDGE     0xB1    /* OUT, wValue bit0 = 1 -> also reset STM32 */

/* ---- Port A usage ------------------------------------------------------
 * PA0 : STM32 NRST (open-drain emulation: IOA.0 = 0, toggle OEA.0)
 * PA1 : LED D2, active low
 * PA2..PA7 : unused, driven low
 */
#define PA_STM_RST          0x01
#define PA_LED              0x02

/* ---- GPIF signal mapping (see docs/GPIF_interface.md) ----------------
 * CTL0 -> nWR    FX2 -> STM32 write strobe (active low)
 * CTL1 -> nRD    FX2 -> STM32 read  strobe (active low)
 * CTL2 -> nSYNC  bus re-synchronisation, low = discard partial frames
 * RDY0 <- ACK    STM32 word handshake
 * RDY1 <- DAV    STM32 has (a frame of) data for the PC
 */
#define CTL_IDLE            0x07    /* all CTL high               */
#define CTL_SYNC_ASSERTED   0x03    /* CTL2 low, CTL1/CTL0 high   */

/* gpif.c */
void gpif_init(void);
void gpif_start_write_ep2(WORD words);
void gpif_start_read_ep6(WORD words);
void gpif_abort(void);
void gpif_pulse_sync(void);

/* descr.c */
extern BYTE code dev_dscr[];
extern BYTE code dev_qual_dscr[];
extern BYTE code hs_cfg_dscr[];
extern BYTE code fs_cfg_dscr[];
extern BYTE code ms_compat_id_dscr[];
BYTE code *string_dscr(BYTE index);
#define CFG_DSCR_LEN        32
#define MS_COMPAT_ID_LEN    40

/* main.c */
void delay_ms(WORD ms);

#endif /* BRIDGE_H */
