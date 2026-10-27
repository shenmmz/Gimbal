#include "stm32f4xx_hal.h"

#include "board.h"
#include "timebase.h"

/* DWT 寄存器定义（CoreDebug 在 core_cm4.h 里有，但 DWT 的 CYCCNTENA 位宏可能不全） */
#ifndef DWT_CTRL_CYCCNTENA_Pos
#define DWT_CTRL_CYCCNTENA_Pos   0U
#define DWT_CTRL_CYCCNTENA_Msk   (1UL << DWT_CTRL_CYCCNTENA_Pos)
#endif

#define CYCLES_PER_US  (BOARD_SYSCLK_HZ / 1000000u)   /* 168 */

void timebase_init(void)
{
    /* 使能 DWT 和 ITM（需要先开 TRCENA 位） */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0u;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

void timebase_delay_us(uint32_t us)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t cycles = us * CYCLES_PER_US;

    /* 处理溢出：CYCCNT 是 32 位无符号，差值算法天然正确 */
    while ((DWT->CYCCNT - start) < cycles) {
        __NOP();
    }
}

void timebase_delay_ms(uint32_t ms)
{
    /* 长延时直接用 HAL 的 SysTick，省 CPU */
    HAL_Delay(ms);
}

uint32_t timebase_us(void)
{
    return DWT->CYCCNT / CYCLES_PER_US;
}

uint32_t timebase_ms(void)
{
    return HAL_GetTick();
}
