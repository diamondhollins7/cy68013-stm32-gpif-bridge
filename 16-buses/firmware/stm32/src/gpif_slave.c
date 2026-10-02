/*
 * gpif_slave.c - STM32F103 slave for the FX2 GPIF master
 *
 * Handshake per 16-bit word (see docs/GPIF_interface.md):
 *
 *   FX2 write:  FX2 drives FD, nWR=0  ->  STM32 latches PE[15:0], ACK=1
 *               FX2 nWR=1             ->  STM32 ACK=0
 *
 *   FX2 read:   FX2 nRD=0             ->  STM32 drives PE[15:0], ACK=1
 *               FX2 samples, nRD=1    ->  STM32 releases bus, ACK=0
 *
 *   DAV (RDY1) is high while a 512-byte frame is waiting to be read.
 *   nSYNC (CTL2) low = FX2 has reset its FIFOs: drop partial frames.
 *
 * Everything is polled: the FX2 waits for ACK, so the STM32 may be busy for a
 * while (e.g. processing a frame) without losing data.
 */
#include <stddef.h>
#include "board.h"
#include "gpif_slave.h"
#include "protocol.h"

/* ~5-10 ms at 72 MHz. A strobe that stays asserted longer means the FX2
 * was reset or its GPIF was aborted. */
#define STROBE_TIMEOUT  100000UL

gpif_stats_t gpif_stats;

static uint16_t rx_buf[FRAME_WORDS];
static uint32_t rx_idx;

static uint16_t txq[TXQ_DEPTH][FRAME_WORDS];
static uint32_t txq_head;               /* next frame to send (FX2 side)   */
static uint32_t txq_tail;               /* next frame to fill (app side)   */
static const uint16_t *tx_ptr;
static uint32_t tx_left;                /* words left in the current frame */
static uint8_t  sync_seen;

static void bus_drive(void)
{
    BUS_PORT->CRL = 0x33333333UL;       /* output push-pull 50 MHz */
    BUS_PORT->CRH = 0x33333333UL;
}

static void bus_release(void)
{
    BUS_PORT->CRL = 0x44444444UL;       /* floating input */
    BUS_PORT->CRH = 0x44444444UL;
}

static int wait_strobe_released(uint32_t pin)
{
    uint32_t n = STROBE_TIMEOUT;

    while (!(CTL_PORT->IDR & pin))
    {
        if (--n == 0)
            return 0;
    }
    return 1;
}

/* Drop partial RX frame and everything queued for TX. */
static void resync(void)
{
    bus_release();
    CTL_PORT->BRR = PIN_ACK | PIN_DAV;
    rx_idx   = 0;
    tx_left  = 0;
    tx_ptr   = NULL;
    txq_head = txq_tail;
    gpif_stats.resyncs++;
}

void gpif_slave_init(void)
{
    bus_release();
    CTL_PORT->BRR = PIN_ACK | PIN_DAV;
    rx_idx = 0;
    tx_left = 0;
    tx_ptr = NULL;
    txq_head = txq_tail = 0;
    sync_seen = 0;
}

uint32_t gpif_tx_free(void)
{
    return TXQ_DEPTH - (txq_tail - txq_head);
}

uint8_t *gpif_tx_alloc(void)
{
    if (gpif_tx_free() == 0)
        return NULL;
    return (uint8_t *)txq[txq_tail % TXQ_DEPTH];
}

void gpif_tx_commit(void)
{
    if (gpif_tx_free() != 0)
        txq_tail++;
}

/* FX2 -> STM32: one word */
static void service_write(void)
{
    uint16_t w = (uint16_t)BUS_PORT->IDR;   /* data was set up before nWR fell */

    CTL_PORT->BSRR = PIN_ACK;
    if (!wait_strobe_released(PIN_NWR))
    {
        gpif_stats.timeouts++;
        resync();
        return;
    }
    CTL_PORT->BRR = PIN_ACK;

    rx_buf[rx_idx++] = w;
    if (rx_idx == FRAME_WORDS)
    {
        rx_idx = 0;
        gpif_stats.rx_frames++;
        protocol_on_frame((const uint8_t *)rx_buf);
    }
}

/* STM32 -> FX2: one word */
static void service_read(void)
{
    uint16_t w = tx_left ? *tx_ptr : 0xFFFFu;

    BUS_PORT->ODR = w;
    bus_drive();
    __NOP();                                /* let the bus settle a little */
    __NOP();
    __NOP();
    __NOP();
    CTL_PORT->BSRR = PIN_ACK;

    if (!wait_strobe_released(PIN_NRD))
    {
        gpif_stats.timeouts++;
        resync();
        return;
    }
    bus_release();                          /* release before ACK falls    */

    if (tx_left)
    {
        tx_ptr++;
        if (--tx_left == 0)
        {
            CTL_PORT->BRR = PIN_DAV;        /* DAV low before ACK low      */
            txq_head++;
            gpif_stats.tx_frames++;
        }
    }
    else
    {
        gpif_stats.underruns++;
    }
    CTL_PORT->BRR = PIN_ACK;
}

void gpif_slave_poll(void)
{
    uint32_t ctl = CTL_PORT->IDR;

    if (!(ctl & PIN_SYNC))
    {
        if (!sync_seen)
        {
            sync_seen = 1;
            resync();
        }
        return;
    }
    sync_seen = 0;

    /* Both strobes low: FX2 is in reset / not configured - ignore. */
    if ((ctl & (PIN_NWR | PIN_NRD)) == 0)
        return;

    if (!(ctl & PIN_NWR))
    {
        service_write();
    }
    else if (!(ctl & PIN_NRD))
    {
        service_read();
    }
    else if (tx_left == 0 && txq_head != txq_tail)
    {
        tx_ptr  = txq[txq_head % TXQ_DEPTH];
        tx_left = FRAME_WORDS;
        CTL_PORT->BSRR = PIN_DAV;           /* tell the FX2 a frame is ready */
    }
}
