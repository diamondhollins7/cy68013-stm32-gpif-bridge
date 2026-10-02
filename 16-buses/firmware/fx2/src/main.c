/*
 * main.c - CY7C68013A (FX2LP, 56-pin) USB <-> GPIF bridge to an STM32F103
 *
 *   PC --USB2.0 HS bulk EP2 OUT--> FX2 --GPIF 16-bit write--> STM32
 *   PC <--USB2.0 HS bulk EP6 IN--- FX2 <--GPIF 16-bit read--- STM32
 *
 * The USB data path never touches the 8051: EP2 is AUTOOUT, EP6 is AUTOIN and
 * the GPIF moves data between the endpoint FIFOs and the STM32. The 8051 only
 * handles control requests (polled, no interrupts) and decides when to start
 * GPIF transfers.
 *
 * Toolchain: Keil C51, memory model SMALL. Output: Intel HEX (convert to .iic
 * with tools/hex2iic.py to program the boot EEPROM).
 */
#include "bridge.h"

static BYTE data configuration;     /* 0 = unconfigured, 1 = configured   */
static WORD data in_chunk_words;    /* words per GPIF read = 1 USB packet  */
static BYTE data led_div;

/* ---------------------------------------------------------------------- */
/* helpers                                                                 */
/* ---------------------------------------------------------------------- */

/* Approximate busy-wait at CPU clock 48 MHz. */
void delay_ms(WORD ms)
{
    WORD i;

    while (ms--)
    {
        for (i = 0; i < 1000; i++)
            _nop_();
    }
}

static void stm32_reset(void)
{
    IOA &= ~PA_STM_RST;
    OEA |= PA_STM_RST;              /* pull NRST low                     */
    delay_ms(2);
    OEA &= ~PA_STM_RST;             /* release - board pull-up           */
    delay_ms(20);                   /* let the STM32 start its firmware  */
}

static void activity(void)
{
    if (!(++led_div & 0x3F))
        IOA ^= PA_LED;
}

/* EP6 AUTOIN commit length must equal wMaxPacketSize of the current speed. */
static void update_packet_size(void)
{
    WORD bytes = (USBCS & bmHSM) ? 512 : 64;

    in_chunk_words = bytes / 2;
    EP6AUTOINLENH = (BYTE)(bytes >> 8);
    SYNCDELAY;
    EP6AUTOINLENL = (BYTE)bytes;
    SYNCDELAY;
}

/* Reset EP2/EP6 FIFOs, re-arm OUT buffers and enter AUTO mode (TRM 15.5). */
static void fifo_init(void)
{
    EP2FIFOCFG = 0x00;              /* leave AUTO mode while resetting   */
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

    EP2FIFOCFG = 0x11;              /* AUTOOUT | WORDWIDE                */
    SYNCDELAY;
    EP6FIFOCFG = 0x09;              /* AUTOIN  | WORDWIDE                */
    SYNCDELAY;

    update_packet_size();
}

static void reset_toggle(BYTE ep)
{
    BYTE v = (ep & 0x0F) | ((ep & 0x80) ? bmTOG_IO : 0);

    TOGCTL = v;
    TOGCTL = v | bmTOG_R;
}

/* Abort any transfer, optionally reset the STM32, re-align frames, flush FIFOs. */
static void bridge_reset(BYTE reset_stm)
{
    gpif_abort();
    if (reset_stm)
        stm32_reset();
    gpif_pulse_sync();
    fifo_init();
    reset_toggle(0x02);
    reset_toggle(0x86);
}

/* ---------------------------------------------------------------------- */
/* EP0 / control requests                                                  */
/* ---------------------------------------------------------------------- */

static WORD setup_wlength(void)
{
    return ((WORD)SETUPDAT[7] << 8) | SETUPDAT[6];
}

/* Arm EP0 IN with 'len' bytes already in EP0BUF (clipped to wLength). */
static void ep0_arm(BYTE len)
{
    if (setup_wlength() < len)
        len = (BYTE)setup_wlength();
    EP0BCH = 0;
    EP0BCL = len;
}

/* Copy a descriptor (<= 64 bytes) into EP0BUF and send it. */
static void ep0_send(BYTE code *src, BYTE len, BYTE patch_type)
{
    BYTE i;

    for (i = 0; i < len; i++)
        EP0BUF[i] = src[i];
    if (patch_type)
        EP0BUF[1] = patch_type;     /* OTHER_SPEED_CONFIGURATION         */
    ep0_arm(len);
}

static volatile BYTE xdata *ep_cs(BYTE ep)
{
    switch (ep)
    {
    case 0x02: return &EP2CS;
    case 0x86: return &EP6CS;
    default:   return 0;
    }
}

static BYTE get_descriptor(void)
{
    BYTE code *p;
    BYTE hs = (USBCS & bmHSM) ? 1 : 0;

    switch (SETUPDAT[3])
    {
    case 0x01:                      /* DEVICE                            */
        ep0_send(dev_dscr, 18, 0);
        return 1;
    case 0x02:                      /* CONFIGURATION                     */
        ep0_send(hs ? hs_cfg_dscr : fs_cfg_dscr, CFG_DSCR_LEN, 0);
        return 1;
    case 0x03:                      /* STRING                            */
        p = string_dscr(SETUPDAT[2]);
        if (!p)
            return 0;
        ep0_send(p, p[0], 0);
        return 1;
    case 0x06:                      /* DEVICE_QUALIFIER                  */
        ep0_send(dev_qual_dscr, 10, 0);
        return 1;
    case 0x07:                      /* OTHER_SPEED_CONFIGURATION         */
        ep0_send(hs ? fs_cfg_dscr : hs_cfg_dscr, CFG_DSCR_LEN, 0x07);
        return 1;
    default:
        return 0;
    }
}

static BYTE std_request(void)
{
    BYTE recip = SETUPDAT[0] & 0x1F;
    BYTE ep = SETUPDAT[4];
    volatile BYTE xdata *cs;

    switch (SETUPDAT[1])
    {
    case 0x00:                      /* GET_STATUS                        */
        EP0BUF[0] = 0;              /* bus powered, no remote wakeup     */
        EP0BUF[1] = 0;
        if (recip == 2 && (ep & 0x7F))
        {
            cs = ep_cs(ep);
            if (!cs)
                return 0;
            EP0BUF[0] = (*cs & bmEPSTALL) ? 1 : 0;
        }
        else if (recip > 2)
            return 0;
        ep0_arm(2);
        return 1;

    case 0x01:                      /* CLEAR_FEATURE                     */
    case 0x03:                      /* SET_FEATURE                       */
        if (recip == 0)             /* remote wakeup / test mode: accept */
            return 1;
        if (recip != 2 || SETUPDAT[2] != 0)
            return 0;               /* only ENDPOINT_HALT supported      */
        if (!(ep & 0x7F))
            return 1;               /* EP0: nothing to do                */
        cs = ep_cs(ep);
        if (!cs)
            return 0;
        if (SETUPDAT[1] == 0x03)
            *cs |= bmEPSTALL;
        else
        {
            *cs &= ~bmEPSTALL;
            reset_toggle(ep);
        }
        return 1;

    case 0x06:                      /* GET_DESCRIPTOR                    */
        return get_descriptor();

    case 0x08:                      /* GET_CONFIGURATION                 */
        EP0BUF[0] = configuration;
        ep0_arm(1);
        return 1;

    case 0x09:                      /* SET_CONFIGURATION                 */
        if (SETUPDAT[2] > 1)
            return 0;
        configuration = SETUPDAT[2];
        if (configuration)
            bridge_reset(0);
        return 1;

    case 0x0A:                      /* GET_INTERFACE                     */
        EP0BUF[0] = 0;
        ep0_arm(1);
        return 1;

    case 0x0B:                      /* SET_INTERFACE (only alt 0)        */
        if (SETUPDAT[2] != 0 || SETUPDAT[4] != 0)
            return 0;
        reset_toggle(0x02);
        reset_toggle(0x86);
        return 1;

    default:                        /* SET_ADDRESS is done by hardware   */
        return 0;
    }
}

static BYTE vendor_request(void)
{
    BYTE dir_in = SETUPDAT[0] & 0x80;

    switch (SETUPDAT[1])
    {
    case VR_MS_OS_VENDOR:           /* MS OS 1.0 extended compat ID      */
        if (!dir_in || SETUPDAT[4] != 0x04 || SETUPDAT[5] != 0x00)
            return 0;
        ep0_send(ms_compat_id_dscr, MS_COMPAT_ID_LEN, 0);
        return 1;

    case VR_GET_STATUS:
        if (!dir_in)
            return 0;
        EP0BUF[0] = FW_VERSION_MAJOR;
        EP0BUF[1] = FW_VERSION_MINOR;
        EP0BUF[2] = GPIFREADYSTAT;  /* bit0 ACK, bit1 DAV                */
        EP0BUF[3] = GPIFTRIG;       /* bit7 = GPIF idle                  */
        EP0BUF[4] = EP24FIFOFLGS;
        EP0BUF[5] = EP68FIFOFLGS;
        EP0BUF[6] = (USBCS & bmHSM) ? 1 : 0;
        EP0BUF[7] = configuration;
        ep0_arm(8);
        return 1;

    case VR_RESET_BRIDGE:
        if (dir_in)
            return 0;
        bridge_reset(SETUPDAT[2] & 0x01);
        return 1;

    default:
        return 0;
    }
}

static void handle_setup(void)
{
    BYTE ok;

    switch (SETUPDAT[0] & 0x60)
    {
    case 0x00: ok = std_request();    break;
    case 0x40: ok = vendor_request(); break;
    default:   ok = 0;                break;
    }

    if (!ok)
        EP0CS |= bmEPSTALL;
    EP0CS |= bmHSNAK;               /* complete the status stage         */
}

/* ---------------------------------------------------------------------- */
/* GPIF scheduling                                                         */
/* ---------------------------------------------------------------------- */

static void bridge_poll(void)
{
    WORD bc;

    if (!(GPIFTRIG & bmGPIFDONE))
        return;                     /* a transfer is still running       */

    /* STM32 -> PC first, so the STM32 transmit queue keeps draining.
     * DAV stays high for a whole 512-byte frame, which is a multiple of
     * the packet size at both speeds, so a chunk never straddles frames. */
    if ((GPIFREADYSTAT & bmRDY1) && !(EP68FIFOFLGS & bmEP6FF))
    {
        gpif_start_read_ep6(in_chunk_words);
        activity();
        return;
    }

    /* PC -> STM32 */
    if (!(EP24FIFOFLGS & bmEP2EF))
    {
        bc = ((WORD)EP2FIFOBCH << 8) | EP2FIFOBCL;
        if (bc)
        {
            gpif_start_write_ep2((bc + 1) >> 1);
            activity();
        }
        else
        {
            fifo_init();            /* a zero-length packet would block  */
        }
    }
}

/* ---------------------------------------------------------------------- */
/* main                                                                    */
/* ---------------------------------------------------------------------- */

static void usb_init(void)
{
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

    fifo_init();
    gpif_init();
}

/* Soft-disconnect and re-enumerate with our own descriptors (RENUM = 1). */
static void usb_connect(void)
{
    USBCS |= bmDISCON | bmRENUM;
    delay_ms(500);
    USBIRQ = 0xFF;
    EPIRQ = 0xFF;
    EXIF &= ~0x10;
    USBCS &= ~bmDISCON;
}

void main(void)
{
    BYTE irq;

    IE = 0;                         /* everything is polled              */
    EIE = 0;
    configuration = 0;
    led_div = 0;

    CPUCS = 0x12;                   /* 8051 at 48 MHz, CLKOUT enabled    */

    PORTACFG = 0x00;                /* PA0/PA1/PA7 as plain GPIO         */
    IOA = PA_LED;                   /* LED off, NRST released            */
    OEA = (BYTE)~PA_STM_RST;        /* PA1..PA7 outputs, PA0 open-drain  */

    usb_init();
    stm32_reset();
    gpif_pulse_sync();
    usb_connect();

    for (;;)
    {
        irq = USBIRQ;
        if (irq)
            USBIRQ = irq;           /* write-1-to-clear                  */

        if (irq & (bmURES | bmHSGRANT))
        {
            if (irq & bmURES)
                configuration = 0;
            gpif_abort();
            update_packet_size();
        }

        if (irq & bmSUDAV)
            handle_setup();

        if (configuration)
            bridge_poll();
    }
}
