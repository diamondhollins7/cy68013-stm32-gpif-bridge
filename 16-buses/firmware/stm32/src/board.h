/*
 * board.h - pin assignment of the FX2-STM32 bridge board (STM32F103VET6, LQFP100)
 */
#ifndef BOARD_H
#define BOARD_H

#include "stm32f103_regs.h"

/* GPIF data bus: FX2 FD[15:0] <-> PE[15:0] (33R series resistors) */
#define BUS_PORT        GPIOE

/* GPIF control lines on port C */
#define CTL_PORT        GPIOC
#define PIN_NWR         (1u << 0)   /* PC0  <- FX2 CTL0, write strobe, active low */
#define PIN_NRD         (1u << 1)   /* PC1  <- FX2 CTL1, read  strobe, active low */
#define PIN_ACK         (1u << 2)   /* PC2  -> FX2 RDY0, word handshake           */
#define PIN_DAV         (1u << 3)   /* PC3  -> FX2 RDY1, data available           */
#define PIN_SYNC        (1u << 4)   /* PC4  <- FX2 CTL2, re-sync, active low      */

/* LEDs on port D (active high) */
#define LED_PORT        GPIOD
#define LED_HEARTBEAT   (1u << 12)  /* PD12 - D4 */
#define LED_USER        (1u << 13)  /* PD13 - D5, controlled from the PC */

/* Debug UART: USART1 PA9 (TX) / PA10 (RX), 115200 8N1 */

extern uint32_t SystemCoreClock;

void     board_init(void);
uint32_t millis(void);
void     led_set(uint32_t mask_on, uint32_t mask_off);
void     led_toggle(uint32_t mask);
void     uart_puts(const char *s);
void     uart_put_hex32(uint32_t v);

#endif /* BOARD_H */
