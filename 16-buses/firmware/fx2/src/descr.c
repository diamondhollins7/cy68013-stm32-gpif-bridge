/*
 * descr.c - USB descriptors
 *
 * One vendor-specific interface with two bulk endpoints:
 *   EP2 OUT  (PC -> STM32)   512 bytes @ high speed, 64 bytes @ full speed
 *   EP6 IN   (STM32 -> PC)   512 bytes @ high speed, 64 bytes @ full speed
 *
 * Microsoft OS 1.0 descriptors (string 0xEE + compat ID "WINUSB") are provided
 * so Windows 8/10/11 binds WinUSB automatically - no INF file needed.
 *
 * All descriptors are <= 64 bytes, so they are sent with a single EP0 packet.
 */
#include "bridge.h"

#define LO(w)   ((BYTE)((w) & 0xFF))
#define HI(w)   ((BYTE)((w) >> 8))

BYTE code dev_dscr[18] =
{
    18,                     /* bLength                      */
    0x01,                   /* bDescriptorType: DEVICE      */
    0x00, 0x02,             /* bcdUSB 2.00                  */
    0x00,                   /* bDeviceClass: per interface  */
    0x00,                   /* bDeviceSubClass              */
    0x00,                   /* bDeviceProtocol              */
    64,                     /* bMaxPacketSize0              */
    LO(USB_VID), HI(USB_VID),
    LO(USB_PID), HI(USB_PID),
    LO(USB_BCD_DEVICE), HI(USB_BCD_DEVICE),
    1,                      /* iManufacturer                */
    2,                      /* iProduct                     */
    3,                      /* iSerialNumber                */
    1                       /* bNumConfigurations           */
};

BYTE code dev_qual_dscr[10] =
{
    10, 0x06,               /* DEVICE_QUALIFIER             */
    0x00, 0x02,             /* bcdUSB 2.00                  */
    0x00, 0x00, 0x00,       /* class per interface          */
    64,                     /* bMaxPacketSize0              */
    1,                      /* bNumConfigurations           */
    0                       /* bReserved                    */
};

#define CFG_BODY(mps_lo, mps_hi)                                              \
    /* configuration */                                                       \
    9, 0x02, CFG_DSCR_LEN, 0x00, 1, 1, 0,                                     \
    0x80,                               /* bus powered          */            \
    (USB_MAX_POWER_MA / 2),                                                   \
    /* interface 0 */                                                         \
    9, 0x04, 0, 0, 2, 0xFF, 0x00, 0x00, 0,                                    \
    /* EP2 OUT bulk */                                                        \
    7, 0x05, 0x02, 0x02, (mps_lo), (mps_hi), 0,                               \
    /* EP6 IN bulk */                                                         \
    7, 0x05, 0x86, 0x02, (mps_lo), (mps_hi), 0

BYTE code hs_cfg_dscr[CFG_DSCR_LEN] = { CFG_BODY(0x00, 0x02) };   /* 512 */
BYTE code fs_cfg_dscr[CFG_DSCR_LEN] = { CFG_BODY(0x40, 0x00) };   /* 64  */

/* ---- strings (UTF-16LE) ----------------------------------------------- */
static BYTE code str0[] = { 4, 0x03, 0x09, 0x04 };              /* en-US */

static BYTE code str1[] =                                       /* manufacturer */
{
    2 + 2 * 10, 0x03,
    'O',0, 'p',0, 'e',0, 'n',0, 'B',0, 'r',0, 'i',0, 'd',0, 'g',0, 'e',0
};

static BYTE code str2[] =                                       /* product */
{
    2 + 2 * 16, 0x03,
    'F',0, 'X',0, '2',0, '-',0, 'S',0, 'T',0, 'M',0, '3',0,
    '2',0, ' ',0, 'B',0, 'r',0, 'i',0, 'd',0, 'g',0, 'e',0
};

static BYTE code str3[] =                                       /* serial */
{
    2 + 2 * 4, 0x03,
    '0',0, '0',0, '0',0, '1',0
};

/* Microsoft OS string descriptor, index 0xEE: "MSFT100" + vendor code */
static BYTE code str_msos[] =
{
    18, 0x03,
    'M',0, 'S',0, 'F',0, 'T',0, '1',0, '0',0, '0',0,
    VR_MS_OS_VENDOR, 0x00
};

/* Microsoft extended compat ID descriptor (wIndex = 4) */
BYTE code ms_compat_id_dscr[MS_COMPAT_ID_LEN] =
{
    MS_COMPAT_ID_LEN, 0, 0, 0,  /* dwLength                          */
    0x00, 0x01,                 /* bcdVersion 1.00                   */
    0x04, 0x00,                 /* wIndex: extended compat ID        */
    1,                          /* bCount                            */
    0, 0, 0, 0, 0, 0, 0,        /* reserved                          */
    0,                          /* bFirstInterfaceNumber             */
    1,                          /* reserved                          */
    'W', 'I', 'N', 'U', 'S', 'B', 0, 0,     /* compatibleID          */
    0, 0, 0, 0, 0, 0, 0, 0,                 /* subCompatibleID       */
    0, 0, 0, 0, 0, 0                        /* reserved              */
};

BYTE code *string_dscr(BYTE index)
{
    switch (index)
    {
    case 0:    return str0;
    case 1:    return str1;
    case 2:    return str2;
    case 3:    return str3;
    case 0xEE: return str_msos;
    default:   return 0;
    }
}
