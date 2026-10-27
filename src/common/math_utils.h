#pragma once
/* 纯数学工具：角度归一化、最短角差、限幅
 *
 * 这一层不碰任何硬件，PC 上可直接跑。
 * 不包含 <math.h> 之外的头文件，也不引用 board.h / HAL。
 */

#include <stdbool.h>

/* 圆周率用 float 字面量，避免 double 提升（-Wdouble-promotion 会抓） */
#define MATH_PI       3.14159265358979323846f
#define MATH_TWO_PI   6.28318530717958647692f
#define MATH_HALF_PI  1.57079632679489661923f

#define MATH_DEG2RAD  0.017453292519943295f
#define MATH_RAD2DEG  57.295779513082320877f

#define MATH_SQRT3    1.73205080756887729353f   /* √3   */
#define MATH_SQRT3_2  0.86602540378443864676f   /* √3/2 */
#define MATH_2_SQRT3  1.15470053837925152902f   /* 2/√3 —— 霍尔 120° 解算用 */

/* 把角度折到 [-π, π) */
float wrap_pi(float a);

/* 把角度折到 [0, 2π) */
float wrap_2pi(float a);

/* 最短有向角差 a-b，结果落在 [-π, π)。
 * 求速度时必须用它，直接相减会在 ±π 处得到 2π 的假速度尖峰。 */
float angle_diff(float a, float b);

/* 限幅。NaN 输入会原样返回 NaN —— 让上层能通过 isnan() 发现异常，
 * 而不是被悄悄夹成一个看似合法的值。 */
float clampf(float x, float lo, float hi);
