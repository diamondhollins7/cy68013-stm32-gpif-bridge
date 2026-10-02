// CyBridgeTest - PC test program for the FX2LP <-> STM32 8-bit GPIF bridge.
//
// Uses the Cypress USB driver (CyUSB3.sys) through the CyAPI C++ library.
//
//   CyBridgeTest list               list Cypress-driver devices
//   CyBridgeTest status             FX2 bridge status (vendor request 0xB0)
//   CyBridgeTest reset              flush bridge FIFOs and reset the STM32 (0xB1)
//   CyBridgeTest info               STM32 version / clock / UID / counters
//   CyBridgeTest led on|off         user LED D5 on the STM32 side
//   CyBridgeTest echo [N]           N echo frames with random data, verified
//   CyBridgeTest stream [N]         N stream frames STM32 -> PC, verified, MB/s
//
// Frame protocol (512 bytes, see docs/GPIF_interface.md):
//   [0] magic (A5 PC->STM32 / 5A STM32->PC)  [1] cmd  [2..3] seq  [4..5] len
//   [6] status  [7] 0  [8..511] payload

#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <random>
#include <vector>

#include "CyAPI.h"

static const USHORT VID = 0x04B4;
static const USHORT PID = 0x1004;
static const UCHAR  EP_OUT = 0x02;
static const UCHAR  EP_IN = 0x86;

static const int FRAME = 512;
static const int HDR = 8;
static const int PAYLOAD_MAX = FRAME - HDR;

static const UCHAR VR_GET_STATUS = 0xB0;
static const UCHAR VR_RESET_BRIDGE = 0xB1;

enum : UCHAR
{
    CMD_ECHO = 0x01,
    CMD_SET_LED = 0x02,
    CMD_GET_INFO = 0x03,
    CMD_STREAM_START = 0x04,
    CMD_STREAM_STOP = 0x05,
    CMD_REPLY = 0x80,
    CMD_STREAM_DATA = 0xC4,
};

static uint16_t rd16(const UCHAR *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t rd32(const UCHAR *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }
static void wr16(UCHAR *p, uint16_t v) { p[0] = (UCHAR)v; p[1] = (UCHAR)(v >> 8); }
static void wr32(UCHAR *p, uint32_t v) { for (int i = 0; i < 4; i++) p[i] = (UCHAR)(v >> (8 * i)); }

class Bridge
{
public:
    Bridge() : dev(new CCyUSBDevice(NULL, CYUSBDRV_GUID, true)) {}
    ~Bridge()
    {
        if (dev->IsOpen())
            dev->Close();
        delete dev;
    }

    void list()
    {
        int n = dev->DeviceCount();
        printf("%d device(s) on the Cypress driver\n", n);
        for (int i = 0; i < n; i++)
        {
            dev->Open((UCHAR)i);
            printf("  [%d] %04X:%04X  %s  %s\n", i, dev->VendorID, dev->ProductID,
                   dev->FriendlyName, dev->bHighSpeed ? "high-speed" : "full-speed");
            dev->Close();
        }
    }

    bool open()
    {
        for (int i = 0; i < dev->DeviceCount(); i++)
        {
            if (dev->Open((UCHAR)i) && dev->VendorID == VID && dev->ProductID == PID)
            {
                epOut = findEp(EP_OUT);
                epIn = findEp(EP_IN);
                if (!epOut || !epIn)
                {
                    printf("device found but EP2 OUT / EP6 IN missing\n");
                    return false;
                }
                epOut->TimeOut = 2000;
                epIn->TimeOut = 2000;
                return true;
            }
            dev->Close();
        }
        printf("device %04X:%04X not found (driver installed? see README)\n", VID, PID);
        return false;
    }

    // ---- vendor requests, handled by the FX2 ------------------------------
    bool vendor(UCHAR req, bool in, USHORT value, UCHAR *buf, LONG &len)
    {
        CCyControlEndPoint *ep = dev->ControlEndPt;
        ep->Target = TGT_DEVICE;
        ep->ReqType = REQ_VENDOR;
        ep->Direction = in ? DIR_FROM_DEVICE : DIR_TO_DEVICE;
        ep->ReqCode = req;
        ep->Value = value;
        ep->Index = 0;
        ep->TimeOut = 1000;
        return ep->XferData(buf, len) != 0;
    }

    void status()
    {
        UCHAR s[8] = {0};
        LONG len = 8;
        if (!vendor(VR_GET_STATUS, true, 0, s, len) || len != 8)
        {
            printf("status request failed\n");
            return;
        }
        printf("  FX2 firmware   %d.%d\n", s[0], s[1]);
        printf("  ACK (RDY0)     %d\n", s[2] & 1);
        printf("  DAV (RDY1)     %d\n", (s[2] >> 1) & 1);
        printf("  GPIF idle      %d\n", (s[3] >> 7) & 1);
        printf("  EP2 empty      %d\n", (s[4] >> 1) & 1);
        printf("  EP6 full       %d\n", s[5] & 1);
        printf("  high speed     %d\n", s[6]);
        printf("  configured     %d\n", s[7]);
    }

    void reset(bool resetStm32)
    {
        UCHAR dummy[1] = {0};
        LONG len = 0;
        if (!vendor(VR_RESET_BRIDGE, false, resetStm32 ? 1 : 0, dummy, len))
            printf("reset request failed\n");
        Sleep(50);
        epIn->Abort();
        epIn->Reset();
        epOut->Reset();
    }

    // ---- frames, through the GPIF to the STM32 -----------------------------
    bool send(UCHAR cmd, const UCHAR *payload, int plen, uint16_t &seqOut)
    {
        UCHAR f[FRAME] = {0};
        seq++;
        f[0] = 0xA5;
        f[1] = cmd;
        wr16(&f[2], seq);
        wr16(&f[4], (uint16_t)plen);
        if (plen)
            memcpy(&f[HDR], payload, plen);
        LONG len = FRAME;
        seqOut = seq;
        return epOut->XferData(f, len) && len == FRAME;
    }

    bool recv(UCHAR *f)
    {
        LONG len = FRAME;
        if (!epIn->XferData(f, len) || len != FRAME || f[0] != 0x5A)
        {
            printf("receive failed (len %ld, magic 0x%02X)\n", len, f[0]);
            return false;
        }
        return true;
    }

    // send a command and wait for its reply; returns payload length or -1
    int command(UCHAR cmd, const UCHAR *payload, int plen, UCHAR *reply)
    {
        uint16_t s;
        if (!send(cmd, payload, plen, s))
        {
            printf("send failed\n");
            return -1;
        }
        if (!recv(reply))
            return -1;
        if (reply[1] != (cmd | CMD_REPLY) || rd16(&reply[2]) != s)
        {
            printf("unexpected reply cmd=0x%02X seq=%u\n", reply[1], rd16(&reply[2]));
            return -1;
        }
        if (reply[6])
        {
            printf("STM32 returned status %d\n", reply[6]);
            return -1;
        }
        return rd16(&reply[4]);
    }

    CCyBulkEndPoint *in() { return epIn; }

private:
    CCyBulkEndPoint *findEp(UCHAR addr)
    {
        for (int i = 1; i < dev->EndPointCount(); i++)
            if (dev->EndPoints[i]->Address == addr)
                return (CCyBulkEndPoint *)dev->EndPoints[i];
        return NULL;
    }

    CCyUSBDevice *dev;
    CCyBulkEndPoint *epOut = NULL;
    CCyBulkEndPoint *epIn = NULL;
    uint16_t seq = 0;
};

static int cmdInfo(Bridge &b)
{
    UCHAR r[FRAME];
    if (b.command(CMD_GET_INFO, NULL, 0, r) < 52)
        return 1;
    const UCHAR *p = &r[HDR];
    uint16_t ver = rd16(p);
    printf("STM32 fw %d.%02d  SYSCLK %u MHz  UID ", ver >> 8, ver & 0xFF, rd32(p + 4) / 1000000);
    for (int i = 0; i < 12; i++)
        printf("%02X", p[8 + i]);
    printf("\n");
    const char *names[] = {"rx_frames", "tx_frames", "timeouts", "resyncs", "underruns",
                           "dropped_replies", "bad_frames", "uptime_ms"};
    for (int i = 0; i < 8; i++)
        printf("  %-16s %u\n", names[i], rd32(p + 20 + 4 * i));
    return 0;
}

static int cmdEcho(Bridge &b, int n)
{
    std::mt19937 rng(12345);
    UCHAR data[PAYLOAD_MAX], r[FRAME];
    DWORD t0 = GetTickCount();
    for (int i = 0; i < n; i++)
    {
        for (int k = 0; k < PAYLOAD_MAX; k++)
            data[k] = (UCHAR)rng();
        int len = b.command(CMD_ECHO, data, PAYLOAD_MAX, r);
        if (len != PAYLOAD_MAX || memcmp(&r[HDR], data, PAYLOAD_MAX) != 0)
        {
            printf("echo mismatch at frame %d\n", i);
            return 1;
        }
    }
    DWORD dt = GetTickCount() - t0;
    printf("echo OK: %d frames, %.1f round trips/s\n", n, dt ? n * 1000.0 / dt : 0.0);
    return 0;
}

static int cmdStream(Bridge &b, int n)
{
    UCHAR r[FRAME], p[4];
    wr32(p, (uint32_t)n);
    if (b.command(CMD_STREAM_START, p, 4, r) < 0)
        return 1;

    const int CHUNK = 64;                       // frames per XferData call
    std::vector<UCHAR> buf(CHUNK * FRAME);
    int got = 0;
    DWORD t0 = GetTickCount();
    while (got < n)
    {
        LONG want = (LONG)((n - got < CHUNK ? n - got : CHUNK) * FRAME);
        LONG len = want;
        if (!b.in()->XferData(buf.data(), len) || len == 0 || len % FRAME)
        {
            printf("stream read failed after %d frames (len %ld)\n", got, len);
            return 1;
        }
        for (LONG off = 0; off < len; off += FRAME)
        {
            const UCHAR *f = &buf[off];
            if (f[0] != 0x5A || f[1] != CMD_STREAM_DATA)
            {
                printf("unexpected frame in stream at %d\n", got);
                return 1;
            }
            uint32_t idx = rd32(&f[HDR]);
            if (idx != (uint32_t)got)
            {
                printf("stream index %u, expected %d\n", idx, got);
                return 1;
            }
            for (int i = 4; i < PAYLOAD_MAX; i++)
                if (f[HDR + i] != (UCHAR)(idx + i))
                {
                    printf("stream data error in frame %u byte %d\n", idx, i);
                    return 1;
                }
            got++;
        }
    }
    DWORD dt = GetTickCount() - t0;
    printf("stream OK: %d frames, %.2f MB/s\n", n, dt ? (double)n * FRAME / dt / 1000.0 : 0.0);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        printf("usage: CyBridgeTest list|status|reset|info|led on|off|echo [N]|stream [N]\n");
        return 1;
    }
    const char *op = argv[1];
    const char *arg = argc > 2 ? argv[2] : NULL;

    Bridge b;
    if (!strcmp(op, "list"))
    {
        b.list();
        return 0;
    }
    if (!b.open())
        return 1;

    if (!strcmp(op, "status"))
        b.status();
    else if (!strcmp(op, "reset"))
    {
        b.reset(true);
        printf("bridge and STM32 reset\n");
    }
    else if (!strcmp(op, "info"))
        return cmdInfo(b);
    else if (!strcmp(op, "led"))
    {
        UCHAR on = (arg && !strcmp(arg, "on")) ? 1 : 0, r[FRAME];
        return b.command(CMD_SET_LED, &on, 1, r) < 0;
    }
    else if (!strcmp(op, "echo"))
        return cmdEcho(b, arg ? atoi(arg) : 100);
    else if (!strcmp(op, "stream"))
        return cmdStream(b, arg ? atoi(arg) : 2000);
    else
    {
        printf("unknown command %s\n", op);
        return 1;
    }
    return 0;
}
