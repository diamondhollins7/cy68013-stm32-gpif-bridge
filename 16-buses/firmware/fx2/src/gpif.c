/*
 * gpif.c - GPIF master configuration for the FX2LP -> STM32F103 parallel link
 *
 * 16-bit bus FD[15:0], fully handshaked (asynchronous) so the STM32 can serve
 * it with plain GPIO polling. One GPIF "transaction" = one 16-bit word.
 *
 *   FIFO write (EP2 OUT -> STM32), waveform 1:
 *     S0  drive data, strobes idle (2 IFCLK setup)
 *     S1  drive data, nWR=0, wait until ACK=1
 *     S2  NEXT (advance FIFO), nWR=1
 *     S3  wait until ACK=0 -> IDLE
 *
 *   FIFO read (STM32 -> EP6 IN), waveform 0:
 *     S0  nRD=0, wait until ACK=1   (STM32 drives bus, then raises ACK)
 *     S1  nRD=0, sample data into FIFO
 *     S2  nRD=1, wait until ACK=0 -> IDLE  (STM32 releases bus first)
 *
 * The GPIF re-runs the waveform until the transaction count (GPIFTCB) expires.
 */
#include "bridge.h"

/* Waveform descriptor layout (32 bytes each):
 *   [0..6] LENGTH/BRANCH  S0..S6   [7] unused
 *   [8..14] OPCODE        S0..S6   [15] unused
 *   [16..22] OUTPUT        S0..S6   [23] unused
 *   [24..30] LOGIC FUNC    S0..S6   [31] unused
 *
 * OPCODE bits : 0x01 DP (decision point), 0x02 DATA, 0x04 NEXT, 0x08 INCAD, 0x10 GINT, 0x20 SGL
 * OUTPUT bits : bit0 CTL0 (nWR), bit1 CTL1 (nRD), bit2 CTL2 (nSYNC)    (TRICTL = 0)
 * DP branch   : bits[5:3] = state if logic true, bits[2:0] = state if false, 7 = IDLE
 * LOGIC FUNC  : [7:6] function (00 = AND), [5:3] TERMA, [2:0] TERMB; 0 = RDY0, 1 = RDY1
 */
#define DP_BRANCH(t, f)     (((t) << 3) | (f))
#define S_IDLE              7
#define LF_RDY0             0x00            /* RDY0 AND RDY0 */

#define OP_DP               0x01
#define OP_DATA             0x02
#define OP_NEXT             0x04

#define OUT_IDLE            0x07            /* nSYNC=1 nRD=1 nWR=1 */
#define OUT_WR              0x06            /* nWR = 0             */
#define OUT_RD              0x05            /* nRD = 0             */

static BYTE code wave_data[128] =
{
    /* ---- Wave 0 : FIFO READ (STM32 -> FX2) --------------------------- */
    /* LenBr */ DP_BRANCH(1, 0), 1, DP_BRANCH(2, S_IDLE), 1, 1, 1, 1, 0x07,
    /* Opcode*/ OP_DP, OP_DATA, OP_DP, 0, 0, 0, 0, 0x00,
    /* Output*/ OUT_RD, OUT_RD, OUT_IDLE, OUT_IDLE, OUT_IDLE, OUT_IDLE, OUT_IDLE, OUT_IDLE,
    /* LogFn */ LF_RDY0, 0, LF_RDY0, 0, 0, 0, 0, 0x3F,

    /* ---- Wave 1 : FIFO WRITE (FX2 -> STM32) -------------------------- */
    /* LenBr */ 2, DP_BRANCH(2, 1), 1, DP_BRANCH(3, S_IDLE), 1, 1, 1, 0x07,
    /* Opcode*/ OP_DATA, OP_DP | OP_DATA, OP_NEXT | OP_DATA, OP_DP, 0, 0, 0, 0x00,
    /* Output*/ OUT_IDLE, OUT_WR, OUT_IDLE, OUT_IDLE, OUT_IDLE, OUT_IDLE, OUT_IDLE, OUT_IDLE,
    /* LogFn */ 0, LF_RDY0, 0, LF_RDY0, 0, 0, 0, 0x3F,

    /* ---- Wave 2 : SINGLE READ  (unused - returns to IDLE at once) ---- */
    /* LenBr */ DP_BRANCH(S_IDLE, S_IDLE), 1, 1, 1, 1, 1, 1, 0x07,
    /* Opcode*/ OP_DP, 0, 0, 0, 0, 0, 0, 0x00,
    /* Output*/ OUT_IDLE, OUT_IDLE, OUT_IDLE, OUT_IDLE, OUT_IDLE, OUT_IDLE, OUT_IDLE, OUT_IDLE,
    /* LogFn */ LF_RDY0, 0, 0, 0, 0, 0, 0, 0x3F,

    /* ---- Wave 3 : SINGLE WRITE (unused - returns to IDLE at once) ---- */
    /* LenBr */ DP_BRANCH(S_IDLE, S_IDLE), 1, 1, 1, 1, 1, 1, 0x07,
    /* Opcode*/ OP_DP, 0, 0, 0, 0, 0, 0, 0x00,
    /* Output*/ OUT_IDLE, OUT_IDLE, OUT_IDLE, OUT_IDLE, OUT_IDLE, OUT_IDLE, OUT_IDLE, OUT_IDLE,
    /* LogFn */ LF_RDY0, 0, 0, 0, 0, 0, 0, 0x3F
};

void gpif_init(void)
{
    BYTE i;

    GPIFABORT = 0xFF;               /* make sure the engine is idle      */
    SYNCDELAY;

    GPIFREADYCFG = 0x00;            /* RDYx synchronised, no INTRDY/TC   */
    SYNCDELAY;
    GPIFCTLCFG = 0x00;              /* CTL0..2 CMOS outputs, TRICTL = 0  */
    SYNCDELAY;
    GPIFIDLECS = 0x00;              /* tri-state FD[15:0] when idle      */
    SYNCDELAY;
    GPIFIDLECTL = CTL_IDLE;         /* strobes high when idle            */
    SYNCDELAY;
    /* SINGLEWR = wave3, SINGLERD = wave2, FIFOWR = wave1, FIFORD = wave0 */
    GPIFWFSELECT = 0xE4;
    SYNCDELAY;
    FLOWSTATE = 0x00;               /* no flow states                    */
    SYNCDELAY;
    GPIFADRH = 0x00;
    SYNCDELAY;
    GPIFADRL = 0x00;
    SYNCDELAY;

    for (i = 0; i < 128; i++)
        GPIF_WAVE_DATA[i] = wave_data[i];

    EP2GPIFFLGSEL = 0x01;           /* FIFOFLAG = empty flag (unused)    */
    SYNCDELAY;
    EP2GPIFPFSTOP = 0x00;
    SYNCDELAY;
    EP6GPIFFLGSEL = 0x02;           /* FIFOFLAG = full flag (unused)     */
    SYNCDELAY;
    EP6GPIFPFSTOP = 0x00;
    SYNCDELAY;
}

static void gpif_set_tc(WORD words)
{
    GPIFTCB3 = 0;
    SYNCDELAY;
    GPIFTCB2 = 0;
    SYNCDELAY;
    GPIFTCB1 = (BYTE)(words >> 8);
    SYNCDELAY;
    GPIFTCB0 = (BYTE)words;
    SYNCDELAY;
}

/* Send 'words' 16-bit words from the EP2 OUT FIFO to the STM32 (non-blocking). */
void gpif_start_write_ep2(WORD words)
{
    gpif_set_tc(words);
    GPIFTRIG = GPIF_EP2;            /* RW = 0 -> write */
}

/* Read 'words' 16-bit words from the STM32 into the EP6 IN FIFO (non-blocking). */
void gpif_start_read_ep6(WORD words)
{
    gpif_set_tc(words);
    GPIFTRIG = bmGPIFREAD | GPIF_EP6;
}

void gpif_abort(void)
{
    GPIFABORT = 0xFF;
    SYNCDELAY;
    while (!(GPIFTRIG & bmGPIFDONE))
        ;
}

/* Pulse CTL2 (nSYNC) low for ~1 ms while the GPIF is idle. The STM32 then
 * drops any partially received/sent frame, so both sides are frame-aligned. */
void gpif_pulse_sync(void)
{
    GPIFIDLECTL = CTL_SYNC_ASSERTED;
    SYNCDELAY;
    delay_ms(1);
    GPIFIDLECTL = CTL_IDLE;
    SYNCDELAY;
}
