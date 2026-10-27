#pragma once
/* 编译期开关与频率配置 —— 不依赖硬件，纯软件调节参数
 *
 * 需要改硬件相关的东西（引脚、定时器、通道号）去 board.h。
 * 需要改控制参数（PID 增益、滤波器截止频率）去对应模块的默认值。
 * 这里只放"全局编译开关"和"各层的工作频率"。
 */

#include <stdint.h>

/* ===== 控制频率（从 PWM 频率派生，不要直接改这里，改 board.h 的 BOARD_PWM_FREQ_HZ） ===== */
/* 8 kHz 控制环 = 24 kHz PWM / 3（RCR=2 时每个第 3 次更新事件触发 ADC） */
#define CTRL_FREQ_HZ        8000u
#define CTRL_PERIOD_S       (1.0f / (float)CTRL_FREQ_HZ)     /* 125 µs */
#define CTRL_PERIOD_US      (1000000u / CTRL_FREQ_HZ)        /* 125 µs */

/* ===== 保护检测频率（1 kHz，在 8 kHz 环里每 8 次跑一次） ===== */
#define PROTECT_FREQ_HZ     1000u

/* ===== 状态机步进频率（100 Hz，在主循环里跑） ===== */
#define FSM_FREQ_HZ         100u

/* ===== 功能开关（=1 启用，=0 关闭） ===== */

/* 反电动势前馈（W7 再开，先关着省算力） */
#define CONFIG_BEMF_FF      0

/* 摩擦前馈（W9 再开） */
#define CONFIG_FRICTION_FF  0

/* IMU 融合（W8 之后再开） */
#define CONFIG_IMU_FUSION   0

/* 调试用：把控制环里的关键变量通过串口打出来（会影响实时性，正式版关掉） */
#define CONFIG_CTRL_DEBUG   0
