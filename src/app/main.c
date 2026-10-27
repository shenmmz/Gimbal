#include "stm32f4xx_hal.h"

#include <stdio.h>

#include "uart.h"

void Error_Handler(void);

/* 168 MHz：HSE 16 MHz → PLLM 16（1 MHz）→ PLLN 336（336 MHz VCO）
 *          → PLLP 2（SYSCLK 168 MHz）、PLLQ 7（48 MHz）
 * APB1 /4 = 42 MHz（上限 42）、APB2 /2 = 84 MHz（上限 84）、Flash 5 等待周期
 */
static void clock_init(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState       = RCC_HSE_ON;
    osc.PLL.PLLState   = RCC_PLL_ON;
    osc.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLM       = 16u;
    osc.PLL.PLLN       = 336u;
    osc.PLL.PLLP       = RCC_PLLP_DIV2;
    osc.PLL.PLLQ       = 7u;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) {
        Error_Handler();
    }

    clk.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                         RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV4;
    clk.APB2CLKDivider = RCC_HCLK_DIV2;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_5) != HAL_OK) {
        Error_Handler();
    }
}

int main(void)
{
    HAL_Init();
    clock_init();
    uart_init();

    printf("\r\n=== Gimbal / IUO112 / STM32F405RGT6 ===\r\n");
    printf("SYSCLK %lu  HCLK %lu  PCLK1 %lu  PCLK2 %lu  (Hz)\r\n",
           (unsigned long)SystemCoreClock,
           (unsigned long)HAL_RCC_GetHCLKFreq(),
           (unsigned long)HAL_RCC_GetPCLK1Freq(),
           (unsigned long)HAL_RCC_GetPCLK2Freq());

    uint32_t last = 0u;
    for (;;) {
        uint32_t now = HAL_GetTick();
        if ((now - last) >= 1000u) {
            last = now;
            printf("alive %lu s\r\n", (unsigned long)(now / 1000u));
        }
    }
}

void Error_Handler(void)
{
    __disable_irq();
    for (;;) {
    }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    (void)file;
    (void)line;
    Error_Handler();
}
#endif
