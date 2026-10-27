#pragma once
/* GPIO 驱动：nSLEEP、加热控制
 *
 * 注意：PWM 引脚和 UART 引脚不在这初始化，由各自驱动的 MspInit 负责。
 *       本文件只管"纯 GPIO"功能——推挽输出、上下拉输入。
 */

#include <stdint.h>
#include <stdbool.h>

void gpio_init(void);

/* nSLEEP：低有效，默认上电下拉=睡眠 */
void gpio_set_sleep(uint8_t axis, bool sleep);
void gpio_set_all_sleep(bool sleep);

/* 加热：1=开，0=关（低边 MOS，栅极只有 1.65V，只做通断） */
void gpio_set_heater(bool on);
