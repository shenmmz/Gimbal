#include "stm32f4xx_hal.h"
#include <stdio.h>
#include "board.h"
#include "uart.h"
#include "gpio.h"
#include "pwm.h"
#include "adc.h"
#include "timebase.h"
#include "types.h"

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

/* W1 自检：打印 7 路 ADC 原始值 + 每秒一行 */
static void self_test_print(void)
{
    static uint32_t s_last = 0u;
    uint32_t now = timebase_ms();

    if ((now - s_last) < 1000u) {
        return;
    }
    s_last = now;

    uint16_t adc[ADC_CH_COUNT];
    adc_get_raw(adc);

    printf("[%5lu s] ADC: M1A=%4u M1B=%4u M2A=%4u M2B=%4u M3A=%4u M3B=%4u VBUS=%4u\r\n",
           (unsigned long)(now / 1000u),
           adc[0], adc[1], adc[2], adc[3], adc[4], adc[5], adc[6]);
}

int main(void)
{
    HAL_Init();         /* HAL 库 + SysTick 1ms */
    clock_init();       /* 168 MHz 时钟树 */
    timebase_init();    /* DWT 微秒计数器 */
    uart_init();        /* 调试串口 UART5 */
    gpio_init();        /* nSLEEP + 加热 GPIO */
    pwm_init();         /* 三轴 24 kHz PWM */
    adc_init();         /* ADC1 扫描 + DMA */

    printf("\r\n=== Gimbal / IUO112 / STM32F405RGT6 ===\r\n");
    printf("SYSCLK %lu  HCLK %lu  PCLK1 %lu  PCLK2 %lu  (Hz)\r\n",
           (unsigned long)SystemCoreClock,
           (unsigned long)HAL_RCC_GetHCLKFreq(),
           (unsigned long)HAL_RCC_GetPCLK1Freq(),
           (unsigned long)HAL_RCC_GetPCLK2Freq());
    printf("W1 板级自检模式：PWM=50%%, ADC 循环采样打印\r\n");

    /* 上电顺序固定为 ① 先配 50% 占空比 → ② 再拉高 nSLEEP（架构 §10.6.1）。
     * 反过来做，桥臂会在 CCR 还是复位值 0 时通电，三相全低，可能直通。 */
    pwm_set_all_zero();
    gpio_set_all_sleep(false);   /* false = 不睡眠 = nSLEEP 拉高 */
    timebase_delay_us(1500u);    /* 等内部电荷泵建立 */

    printf("nSLEEP 已拉高，PWM=50%%，开始 ADC 采样...\r\n\r\n");

    for (;;) {
        self_test_print();
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
