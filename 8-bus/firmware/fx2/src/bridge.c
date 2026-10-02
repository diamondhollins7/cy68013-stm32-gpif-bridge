/*
 * bridge.c - CY7C68013A (FX2LP, 56-pin) USB <-> 8-bit GPIF bridge to an STM32F103
 *
 * User part of the Cypress FX2LP firmware framework: fw.c calls TD_Init() once,
 * then TD_Poll() continuously, and the DR_xxx() hooks for control requests.
 *
 *   PC --bulk EP2 OUT--> FX2 --GPIF 8-bit write--> STM32
 *   PC <--bulk EP6 IN--- FX2 <--GPIF 8-bit read--- STM32
 *
 * EP2 is AUTOOUT, EP6 is AUTOIN: USB data never passes through the 8051.
 * The PC side uses the Cypress driver (CyUSB3.sys) and CyAPI, see host/.
 */
#pragma NOIV                        /* interrupt vectors come from USBJmpTb */
#include "bridge.h"

extern BOOL GotSUD;                 /* defined in fw.c */
extern BOOL Sleep;
extern BOOL Rwuen;
extern BOOL Selfpwr;

BYTE Configuration;                 /* current configuration   */
BYTE AlternateSetting;              /* current alternate setting */

static WORD in_chunk;               /* bytes per GPIF read = 1 USB packet */
static BYTE hs_mode;                /* last seen USBCS.HSM               */
static BYTE led_div;

/* ---------------------------------------------------------------------- */
/* helpers                                                                 */
/* ---------------------------------------------------------------------- */

static void stm32_reset(void)
{
    IOA &= ~PA_STM_RST;
    OEA |= PA_STM_RST;              /* pull NRST low                     */
    EZUSB_Delay(2);
    OEA &= ~PA_STM_RST;             /* release - board pull-up           */
    EZUSB_Delay(20);                /* let the STM32 start its firmware  */
}

static void activity(void)
{
    if (!(++led_div & 0x3F))
        IOA ^= PA_LED;
}

/* EP6 AUTOIN commit length must equal wMaxPacketSize of the current speed. */
static void update_packet_size(void)
{
    hs_mode = USBCS & BR_HSM;
    in_chunk = hs_mode ? 512 : 64;
    EP6AUTOINLENH = MSB(in_chunk);
    SYNCDELAY;
    EP6AUTOINLENL = LSB(in_chunk);
    SYNCDELAY;
}

/* Reset EP2/EP6 FIFOs, re-arm the OUT buffers and enter AUTO mode, 8 bit. */
static void fifo_init(void)
{
    EP2FIFOCFG = 0x00;              /* manual mode while resetting       */
    SYNCDELAY;
    EP6FIFOCFG = 0x00;
    SYNCDELAY;

    FIFORESET = 0x80;               /* NAK all host transfers            */
    SYNCDELAY;
    FIFORESET = 0x02;
    SYNCDELAY;
    FIFORESET = 0x04;
    SYNCDELAY;
    FIFORESET = 0x06;
    SYNCDELAY;
    FIFORESET = 0x08;
    SYNCDELAY;
    FIFORESET = 0x00;
    SYNCDELAY;

    OUTPKTEND = 0x82;               /* arm both EP2 buffers (REVCTL.0=1) */
    SYNCDELAY;
    OUTPKTEND = 0x82;
    SYNCDELAY;

    EP2FIFOCFG = 0x10;              /* AUTOOUT, WORDWIDE = 0 (8 bit)     */
    SYNCDELAY;
    EP6FIFOCFG = 0x08;              /* AUTOIN,  WORDWIDE = 0 (8 bit)     */
    SYNCDELAY;

    update_packet_size();
}

static void bridge_reset(BYTE reset_stm)
{
    gpif_abort();
    if (reset_stm)
        stm32_reset();
    gpif_pulse_sync();
    fifo_init();
    EZUSB_RESET_DATA_TOGGLE(0x02);
    EZUSB_RESET_DATA_TOGGLE(0x86);
}

/* ---------------------------------------------------------------------- */
/* framework task hooks                                                    */
/* ---------------------------------------------------------------------- */

void TD_Init(void)
{
    Configuration = 0;
    AlternateSetting = 0;
    led_div = 0;

    CPUCS = (CPUCS & ~0x18) | 0x10; /* 8051 at 48 MHz                   */

    PORTACFG = 0x00;                /* PA0/PA1/PA7 as plain GPIO         */
    IOA = PA_LED;                   /* LED off, NRST released            */
    OEA = (BYTE)~PA_STM_RST;        /* PA1..PA7 outputs, PA0 open-drain  */

    REVCTL = 0x03;                  /* DYN_OUT | ENH_PKT (recommended)   */
    SYNCDELAY;
    /* internal 48 MHz IFCLK, IFCLK pin not driven, async GPIF, GPIF master */
    IFCONFIG = 0xCA;
    SYNCDELAY;

    EP1OUTCFG = 0x00;               /* EP1 not used                      */
    SYNCDELAY;
    EP1INCFG = 0x00;
    SYNCDELAY;
    EP2CFG = 0xA2;                  /* valid, OUT, bulk, 512, double     */
    SYNCDELAY;
    EP4CFG = 0x00;                  /* not used                          */
    SYNCDELAY;
    EP6CFG = 0xE2;                  /* valid, IN,  bulk, 512, double     */
    SYNCDELAY;
    EP8CFG = 0x00;                  /* not used                          */
    SYNCDELAY;

    /* All four FIFOs must be WORDWIDE = 0, otherwise port D becomes FD[15:8]. */
    EP4FIFOCFG = 0x00;
    SYNCDELAY;
    EP8FIFOCFG = 0x00;
    SYNCDELAY;
    fifo_init();

    IOD = 0x00;                     /* port D unused in 8-bit mode:      */
    OED = 0xFF;                     /* drive low, never floating         */

    gpif_init();
    stm32_reset();
    gpif_pulse_sync();
}

void TD_Poll(void)
{
    WORD bc;

    if (!(GPIFTRIG & BR_GPIF_DONE))
        return;                     /* a transfer is still running       */

    if ((USBCS & BR_HSM) != hs_mode)
        update_packet_size();       /* speed changed (HS handshake)      */

    if (Configuration != 1)
        return;

    /* STM32 -> PC first, so the STM32 transmit queue keeps draining.
     * DAV stays high for a whole 512-byte frame, a multiple of the packet
     * size at both speeds, so a chunk never straddles two frames. */
    if ((GPIFREADYSTAT & BR_RDY1) && !(EP68FIFOFLGS & BR_EP6FF))
    {
        gpif_start_read_ep6(in_chunk);
        activity();
        return;
    }

    /* PC -> STM32 */
    if (!(EP24FIFOFLGS & BR_EP2EF))
    {
        bc = ((WORD)EP2FIFOBCH << 8) | EP2FIFOBCL;
        if (bc)
        {
            gpif_start_write_ep2(bc);
            activity();
        }
        else
        {
            fifo_init();            /* a zero-length packet would block  */
        }
    }
}

BOOL TD_Suspend(void)
{
    return TRUE;
}

BOOL TD_Resume(void)
{
    return TRUE;
}

/* ---------------------------------------------------------------------- */
/* device request hooks (TRUE = let the framework do the standard work)    */
/* ---------------------------------------------------------------------- */

BOOL DR_GetDescriptor(void)
{
    return TRUE;
}

BOOL DR_SetConfiguration(void)
{
    Configuration = SETUPDAT[2];
    if (Configuration == 1)
        bridge_reset(0);
    return TRUE;
}

BOOL DR_GetConfiguration(void)
{
    EP0BUF[0] = Configuration;
    EP0BCH = 0;
    EP0BCL = 1;
    return TRUE;
}

BOOL DR_SetInterface(void)
{
    AlternateSetting = SETUPDAT[2];
    return TRUE;
}

BOOL DR_GetInterface(void)
{
    EP0BUF[0] = AlternateSetting;
    EP0BCH = 0;
    EP0BCL = 1;
    return TRUE;
}

BOOL DR_GetStatus(void)
{
    return TRUE;
}

BOOL DR_ClearFeature(void)
{
    return TRUE;
}

BOOL DR_SetFeature(void)
{
    return TRUE;
}

/* Returns FALSE when the request was handled, TRUE to stall EP0. */
BOOL DR_VendorCmnd(void)
{
    BYTE len;

    switch (SETUPDAT[1])
    {
    case VR_GET_STATUS:
        if (!(SETUPDAT[0] & 0x80))
            return TRUE;
        EP0BUF[0] = FW_VERSION_MAJOR;
        EP0BUF[1] = FW_VERSION_MINOR;
        EP0BUF[2] = GPIFREADYSTAT;  /* bit0 ACK, bit1 DAV                */
        EP0BUF[3] = GPIFTRIG;       /* bit7 = GPIF idle                  */
        EP0BUF[4] = EP24FIFOFLGS;
        EP0BUF[5] = EP68FIFOFLGS;
        EP0BUF[6] = (USBCS & BR_HSM) ? 1 : 0;
        EP0BUF[7] = Configuration;
        len = 8;
        if (SETUPDAT[7] == 0 && SETUPDAT[6] < len)
            len = SETUPDAT[6];
        EP0BCH = 0;
        EP0BCL = len;
        return FALSE;

    case VR_RESET_BRIDGE:
        if (SETUPDAT[0] & 0x80)
            return TRUE;
        bridge_reset(SETUPDAT[2] & 0x01);
        return FALSE;

    default:
        return TRUE;
    }
}

/* ---------------------------------------------------------------------- */
/* USB interrupt handlers (vectored through USBJmpTb)                      */
/* ---------------------------------------------------------------------- */

void ISR_Sudav(void) interrupt 0
{
    GotSUD = TRUE;                  /* fw.c calls SetupCommand()         */
    EZUSB_IRQ_CLEAR();
    USBIRQ = bmSUDAV;
}

void ISR_Sutok(void) interrupt 0
{
    EZUSB_IRQ_CLEAR();
    USBIRQ = bmSUTOK;
}

void ISR_Sof(void) interrupt 0
{
    EZUSB_IRQ_CLEAR();
    USBIRQ = bmSOF;
}

void ISR_Ures(void) interrupt 0
{
    /* a bus reset always starts at full speed */
    pConfigDscr = pFullSpeedConfigDscr;
    ((CONFIGDSCR xdata *) pConfigDscr)->type = CONFIG_DSCR;
    pOtherConfigDscr = pHighSpeedConfigDscr;
    ((CONFIGDSCR xdata *) pOtherConfigDscr)->type = OTHERSPEED_DSCR;
    Configuration = 0;

    EZUSB_IRQ_CLEAR();
    USBIRQ = bmURES;
}

void ISR_Susp(void) interrupt 0
{
    Sleep = TRUE;
    EZUSB_IRQ_CLEAR();
    USBIRQ = bmSUSP;
}

void ISR_Highspeed(void) interrupt 0
{
    if (EZUSB_HIGHSPEED())
    {
        pConfigDscr = pHighSpeedConfigDscr;
        ((CONFIGDSCR xdata *) pConfigDscr)->type = CONFIG_DSCR;
        pOtherConfigDscr = pFullSpeedConfigDscr;
        ((CONFIGDSCR xdata *) pOtherConfigDscr)->type = OTHERSPEED_DSCR;
    }
    EZUSB_IRQ_CLEAR();
    USBIRQ = bmHSGRANT;
}

void ISR_Ep0ack(void) interrupt 0 { }
void ISR_Stub(void) interrupt 0 { }
void ISR_Ep0in(void) interrupt 0 { }
void ISR_Ep0out(void) interrupt 0 { }
void ISR_Ep1in(void) interrupt 0 { }
void ISR_Ep1out(void) interrupt 0 { }
void ISR_Ep2inout(void) interrupt 0 { }
void ISR_Ep4inout(void) interrupt 0 { }
void ISR_Ep6inout(void) interrupt 0 { }
void ISR_Ep8inout(void) interrupt 0 { }
void ISR_Ibn(void) interrupt 0 { }
void ISR_Ep0pingnak(void) interrupt 0 { }
void ISR_Ep1pingnak(void) interrupt 0 { }
void ISR_Ep2pingnak(void) interrupt 0 { }
void ISR_Ep4pingnak(void) interrupt 0 { }
void ISR_Ep6pingnak(void) interrupt 0 { }
void ISR_Ep8pingnak(void) interrupt 0 { }
void ISR_Errorlimit(void) interrupt 0 { }
void ISR_Ep2piderror(void) interrupt 0 { }
void ISR_Ep4piderror(void) interrupt 0 { }
void ISR_Ep6piderror(void) interrupt 0 { }
void ISR_Ep8piderror(void) interrupt 0 { }
void ISR_Ep2pflag(void) interrupt 0 { }
void ISR_Ep4pflag(void) interrupt 0 { }
void ISR_Ep6pflag(void) interrupt 0 { }
void ISR_Ep8pflag(void) interrupt 0 { }
void ISR_Ep2eflag(void) interrupt 0 { }
void ISR_Ep4eflag(void) interrupt 0 { }
void ISR_Ep6eflag(void) interrupt 0 { }
void ISR_Ep8eflag(void) interrupt 0 { }
void ISR_Ep2fflag(void) interrupt 0 { }
void ISR_Ep4fflag(void) interrupt 0 { }
void ISR_Ep6fflag(void) interrupt 0 { }
void ISR_Ep8fflag(void) interrupt 0 { }
void ISR_GpifComplete(void) interrupt 0 { }
void ISR_GpifWaveform(void) interrupt 0 { }
