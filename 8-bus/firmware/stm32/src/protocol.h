/*
 * protocol.h - 512-byte frame protocol between the PC and the STM32
 *
 * Every USB bulk transfer is a whole number of 512-byte frames.
 *
 *   offset size  PC -> STM32 (EP2 OUT)        STM32 -> PC (EP6 IN)
 *   0      1     magic 0xA5                   magic 0x5A
 *   1      1     command                      command | 0x80 (reply) / 0xC4 (stream)
 *   2      2     sequence number (LE)         same sequence number / stream counter
 *   4      2     payload length (LE, <= 504)  payload length
 *   6      1     0                            status (0 = OK)
 *   7      1     0                            0
 *   8      504   payload                      payload
 */
#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

#define PROTO_MAGIC_H2D     0xA5u
#define PROTO_MAGIC_D2H     0x5Au
#define PROTO_HDR_SIZE      8u
#define PROTO_PAYLOAD_MAX   (512u - PROTO_HDR_SIZE)

#define CMD_ECHO            0x01u   /* reply with the same payload              */
#define CMD_SET_LED         0x02u   /* payload[0] bit0 = user LED               */
#define CMD_GET_INFO        0x03u   /* reply: version, clock, UID, counters     */
#define CMD_STREAM_START    0x04u   /* payload: uint32 frame count, 0 = endless */
#define CMD_STREAM_STOP     0x05u
#define CMD_REPLY_FLAG      0x80u
#define CMD_STREAM_DATA     0xC4u

#define ST_OK               0x00u
#define ST_BAD_CMD          0x01u
#define ST_BAD_LEN          0x02u

#define FW_VERSION          0x0200u /* 2.00 = 8-bit bus */

void protocol_init(void);
void protocol_on_frame(const uint8_t *frame);   /* called by gpif_slave.c */
void protocol_task(void);                       /* call from the main loop */

#endif /* PROTOCOL_H */
