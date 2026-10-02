/*
 * gpif_slave.h - STM32 side of the FX2 GPIF link (8-bit, 4-wire handshake)
 */
#ifndef GPIF_SLAVE_H
#define GPIF_SLAVE_H

#include <stdint.h>

#define FRAME_BYTES     512u                /* one frame = one HS USB packet */
#define TXQ_DEPTH       8u                  /* frames waiting for the FX2    */

typedef struct
{
    uint32_t rx_frames;     /* frames received from the FX2 (PC -> STM32)   */
    uint32_t tx_frames;     /* frames read by the FX2   (STM32 -> PC)       */
    uint32_t timeouts;      /* strobe did not return within the timeout     */
    uint32_t resyncs;       /* nSYNC pulses / error recoveries              */
    uint32_t underruns;     /* FX2 read with no data queued (filler sent)   */
} gpif_stats_t;

extern gpif_stats_t gpif_stats;

void      gpif_slave_init(void);
void      gpif_slave_poll(void);        /* call as often as possible         */

uint32_t  gpif_tx_free(void);           /* free frame slots in the TX queue  */
uint8_t  *gpif_tx_alloc(void);          /* NULL if full; fill FRAME_BYTES     */
void      gpif_tx_commit(void);         /* queue the frame from gpif_tx_alloc */

#endif /* GPIF_SLAVE_H */
