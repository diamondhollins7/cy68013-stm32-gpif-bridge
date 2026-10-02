/*
 * main.c - STM32F103VET6 GPIF slave for the CY7C68013A (FX2LP) USB bridge
 *
 * Toolchain: Keil MDK-ARM (Arm Compiler 6 or 5), no HAL / StdPeriph needed.
 */
#include "board.h"
#include "gpif_slave.h"
#include "protocol.h"

int main(void)
{
    uint32_t last_blink = 0;

    board_init();
    gpif_slave_init();
    protocol_init();

    uart_puts("\r\nSTM32F103 GPIF slave v1.00, SYSCLK = ");
    uart_put_hex32(SystemCoreClock);
    uart_puts("\r\n");

    for (;;)
    {
        gpif_slave_poll();
        protocol_task();

        if (millis() - last_blink >= 500u)
        {
            last_blink = millis();
            led_toggle(LED_HEARTBEAT);
        }
    }
}
