/*
 * board.c - clock, GPIO, SysTick and debug UART set-up
 */
#include "board.h"

uint32_t SystemCoreClock = 8000000UL;
static volatile uint32_t ms_ticks;

void SysTick_Handler(void)
{
    ms_ticks++;
}

uint32_t millis(void)
{
    return ms_ticks;
}

/* 72 MHz from the 8 MHz crystal (HSE x 9). Falls back to 64 MHz (HSI/2 x 16)
 * if the crystal does not start, so the board still works for debugging. */
static void clock_init(void)
{
    uint32_t n;

    RCC->CR |= RCC_CR_HSEON;
    for (n = 0; n < 500000UL && !(RCC->CR & RCC_CR_HSERDY); n++)
        ;

    FLASH->ACR = FLASH_ACR_PRFTBE | FLASH_ACR_LATENCY_2;

    if (RCC->CR & RCC_CR_HSERDY)
    {
        RCC->CFGR = RCC_CFGR_PLLSRC_HSE | RCC_CFGR_PLLMUL(9) | RCC_CFGR_PPRE1_DIV2;
        SystemCoreClock = 72000000UL;
    }
    else
    {
        RCC->CR &= ~RCC_CR_HSEON;
        RCC->CFGR = RCC_CFGR_PLLMUL(16) | RCC_CFGR_PPRE1_DIV2;
        SystemCoreClock = 64000000UL;
    }

    RCC->CR |= RCC_CR_PLLON;
    while (!(RCC->CR & RCC_CR_PLLRDY))
        ;
    RCC->CFGR |= RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & RCC_CFGR_SWS_MASK) != RCC_CFGR_SWS_PLL)
        ;
}

static void gpio_init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_AFIOEN | RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPBEN |
                    RCC_APB2ENR_IOPCEN | RCC_APB2ENR_IOPDEN | RCC_APB2ENR_IOPEEN |
                    RCC_APB2ENR_USART1EN;

    /* PE0..PE15: GPIF data bus, floating inputs until the FX2 reads */
    BUS_PORT->CRL = 0x44444444UL;
    BUS_PORT->CRH = 0x44444444UL;

    /* PC0 nWR, PC1 nRD, PC4 nSYNC: inputs with pull-up
     * PC2 ACK, PC3 DAV: push-pull outputs, start low */
    CTL_PORT->ODR = PIN_NWR | PIN_NRD | PIN_SYNC;
    CTL_PORT->CRL = (CTL_PORT->CRL & 0xFFF00000UL) |
                    (GPIO_IN_PULL      << 0)  |     /* PC0 */
                    (GPIO_IN_PULL      << 4)  |     /* PC1 */
                    (GPIO_OUT_PP_50MHZ << 8)  |     /* PC2 */
                    (GPIO_OUT_PP_50MHZ << 12) |     /* PC3 */
                    (GPIO_IN_PULL      << 16);      /* PC4 */

    /* PD12, PD13: LEDs */
    LED_PORT->BRR = LED_HEARTBEAT | LED_USER;
    LED_PORT->CRH = (LED_PORT->CRH & ~(0xFFUL << 16)) |
                    (GPIO_OUT_PP_2MHZ << 16) | (GPIO_OUT_PP_2MHZ << 20);

    /* PA9 USART1_TX (AF push-pull), PA10 USART1_RX (floating input) */
    GPIOA->CRH = (GPIOA->CRH & ~(0xFFUL << 4)) |
                 (GPIO_AF_PP_50MHZ << 4) | (GPIO_IN_FLOAT << 8);
}

static void uart_init(uint32_t baud)
{
    USART1->CR1 = 0;
    USART1->BRR = (uint16_t)((SystemCoreClock + baud / 2) / baud);
    USART1->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;
}

void board_init(void)
{
    clock_init();
    gpio_init();

    SysTick->LOAD = SystemCoreClock / 1000UL - 1UL;
    SysTick->VAL  = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE | SysTick_CTRL_TICKINT | SysTick_CTRL_ENABLE;

    uart_init(115200UL);
}

void led_set(uint32_t mask_on, uint32_t mask_off)
{
    LED_PORT->BSRR = mask_on | (mask_off << 16);
}

void led_toggle(uint32_t mask)
{
    LED_PORT->ODR ^= mask;
}

void uart_puts(const char *s)
{
    while (*s)
    {
        while (!(USART1->SR & USART_SR_TXE))
            ;
        USART1->DR = (uint16_t)*s++;
    }
}

void uart_put_hex32(uint32_t v)
{
    static const char hex[] = "0123456789ABCDEF";
    char buf[11];
    int i;

    buf[0] = '0';
    buf[1] = 'x';
    for (i = 0; i < 8; i++)
        buf[2 + i] = hex[(v >> (28 - 4 * i)) & 0xF];
    buf[10] = 0;
    uart_puts(buf);
}
