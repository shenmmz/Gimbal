#pragma once
/* 时间基准：微秒级延时与时间戳
 *
 * 用 DWT 周期计数器实现，不占用定时器资源。
 * 精度 = 1 / SYSCLK = 1 / 168 MHz ≈ 5.95 ns
 *
 * 注意：
 *   - DWT 在 debug 接口被禁用时可能不可用（生产版通常没问题）
 *   - 延时函数是忙等，不要在中断里长时间调用
 *   - 时间戳是 32 位，约 25 秒溢出一次，调用方要自己处理回绕
 */

#include <stdint.h>

void timebase_init(void);

/* 忙等延时 */
void timebase_delay_us(uint32_t us);
void timebase_delay_ms(uint32_t ms);

/* 微秒时间戳（相对于启动时刻，32 位，约 25s 溢出） */
uint32_t timebase_us(void);

/* 毫秒时间戳（封装 HAL_GetTick，避免各处直接依赖 HAL） */
uint32_t timebase_ms(void);
