#pragma once
/* 硬件常量与引脚映射 —— 全项目唯一事实来源（架构文档 §5）
 *
 * 使用限制：
 *   - 只有 driver/ 与 app/ 可以包含本文件；
 *   - algorithm/ 与 control/ 不得包含（架构 §14 第 4 条，保证可移植可测试）；
 *   - 本文件用到 HAL 的 GPIO_PIN_x 宏，必须在 stm32f4xx_hal.h 之后包含。
 *
 * 引脚依据：docs/IUO112_引脚配置清单.md、docs/硬件设计总结.md
 */

#include <stdint.h>

/* ===== 时钟 ===== */
#define BOARD_HSE_HZ            16000000u    /* Y1 16 MHz 晶振 */
#define BOARD_SYSCLK_HZ         168000000u
#define BOARD_APB1_HZ           42000000u    /* 上限 42 MHz */
#define BOARD_APB2_HZ           84000000u    /* 上限 84 MHz */

/* ===== 调试串口 J4 = UART5（PC12 / PD2） ===== */
#define BOARD_DEBUG_UART_BAUD   115200u
#define BOARD_DEBUG_UART_AF     GPIO_AF8_UART5
#define BOARD_DEBUG_UART_TX_PORT GPIOC
#define BOARD_DEBUG_UART_TX_PIN  GPIO_PIN_12
#define BOARD_DEBUG_UART_RX_PORT GPIOD
#define BOARD_DEBUG_UART_RX_PIN  GPIO_PIN_2

/* ===== 三路电机驱动 nSLEEP（低有效，外部 4.99K 下拉） ===== */
#define BOARD_SLEEP_M1_PORT     GPIOC
#define BOARD_SLEEP_M1_PIN      GPIO_PIN_10
#define BOARD_SLEEP_M2_PORT     GPIOA
#define BOARD_SLEEP_M2_PIN      GPIO_PIN_11
#define BOARD_SLEEP_M3_PORT     GPIOA
#define BOARD_SLEEP_M3_PIN      GPIO_PIN_12

/* ===== 三相 PWM =====
 * 三轴定时器与通道顺序各不相同（布线结果），必须表驱动，不可假设顺序一致。
 * M1 = TIM4 倒序 CH3/CH2/CH1，M2 = TIM1 正序 CH1/CH2/CH3，M3 = TIM3 从 CH4 起。
 */
#define BOARD_PWM_M1_TIM        TIM4
#define BOARD_PWM_M1_PORT       GPIOB
#define BOARD_PWM_M1_A_PIN      GPIO_PIN_8     /* TIM4_CH3 */
#define BOARD_PWM_M1_B_PIN      GPIO_PIN_7     /* TIM4_CH2 */
#define BOARD_PWM_M1_C_PIN      GPIO_PIN_6     /* TIM4_CH1 */
#define BOARD_PWM_M1_AF         GPIO_AF2_TIM4

#define BOARD_PWM_M2_TIM        TIM1
#define BOARD_PWM_M2_PORT       GPIOA
#define BOARD_PWM_M2_A_PIN      GPIO_PIN_8     /* TIM1_CH1 */
#define BOARD_PWM_M2_B_PIN      GPIO_PIN_9     /* TIM1_CH2 */
#define BOARD_PWM_M2_C_PIN      GPIO_PIN_10    /* TIM1_CH3 */
#define BOARD_PWM_M2_AF         GPIO_AF1_TIM1

#define BOARD_PWM_M3_TIM        TIM3
#define BOARD_PWM_M3_PORT       GPIOC
#define BOARD_PWM_M3_A_PIN      GPIO_PIN_9     /* TIM3_CH4 */
#define BOARD_PWM_M3_B_PIN      GPIO_PIN_8     /* TIM3_CH3 */
#define BOARD_PWM_M3_C_PIN      GPIO_PIN_7     /* TIM3_CH2 */
#define BOARD_PWM_M3_AF         GPIO_AF2_TIM3

#define BOARD_PWM_FREQ_HZ       24000u         /* TIM1 主，ARR=6999，PSC=0 */
#define BOARD_ADC_TRIG_HZ       8000u          /* RCR=2 时 TRGO = 24k/3 */

/* ===== 六路线性霍尔（MT9101ET），全部在 ADC1 ===== */
#define BOARD_HALL_M1A_ADC_CH   ADC_CHANNEL_11  /* PC1 */
#define BOARD_HALL_M1B_ADC_CH   ADC_CHANNEL_12  /* PC2 */
#define BOARD_HALL_M2A_ADC_CH   ADC_CHANNEL_0   /* PA0 */
#define BOARD_HALL_M2B_ADC_CH   ADC_CHANNEL_1   /* PA1 */
#define BOARD_HALL_M3A_ADC_CH   ADC_CHANNEL_14  /* PC4 */
#define BOARD_HALL_M3B_ADC_CH   ADC_CHANNEL_15  /* PC5 */
#define BOARD_VBUS_ADC_CH       ADC_CHANNEL_10  /* PC0，1:2 分压，>6.6V 饱和，仅做欠压判别 */
#define BOARD_ADC_CHANNEL_COUNT 7u              /* 扫描序列长度，vbus 在下标 6 */

/* ===== IMU 板 R14AQ：ICM-42688-P，SPI2 ===== */
#define BOARD_IMU_SPI           SPI2
#define BOARD_IMU_SCK_PORT      GPIOB
#define BOARD_IMU_SCK_PIN       GPIO_PIN_13    /* SPI2_SCK */
#define BOARD_IMU_MISO_PORT     GPIOB
#define BOARD_IMU_MISO_PIN      GPIO_PIN_14    /* SPI2_MISO */
#define BOARD_IMU_MOSI_PORT     GPIOB
#define BOARD_IMU_MOSI_PIN      GPIO_PIN_15    /* SPI2_MOSI */
#define BOARD_IMU_CS_PORT       GPIOB
#define BOARD_IMU_CS_PIN        GPIO_PIN_12    /* SPI2_NSS，软件片选 */
#define BOARD_IMU_SPI_AF        GPIO_AF5_SPI2

/* ===== IMU 加热（低边 MOS，栅极仅 1.65V，只做通断不做细调） ===== */
#define BOARD_HEATER_PORT       GPIOC
#define BOARD_HEATER_PIN        GPIO_PIN_6
