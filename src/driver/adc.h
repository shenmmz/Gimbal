#pragma once
/* ADC 驱动：ADC1 扫描 7 通道（6 路霍尔 + 1 路母线电压），DMA 搬运
 *
 * 通道顺序（与 DMA 缓冲下标对应）：
 *   [0] M1A 霍尔  (CH11, PC1)
 *   [1] M1B 霍尔  (CH12, PC2)
 *   [2] M2A 霍尔  (CH0,  PA0)
 *   [3] M2B 霍尔  (CH1,  PA1)
 *   [4] M3A 霍尔  (CH14, PC4)
 *   [5] M3B 霍尔  (CH15, PC5)
 *   [6] VBUS 分压 (CH10, PC0)  —— 1:2 分压，超量程，仅做欠压判别
 *
 * W1 阶段：软件触发 + 连续扫描，主循环里直接读缓冲就行。
 * W2 阶段：改成 TIM1 TRGO 硬件触发，8 kHz 同步采样。
 */

#include <stdint.h>

#define ADC_CH_COUNT  BOARD_ADC_CHANNEL_COUNT   /* 7 */

void adc_init(void);

/* 读取最新的 7 路 ADC 原始值（12 位，0~4095） */
void adc_get_raw(uint16_t buf[ADC_CH_COUNT]);

/* 单路读取（轴 0~2，霍尔 A/B），方便调试 */
uint16_t adc_get_hall(uint8_t axis, uint8_t hall_ab);   /* hall_ab: 0=A, 1=B */
uint16_t adc_get_vbus(void);
