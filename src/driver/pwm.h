#pragma once
/* PWM 驱动：三轴 24 kHz 中心对齐 PWM
 *
 * 硬件事实（必须表驱动，不能假设顺序一致）：
 *   M1 = TIM4  倒序：CH3 / CH2 / CH1   → GPIOB 引脚 8/7/6
 *   M2 = TIM1  正序：CH1 / CH2 / CH3   → GPIOA 引脚 8/9/10
 *   M3 = TIM3  从 CH4：CH4 / CH3 / CH2 → GPIOC 引脚 9/8/7
 *
 * W1 阶段：三路独立输出，固定占空比能看到波形就行。
 * W2 阶段：TIM1 主、TIM3/TIM4 从模式同步 + TRGO 触发 ADC。
 *
 * 占空比范围：0.0f ~ 1.0f，内部自动限幅到 0.02~0.98（避免直通和死区饱和）。
 */

#include <stdint.h>

void pwm_init(void);

/* 设置某轴三相占空比，duty[3] = {A相, B相, C相}，范围 0~1 */
void pwm_set_duty(uint8_t axis, const float duty[3]);

/* 全部设为 0.5（零电压矢量，电机自由但有弱保持） */
void pwm_set_zero(uint8_t axis);
void pwm_set_all_zero(void);
