/*
 * fx2regs.h - Register definitions for the Cypress/Infineon CY7C68013A (EZ-USB FX2LP)
 *
 * Self-contained (no Cypress framework needed). Addresses were taken from the
 * "FX2LP Register Summary" table of datasheet 38-08032 Rev. AD.
 * Compiler: Keil C51.
 */
#ifndef FX2REGS_H
#define FX2REGS_H

#include <intrins.h>

typedef unsigned char  BYTE;
typedef unsigned int   WORD;      /* 16 bit in C51 */

/* ---------------------------------------------------------------------- */
/* XDATA mapped registers                                                  */
/* ---------------------------------------------------------------------- */
#define XREG(a)         (*(volatile BYTE xdata *)(a))
#define XBUF(a)         ((volatile BYTE xdata *)(a))

#define GPIF_WAVE_DATA  XBUF(0xE400)      /* 128 bytes: 4 waveforms x 32 bytes */

#define CPUCS           XREG(0xE600)
#define IFCONFIG        XREG(0xE601)
#define PINFLAGSAB      XREG(0xE602)
#define PINFLAGSCD      XREG(0xE603)
#define FIFORESET       XREG(0xE604)
#define BREAKPT         XREG(0xE605)
#define FIFOPINPOLAR    XREG(0xE609)
#define REVID           XREG(0xE60A)
#define REVCTL          XREG(0xE60B)

#define EP1OUTCFG       XREG(0xE610)
#define EP1INCFG        XREG(0xE611)
#define EP2CFG          XREG(0xE612)
#define EP4CFG          XREG(0xE613)
#define EP6CFG          XREG(0xE614)
#define EP8CFG          XREG(0xE615)
#define EP2FIFOCFG      XREG(0xE618)
#define EP4FIFOCFG      XREG(0xE619)
#define EP6FIFOCFG      XREG(0xE61A)
#define EP8FIFOCFG      XREG(0xE61B)
#define EP6AUTOINLENH   XREG(0xE624)
#define EP6AUTOINLENL   XREG(0xE625)

#define INPKTEND        XREG(0xE648)
#define OUTPKTEND       XREG(0xE649)

#define USBIE           XREG(0xE65C)
#define USBIRQ          XREG(0xE65D)
#define EPIE            XREG(0xE65E)
#define EPIRQ           XREG(0xE65F)
#define GPIFIE          XREG(0xE660)
#define GPIFIRQ         XREG(0xE661)
#define INTSETUP        XREG(0xE668)

#define PORTACFG        XREG(0xE670)

#define USBCS           XREG(0xE680)
#define TOGCTL          XREG(0xE683)
#define FNADDR          XREG(0xE687)

#define EP0BCH          XREG(0xE68A)
#define EP0BCL          XREG(0xE68B)

#define EP0CS           XREG(0xE6A0)
#define EP1OUTCS        XREG(0xE6A1)
#define EP1INCS         XREG(0xE6A2)
#define EP2CS           XREG(0xE6A3)
#define EP4CS           XREG(0xE6A4)
#define EP6CS           XREG(0xE6A5)
#define EP8CS           XREG(0xE6A6)
#define EP2FIFOFLGS     XREG(0xE6A7)
#define EP6FIFOFLGS     XREG(0xE6A9)
#define EP2FIFOBCH      XREG(0xE6AB)
#define EP2FIFOBCL      XREG(0xE6AC)
#define EP6FIFOBCH      XREG(0xE6AF)
#define EP6FIFOBCL      XREG(0xE6B0)

#define SUDPTRH         XREG(0xE6B3)
#define SUDPTRL         XREG(0xE6B4)
#define SUDPTRCTL       XREG(0xE6B5)
#define SETUPDAT        XBUF(0xE6B8)      /* 8 bytes */

#define GPIFWFSELECT    XREG(0xE6C0)
#define GPIFIDLECS      XREG(0xE6C1)
#define GPIFIDLECTL     XREG(0xE6C2)
#define GPIFCTLCFG      XREG(0xE6C3)
#define GPIFADRH        XREG(0xE6C4)
#define GPIFADRL        XREG(0xE6C5)
#define FLOWSTATE       XREG(0xE6C6)
#define GPIFTCB3        XREG(0xE6CE)
#define GPIFTCB2        XREG(0xE6CF)
#define GPIFTCB1        XREG(0xE6D0)
#define GPIFTCB0        XREG(0xE6D1)
#define EP2GPIFFLGSEL   XREG(0xE6D2)
#define EP2GPIFPFSTOP   XREG(0xE6D3)
#define EP6GPIFFLGSEL   XREG(0xE6E2)
#define EP6GPIFPFSTOP   XREG(0xE6E3)
#define GPIFREADYCFG    XREG(0xE6F3)
#define GPIFREADYSTAT   XREG(0xE6F4)
#define GPIFABORT       XREG(0xE6F5)

#define EP0BUF          XBUF(0xE740)      /* 64 bytes */

/* ---------------------------------------------------------------------- */
/* Special function registers                                              */
/* ---------------------------------------------------------------------- */
sfr IOA           = 0x80;
sfr DPS           = 0x86;
sfr CKCON         = 0x8E;
sfr IOB           = 0x90;
sfr EXIF          = 0x91;
sfr IOC           = 0xA0;
sfr IE            = 0xA8;
sfr EP2468STAT    = 0xAA;
sfr EP24FIFOFLGS  = 0xAB;
sfr EP68FIFOFLGS  = 0xAC;
sfr IOD           = 0xB0;
sfr OEA           = 0xB2;
sfr OEB           = 0xB3;
sfr OED           = 0xB5;
sfr EP01STAT      = 0xBA;
sfr GPIFTRIG      = 0xBB;
sfr EIE           = 0xE8;

/* ---------------------------------------------------------------------- */
/* Bit masks                                                               */
/* ---------------------------------------------------------------------- */
/* USBIRQ / USBIE */
#define bmSUDAV     0x01
#define bmSOF       0x02
#define bmSUTOK     0x04
#define bmSUSP      0x08
#define bmURES      0x10
#define bmHSGRANT   0x20
#define bmEP0ACK    0x40

/* USBCS */
#define bmHSM       0x80
#define bmDISCON    0x08
#define bmNOSYNSOF  0x04
#define bmRENUM     0x02
#define bmSIGRESUME 0x01

/* EP0CS / EPxCS */
#define bmHSNAK     0x80
#define bmEPBUSY    0x02
#define bmEPSTALL   0x01

/* GPIFTRIG */
#define bmGPIFDONE  0x80
#define bmGPIFREAD  0x04
#define GPIF_EP2    0x00
#define GPIF_EP6    0x02

/* EP24FIFOFLGS / EP68FIFOFLGS (peripheral-side FIFO flags) */
#define bmEP2FF     0x01
#define bmEP2EF     0x02
#define bmEP6FF     0x01
#define bmEP6EF     0x02

/* GPIFREADYSTAT */
#define bmRDY0      0x01
#define bmRDY1      0x02

/* TOGCTL */
#define bmTOG_IO    0x10
#define bmTOG_R     0x20

/*
 * Synchronisation delay required after writing some FIFO/GPIF registers
 * (TRM chapter 15.14). 4 NOPs is enough for CPU clock 48 MHz with IFCLK 48 MHz.
 */
#define SYNCDELAY   do { _nop_(); _nop_(); _nop_(); _nop_(); } while (0)

#endif /* FX2REGS_H */
