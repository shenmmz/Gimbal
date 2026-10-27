#include "math_utils.h"

#include <math.h>

float wrap_pi(float a)
{
    /* fmodf 在 a 很大时仍精确到 float 有效位；先加 π 再减 π 是为了得到
     * 左闭右开区间 [-π, π)，这样 atan2 的输出可以直接喂进来不会多一次跳变。 */
    a = fmodf(a + MATH_PI, MATH_TWO_PI);
    if (a < 0.0f) {
        a += MATH_TWO_PI;
    }
    return a - MATH_PI;
}

float wrap_2pi(float a)
{
    a = fmodf(a, MATH_TWO_PI);
    if (a < 0.0f) {
        a += MATH_TWO_PI;
    }
    return a;
}

float angle_diff(float a, float b)
{
    return wrap_pi(a - b);
}

float clampf(float x, float lo, float hi)
{
    /* 用比较而不是 fminf/fmaxf：NaN 时两者都返回另一操作数，
     * 会把 NaN 悄悄变成边界值。这里保持 NaN 穿透。 */
    if (x < lo) {
        return lo;
    }
    if (x > hi) {
        return hi;
    }
    return x;
}
