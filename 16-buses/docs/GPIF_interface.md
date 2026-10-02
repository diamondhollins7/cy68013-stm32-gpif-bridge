# GPIF interface specification

FX2LP (CY7C68013A-56PVXC) is the **GPIF master**; STM32F103VET6 is the **slave**.
The bus is 16 bits wide and fully handshaked. Each GPIF transaction moves one
16-bit word, and the STM32 serves it by polling GPIOs. This means there are no
cycle-accurate timing requirements on the STM32 side.

## Signals

| Signal | FX2 pin (56-SSOP) | STM32 pin (LQFP100) | Driven by | Active | Meaning |
|---|---|---|---|---|---|
| FD[7:0]  | PB0..PB7 = 25..32 | PE0..PE7 = 97,98,1,2,3,4,5,38 | both | – | data low byte = 1st byte of the USB stream |
| FD[15:8] | PD0..PD7 = 52,53,54,55,56,1,2,3 | PE8..PE15 = 39..46 | both | – | data high byte = 2nd byte |
| nWR   | CTL0 = 36 | PC0 = 15 | FX2   | low  | write strobe: FD holds a valid word |
| nRD   | CTL1 = 37 | PC1 = 16 | FX2   | low  | read strobe: STM32 must drive FD |
| nSYNC | CTL2 = 38 | PC4 = 33 | FX2   | low  | resynchronise: STM32 drops partial frames and its TX queue |
| ACK   | RDY0 = 8  | PC2 = 17 | STM32 | high | word handshake |
| DAV   | RDY1 = 9  | PC3 = 18 | STM32 | high | a 512-byte frame is waiting to be read |
| NRST  | PA0 = 40 (via R16 100 Ω) | NRST = 14 | FX2 | low | STM32 reset (open-drain emulation) |

The data lines have 33 Ω series resistors (RN1..RN4). The control lines are biased:
10 k pull-ups on nWR/nRD/nSYNC and 10 k pull-downs on ACK/DAV.

## Word write (FX2 → STM32), GPIF waveform 1 (FIFO write)

```
           S0      S1 (DP)           S2        S3 (DP)
FD     ===X=== word n =================X=== word n+1 ...
nWR    ‾‾‾‾‾‾‾‾\_______________________/‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾
ACK    ____________________/‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾\__________
                    STM32 reads PE,       STM32 sees nWR=1,
                    sets ACK=1            clears ACK -> IDLE
```

| State | LEN/BR | OPCODE | OUTPUT (CTL2..0) | LOGIC | Action |
|---|---|---|---|---|---|
| S0 | 2 cycles | DATA | 111 | – | drive word, setup time |
| S1 | DP: 1→S2, 0→S1 | DP+DATA | 110 (nWR=0) | RDY0 | wait for ACK=1 |
| S2 | 1 cycle | NEXT+DATA | 111 | – | strobe released, next FIFO word |
| S3 | DP: 1→S3, 0→IDLE | DP | 111 | RDY0 | wait for ACK=0 |

## Word read (STM32 → FX2), GPIF waveform 0 (FIFO read)

```
           S0 (DP)                S1   S2 (DP)
nRD    ‾‾‾‾\_____________________________/‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾
FD     ---------<===== STM32 drives ======>--------------- (Z)
ACK    _________________/‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾\_________
```

| State | LEN/BR | OPCODE | OUTPUT (CTL2..0) | LOGIC | Action |
|---|---|---|---|---|---|
| S0 | DP: 1→S1, 0→S0 | DP | 101 (nRD=0) | RDY0 | wait for ACK=1 (data valid) |
| S1 | 1 cycle | DATA | 101 | – | sample FD into the EP6 FIFO |
| S2 | DP: 1→S2, 0→IDLE | DP | 111 | RDY0 | wait for ACK=0 (bus released) |

The STM32 releases the bus (switches PE back to input) **before** it drops ACK.
The FX2 does not finish a read until ACK is low. Because of this ordering, the two
chips never drive FD at the same time.

`IFCONFIG = 0xCA`: internal 48 MHz IFCLK, asynchronous GPIF (RDY inputs are
synchronised), GPIF master mode. `GPIFIDLECTL = 0x07` keeps all strobes high when idle.

## Scheduling (FX2 firmware `bridge_poll`)

1. If the GPIF is busy, do nothing.
2. If **DAV = 1** and the EP6 FIFO is not full, start a FIFO read of one USB
   packet: 256 words at high speed, 32 words at full speed. EP6 is AUTOIN, so the
   packet is committed to USB automatically.
3. Otherwise, if the EP2 FIFO holds an OUT packet, start a FIFO write of
   `byte count / 2` words. EP2 is AUTOOUT, so the buffer is released automatically.

## Frame protocol (PC ↔ STM32)

Every frame is 512 bytes, which is one high-speed packet. At full speed a frame
is 8 packets. The host must only send whole 512-byte frames, and never zero-length
packets.

| Offset | Size | PC → STM32 | STM32 → PC |
|---|---|---|---|
| 0 | 1 | magic `0xA5` | magic `0x5A` |
| 1 | 1 | command | command \| 0x80 (reply), `0xC4` (stream data) |
| 2 | 2 | sequence (LE) | same sequence / stream counter |
| 4 | 2 | payload length (≤ 504) | payload length |
| 6 | 1 | 0 | status (0 = OK, 1 = bad command, 2 = bad length) |
| 7 | 1 | 0 | 0 |
| 8 | 504 | payload | payload |

| Command | Code | Payload | Reply |
|---|---|---|---|
| ECHO | 0x01 | any | same payload |
| SET_LED | 0x02 | byte 0 bit 0 = LED D5 | – |
| GET_INFO | 0x03 | – | version, SYSCLK, 96-bit UID, counters |
| STREAM_START | 0x04 | uint32 frame count (0 = endless) | – then stream frames `0xC4` |
| STREAM_STOP | 0x05 | – | – |

Stream frame payload: a uint32 frame index, followed by bytes `(index + i) & 0xFF`
for i = 4..503. `tools/host_test.py stream` verifies every frame.

## FX2 control requests

| bmRequestType | bRequest | wValue | Data | Function |
|---|---|---|---|---|
| 0xC0 | 0xB0 | 0 | 8 bytes IN | status: fw version, GPIFREADYSTAT, GPIFTRIG, FIFO flags, HS, configuration |
| 0x40 | 0xB1 | bit0 = reset STM32 | – | abort GPIF, (reset STM32), pulse nSYNC, flush FIFOs |
| 0xC0 | 0x20 | 0 | 40 bytes IN | Microsoft OS 1.0 compat ID "WINUSB" (wIndex = 4) |

## Expected throughput

Each word needs two handshake round trips. Each round trip includes the STM32
polling latency plus 2–3 IFCLK cycles of RDY synchronisation in the FX2, so the
expected rate is roughly **2–4 MB/s** in each direction. This figure is an
estimate, not a measurement. `host_test.py stream` prints the real figure.
