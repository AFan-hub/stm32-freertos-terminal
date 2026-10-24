#ifndef BSP_DELAY_H
#define BSP_DELAY_H

#include <stdint.h>

#include "stm32f4xx_hal.h"

/* Interrupt-independent busy-wait delays based on the DWT cycle counter.
 *
 * Header-only (static inline) on purpose: no project-file entry needed.
 * Needed because lcd_init() runs before the scheduler starts and must not
 * depend on the HAL tick (which turned out to be suspect on Day 1).
 * Works with interrupts fully masked; resolution = 1 CPU cycle.
 */

static inline void bsp_delay_init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static inline void bsp_delay_us(uint32_t us)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = us * (SystemCoreClock / 1000000U);

    while ((DWT->CYCCNT - start) < ticks)
    {
    }
}

static inline void bsp_delay_ms(uint32_t ms)
{
    /* per-ms chunks keep the 32-bit cycle counter from wrapping */
    for (uint32_t i = 0; i < ms; ++i)
    {
        bsp_delay_us(1000);
    }
}

#endif
