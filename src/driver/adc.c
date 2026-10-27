#include "stm32f4xx_hal.h"

#include <stddef.h>

#include "board.h"
#include "adc.h"
#include "types.h"
#include "error.h"

/* DMA 缓冲：7 通道，双缓冲其实 W1 不需要，就单缓冲循环模式 */
static uint16_t s_adc_buf[ADC_CH_COUNT];

static ADC_HandleTypeDef s_hadc;
static DMA_HandleTypeDef s_hdma_adc;

/* ADC 通道配置表（顺序 = 扫描序列顺序，也 = DMA 缓冲下标） */
typedef struct {
    uint32_t channel;
    GPIO_TypeDef *port;
    uint16_t pin;
} adc_ch_cfg_t;

static const adc_ch_cfg_t s_adc_ch[ADC_CH_COUNT] = {
    /* [0] M1A = CH11 PC1 */
    { BOARD_HALL_M1A_ADC_CH, GPIOC, GPIO_PIN_1 },
    /* [1] M1B = CH12 PC2 */
    { BOARD_HALL_M1B_ADC_CH, GPIOC, GPIO_PIN_2 },
    /* [2] M2A = CH0 PA0 */
    { BOARD_HALL_M2A_ADC_CH, GPIOA, GPIO_PIN_0 },
    /* [3] M2B = CH1 PA1 */
    { BOARD_HALL_M2B_ADC_CH, GPIOA, GPIO_PIN_1 },
    /* [4] M3A = CH14 PC4 */
    { BOARD_HALL_M3A_ADC_CH, GPIOC, GPIO_PIN_4 },
    /* [5] M3B = CH15 PC5 */
    { BOARD_HALL_M3B_ADC_CH, GPIOC, GPIO_PIN_5 },
    /* [6] VBUS = CH10 PC0 */
    { BOARD_VBUS_ADC_CH, GPIOC, GPIO_PIN_0 },
};

/* HAL 在 HAL_ADC_MspInit 里回调 */
void HAL_ADC_MspInit(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance != ADC1) {
        return;
    }

    __HAL_RCC_ADC1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_DMA2_CLK_ENABLE();

    /* 配置 7 个 GPIO 为模拟输入 */
    GPIO_InitTypeDef gpio = {0};
    gpio.Mode = GPIO_MODE_ANALOG;
    gpio.Pull = GPIO_NOPULL;

    for (uint8_t i = 0; i < ADC_CH_COUNT; i++) {
        gpio.Pin = s_adc_ch[i].pin;
        HAL_GPIO_Init(s_adc_ch[i].port, &gpio);
    }

    /* DMA2 Stream0 Channel0 → ADC1 */
    s_hdma_adc.Instance                 = DMA2_Stream0;
    s_hdma_adc.Init.Channel             = DMA_CHANNEL_0;
    s_hdma_adc.Init.Direction           = DMA_PERIPH_TO_MEMORY;
    s_hdma_adc.Init.PeriphInc           = DMA_PINC_DISABLE;
    s_hdma_adc.Init.MemInc              = DMA_MINC_ENABLE;
    s_hdma_adc.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    s_hdma_adc.Init.MemDataAlignment    = DMA_MDATAALIGN_HALFWORD;
    s_hdma_adc.Init.Mode                = DMA_CIRCULAR;   /* 循环模式，一直搬 */
    s_hdma_adc.Init.Priority            = DMA_PRIORITY_HIGH;
    s_hdma_adc.Init.FIFOMode            = DMA_FIFOMODE_DISABLE;

    if (HAL_DMA_Init(&s_hdma_adc) != HAL_OK) {
        Error_Handler();
    }

    __HAL_LINKDMA(hadc, DMA_Handle, s_hdma_adc);
}

void adc_init(void)
{
    ADC_ChannelConfTypeDef ch_cfg = {0};

    s_hadc.Instance                   = ADC1;
    s_hadc.Init.ClockPrescaler        = ADC_CLOCK_SYNC_PCLK_DIV4;  /* APB2/4 = 84/4 = 21 MHz */
    s_hadc.Init.Resolution            = ADC_RESOLUTION_12B;
    s_hadc.Init.ScanConvMode          = ENABLE;                    /* 扫描模式 */
    s_hadc.Init.ContinuousConvMode    = ENABLE;                    /* W1 用连续转换，W2 改触发 */
    s_hadc.Init.DiscontinuousConvMode = DISABLE;
    s_hadc.Init.NbrOfConversion       = ADC_CH_COUNT;
    s_hadc.Init.ExternalTrigConv      = ADC_SOFTWARE_START;        /* W1 软件触发 */
    s_hadc.Init.ExternalTrigConvEdge  = ADC_EXTERNALTRIGCONVEDGE_NONE;
    s_hadc.Init.DataAlign             = ADC_DATAALIGN_RIGHT;
    s_hadc.Init.DMAContinuousRequests = ENABLE;                    /* DMA 循环请求 */
    s_hadc.Init.EOCSelection          = ADC_EOC_SEQ_CONV;          /* 序列结束才置 EOC */

    if (HAL_ADC_Init(&s_hadc) != HAL_OK) {
        Error_Handler();
    }

    /* 配置各通道的采样时间和序列秩 */
    for (uint8_t i = 0; i < ADC_CH_COUNT; i++) {
        ch_cfg.Channel      = s_adc_ch[i].channel;
        ch_cfg.Rank         = i + 1;   /* 秩从 1 开始 */
        /* 霍尔通道用较短采样时间，母线通道用长一点（输入阻抗高） */
        if (i == 6u) {
            ch_cfg.SamplingTime = ADC_SAMPLETIME_84CYCLES;  /* VBUS，约 4 µs */
        } else {
            ch_cfg.SamplingTime = ADC_SAMPLETIME_15CYCLES;   /* 霍尔，约 0.7 µs */
        }
        ch_cfg.Offset = 0;

        if (HAL_ADC_ConfigChannel(&s_hadc, &ch_cfg) != HAL_OK) {
            Error_Handler();
        }
    }

    /* 启动 ADC + DMA */
    if (HAL_ADC_Start_DMA(&s_hadc, (uint32_t *)s_adc_buf, ADC_CH_COUNT) != HAL_OK) {
        Error_Handler();
    }
}

void adc_get_raw(uint16_t buf[ADC_CH_COUNT])
{
    if (buf == NULL) {
        return;
    }
    /* 直接拷贝，7 个 uint16_t 很快，不担心 torn read（W1 阶段无所谓） */
    for (uint8_t i = 0; i < ADC_CH_COUNT; i++) {
        buf[i] = s_adc_buf[i];
    }
}

uint16_t adc_get_hall(uint8_t axis, uint8_t hall_ab)
{
    if ((axis >= AXIS_COUNT) || (hall_ab > 1u)) {
        return 0u;
    }
    return s_adc_buf[axis * 2u + hall_ab];
}

uint16_t adc_get_vbus(void)
{
    return s_adc_buf[6];
}
