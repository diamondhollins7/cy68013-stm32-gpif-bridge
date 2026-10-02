/*
 * stm32f103_regs.h - minimal register definitions for STM32F103xE
 * (only the peripherals used by this project; RM0008 reference manual).
 *
 * Kept self-contained so the Keil project does not depend on a particular
 * StdPeriph/HAL/CMSIS-pack version. If you prefer, replace this file with the
 * CMSIS "stm32f10x.h" - the names used here are the same.
 */
#ifndef STM32F103_REGS_H
#define STM32F103_REGS_H

#include <stdint.h>

#define __IO volatile

typedef struct
{
    __IO uint32_t CRL;
    __IO uint32_t CRH;
    __IO uint32_t IDR;
    __IO uint32_t ODR;
    __IO uint32_t BSRR;
    __IO uint32_t BRR;
    __IO uint32_t LCKR;
} GPIO_TypeDef;

typedef struct
{
    __IO uint32_t EVCR;
    __IO uint32_t MAPR;
    __IO uint32_t EXTICR[4];
    uint32_t      RESERVED0;
    __IO uint32_t MAPR2;
} AFIO_TypeDef;

typedef struct
{
    __IO uint32_t CR;
    __IO uint32_t CFGR;
    __IO uint32_t CIR;
    __IO uint32_t APB2RSTR;
    __IO uint32_t APB1RSTR;
    __IO uint32_t AHBENR;
    __IO uint32_t APB2ENR;
    __IO uint32_t APB1ENR;
    __IO uint32_t BDCR;
    __IO uint32_t CSR;
} RCC_TypeDef;

typedef struct
{
    __IO uint32_t ACR;
    __IO uint32_t KEYR;
    __IO uint32_t OPTKEYR;
    __IO uint32_t SR;
    __IO uint32_t CR;
    __IO uint32_t AR;
    __IO uint32_t RESERVED;
    __IO uint32_t OBR;
    __IO uint32_t WRPR;
} FLASH_TypeDef;

typedef struct
{
    __IO uint16_t SR;   uint16_t RESERVED0;
    __IO uint16_t DR;   uint16_t RESERVED1;
    __IO uint16_t BRR;  uint16_t RESERVED2;
    __IO uint16_t CR1;  uint16_t RESERVED3;
    __IO uint16_t CR2;  uint16_t RESERVED4;
    __IO uint16_t CR3;  uint16_t RESERVED5;
    __IO uint16_t GTPR; uint16_t RESERVED6;
} USART_TypeDef;

typedef struct
{
    __IO uint32_t CTRL;
    __IO uint32_t LOAD;
    __IO uint32_t VAL;
    __IO uint32_t CALIB;
} SysTick_Type;

#define AFIO        ((AFIO_TypeDef  *) 0x40010000UL)
#define GPIOA       ((GPIO_TypeDef  *) 0x40010800UL)
#define GPIOB       ((GPIO_TypeDef  *) 0x40010C00UL)
#define GPIOC       ((GPIO_TypeDef  *) 0x40011000UL)
#define GPIOD       ((GPIO_TypeDef  *) 0x40011400UL)
#define GPIOE       ((GPIO_TypeDef  *) 0x40011800UL)
#define USART1      ((USART_TypeDef *) 0x40013800UL)
#define RCC         ((RCC_TypeDef   *) 0x40021000UL)
#define FLASH       ((FLASH_TypeDef *) 0x40022000UL)
#define SysTick     ((SysTick_Type  *) 0xE000E010UL)

/* 96-bit unique device ID */
#define UID_BASE    0x1FFFF7E8UL

/* RCC_CR */
#define RCC_CR_HSION        (1UL << 0)
#define RCC_CR_HSIRDY       (1UL << 1)
#define RCC_CR_HSEON        (1UL << 16)
#define RCC_CR_HSERDY       (1UL << 17)
#define RCC_CR_PLLON        (1UL << 24)
#define RCC_CR_PLLRDY       (1UL << 25)

/* RCC_CFGR */
#define RCC_CFGR_SW_MASK    (3UL << 0)
#define RCC_CFGR_SW_PLL     (2UL << 0)
#define RCC_CFGR_SWS_MASK   (3UL << 2)
#define RCC_CFGR_SWS_PLL    (2UL << 2)
#define RCC_CFGR_PPRE1_DIV2 (4UL << 8)
#define RCC_CFGR_PLLSRC_HSE (1UL << 16)
#define RCC_CFGR_PLLMUL(x)  ((uint32_t)((x) - 2) << 18)

/* RCC_APB2ENR */
#define RCC_APB2ENR_AFIOEN   (1UL << 0)
#define RCC_APB2ENR_IOPAEN   (1UL << 2)
#define RCC_APB2ENR_IOPBEN   (1UL << 3)
#define RCC_APB2ENR_IOPCEN   (1UL << 4)
#define RCC_APB2ENR_IOPDEN   (1UL << 5)
#define RCC_APB2ENR_IOPEEN   (1UL << 6)
#define RCC_APB2ENR_USART1EN (1UL << 14)

/* FLASH_ACR */
#define FLASH_ACR_LATENCY_2  (2UL << 0)
#define FLASH_ACR_PRFTBE     (1UL << 4)

/* USART */
#define USART_SR_TXE        (1U << 7)
#define USART_SR_TC         (1U << 6)
#define USART_SR_RXNE       (1U << 5)
#define USART_CR1_UE        (1U << 13)
#define USART_CR1_TE        (1U << 3)
#define USART_CR1_RE        (1U << 2)

/* SysTick */
#define SysTick_CTRL_ENABLE    (1UL << 0)
#define SysTick_CTRL_TICKINT   (1UL << 1)
#define SysTick_CTRL_CLKSOURCE (1UL << 2)

/* GPIO CRL/CRH nibbles (MODE + CNF) */
#define GPIO_IN_ANALOG      0x0u
#define GPIO_IN_FLOAT       0x4u
#define GPIO_IN_PULL        0x8u    /* pull-up/down selected by ODR */
#define GPIO_OUT_PP_2MHZ    0x2u
#define GPIO_OUT_PP_50MHZ   0x3u
#define GPIO_AF_PP_50MHZ    0xBu

#if defined(__CC_ARM)               /* Arm Compiler 5 */
#define __NOP()             __nop()
#else                               /* Arm Compiler 6 (armclang), GCC */
#define __NOP()             __asm volatile ("nop")
#endif

#endif /* STM32F103_REGS_H */
