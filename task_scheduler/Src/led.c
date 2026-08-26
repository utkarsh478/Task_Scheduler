/*
 * led.c  --  NUCLEO-F446RE (STM32F446RET6)
 *
 * Three fixes versus the original F407 Discovery version:
 *   1. GPIOD -> GPIOA. PD12..PD15 do not exist in the LQFP64 package.
 *   2. ODR read-modify-write -> BSRR. The old version was not atomic,
 *      which is a real race in a pre-emptive scheduler.
 *   3. Added volatile on every register access. Without it the
 *      compiler is free to cache, reorder, or delete MMIO writes.
 */

#include <stdint.h>
#include "led.h"

/* ------------------------------------------------------------------
 * Register map
 *
 *   RCC base    0x40023800
 *     AHB1ENR   +0x30  ->  0x40023830     bit 0 = GPIOAEN
 *
 *   GPIOA base  0x40020000
 *     MODER     +0x00   2 bits/pin   00 in, 01 out, 10 AF, 11 analog
 *     OTYPER    +0x04   1 bit/pin    0 push-pull, 1 open-drain
 *     OSPEEDR   +0x08   2 bits/pin
 *     PUPDR     +0x0C   2 bits/pin   00 none, 01 up, 10 down
 *     IDR       +0x10   read
 *     ODR       +0x14   read/write   (NOT atomic)
 *     BSRR      +0x18   write-only   low half SET, high half RESET
 * ------------------------------------------------------------------ */

#define RCC_AHB1ENR     (*(volatile uint32_t *)0x40023830UL)

#define GPIOA_MODER     (*(volatile uint32_t *)0x40020000UL)
#define GPIOA_OTYPER    (*(volatile uint32_t *)0x40020004UL)
#define GPIOA_OSPEEDR   (*(volatile uint32_t *)0x40020008UL)
#define GPIOA_PUPDR     (*(volatile uint32_t *)0x4002000CUL)
#define GPIOA_ODR       (*(volatile uint32_t *)0x40020014UL)
#define GPIOA_BSRR      (*(volatile uint32_t *)0x40020018UL)

#define RCC_GPIOAEN     (1UL << 0)


/* Configure one pin as a low-speed push-pull output. */
static void pin_as_output(uint32_t pin)
{
    GPIOA_MODER   &= ~(3UL << (pin * 2U));
    GPIOA_MODER   |=  (1UL << (pin * 2U));     /* 01 = output    */

    GPIOA_OTYPER  &= ~(1UL << pin);            /* push-pull      */

    GPIOA_OSPEEDR &= ~(3UL << (pin * 2U));     /* low speed:
                                                * an LED does not need
                                                * fast edges, and slow
                                                * edges radiate less */

    GPIOA_PUPDR   &= ~(3UL << (pin * 2U));     /* no pull        */
}


void led_init_all(void)
{
    RCC_AHB1ENR |= RCC_GPIOAEN;
    (void)RCC_AHB1ENR;          /* read-back: the RCC write needs a
                                 * couple of AHB cycles to land before
                                 * the first GPIOA access */

    pin_as_output(LED_GREEN);
    pin_as_output(LED_ORANGE);
    pin_as_output(LED_RED);
    pin_as_output(LED_BLUE);

    led_off(LED_GREEN);
    led_off(LED_ORANGE);
    led_off(LED_RED);
    led_off(LED_BLUE);
}


void led_on(uint8_t led_no)
{
    GPIOA_BSRR = (1UL << led_no);
}


void led_off(uint8_t led_no)
{
    GPIOA_BSRR = (1UL << ((uint32_t)led_no + 16U));
}


void led_toggle(uint8_t led_no)
{
    if (GPIOA_ODR & (1UL << led_no)) {
        GPIOA_BSRR = (1UL << ((uint32_t)led_no + 16U));
    } else {
        GPIOA_BSRR = (1UL << led_no);
    }
}


void delay(uint32_t count)
{
    for (volatile uint32_t i = 0U; i < count; i++) {}
}
