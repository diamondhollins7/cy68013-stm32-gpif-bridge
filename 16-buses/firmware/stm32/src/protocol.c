/*
 * protocol.c - command handling and test-pattern streaming
 */
#include <string.h>
#include "board.h"
#include "gpif_slave.h"
#include "protocol.h"

static uint32_t stream_remaining;      /* 0xFFFFFFFF = endless */
static uint16_t stream_seq;
static uint32_t stream_index;
static uint32_t dropped_replies;
static uint32_t bad_frames;

static void put_u16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static void put_u32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static uint32_t get_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint8_t *frame_begin(uint8_t cmd, uint16_t seq, uint16_t len, uint8_t status)
{
    uint8_t *f = gpif_tx_alloc();

    if (f == NULL)
        return NULL;
    memset(f, 0, FRAME_BYTES);
    f[0] = PROTO_MAGIC_D2H;
    f[1] = cmd;
    put_u16(&f[2], seq);
    put_u16(&f[4], len);
    f[6] = status;
    return f;
}

static void reply(uint8_t cmd, uint16_t seq, uint8_t status,
                  const uint8_t *payload, uint16_t len)
{
    uint8_t *f = frame_begin((uint8_t)(cmd | CMD_REPLY_FLAG), seq, len, status);

    if (f == NULL)
    {
        dropped_replies++;              /* PC is not reading EP6 */
        return;
    }
    if (len)
        memcpy(&f[PROTO_HDR_SIZE], payload, len);
    gpif_tx_commit();
}

static uint16_t build_info(uint8_t *p)
{
    const uint8_t *uid = (const uint8_t *)UID_BASE;

    put_u16(&p[0], FW_VERSION);
    put_u16(&p[2], 0);
    put_u32(&p[4], SystemCoreClock);
    memcpy(&p[8], uid, 12);
    put_u32(&p[20], gpif_stats.rx_frames);
    put_u32(&p[24], gpif_stats.tx_frames);
    put_u32(&p[28], gpif_stats.timeouts);
    put_u32(&p[32], gpif_stats.resyncs);
    put_u32(&p[36], gpif_stats.underruns);
    put_u32(&p[40], dropped_replies);
    put_u32(&p[44], bad_frames);
    put_u32(&p[48], millis());
    return 52;
}

void protocol_init(void)
{
    stream_remaining = 0;
    stream_seq = 0;
    stream_index = 0;
    dropped_replies = 0;
    bad_frames = 0;
}

void protocol_on_frame(const uint8_t *f)
{
    uint8_t  cmd = f[1];
    uint16_t seq = (uint16_t)(f[2] | (f[3] << 8));
    uint16_t len = (uint16_t)(f[4] | (f[5] << 8));
    const uint8_t *payload = &f[PROTO_HDR_SIZE];
    uint8_t  info[64];

    if (f[0] != PROTO_MAGIC_H2D)
    {
        bad_frames++;
        return;
    }
    if (len > PROTO_PAYLOAD_MAX)
    {
        reply(cmd, seq, ST_BAD_LEN, NULL, 0);
        return;
    }

    switch (cmd)
    {
    case CMD_ECHO:
        reply(cmd, seq, ST_OK, payload, len);
        break;

    case CMD_SET_LED:
        if (len >= 1 && (payload[0] & 0x01))
            led_set(LED_USER, 0);
        else
            led_set(0, LED_USER);
        reply(cmd, seq, ST_OK, NULL, 0);
        break;

    case CMD_GET_INFO:
        reply(cmd, seq, ST_OK, info, build_info(info));
        break;

    case CMD_STREAM_START:
        stream_remaining = (len >= 4) ? get_u32(payload) : 0;
        if (stream_remaining == 0)
            stream_remaining = 0xFFFFFFFFUL;
        stream_seq = 0;
        stream_index = 0;
        reply(cmd, seq, ST_OK, NULL, 0);
        break;

    case CMD_STREAM_STOP:
        stream_remaining = 0;
        reply(cmd, seq, ST_OK, NULL, 0);
        break;

    default:
        reply(cmd, seq, ST_BAD_CMD, NULL, 0);
        break;
    }
}

/* Stream frames: payload = uint32 index followed by bytes (index + i). The
 * last queue slot is kept free so command replies are never blocked. */
void protocol_task(void)
{
    uint8_t *f;
    uint32_t i;

    if (stream_remaining == 0 || gpif_tx_free() < 2)
        return;

    f = frame_begin(CMD_STREAM_DATA, stream_seq, PROTO_PAYLOAD_MAX, ST_OK);
    if (f == NULL)
        return;
    put_u32(&f[PROTO_HDR_SIZE], stream_index);
    for (i = 4; i < PROTO_PAYLOAD_MAX; i++)
        f[PROTO_HDR_SIZE + i] = (uint8_t)(stream_index + i);
    gpif_tx_commit();

    stream_seq++;
    stream_index++;
    if (stream_remaining != 0xFFFFFFFFUL)
        stream_remaining--;
}
