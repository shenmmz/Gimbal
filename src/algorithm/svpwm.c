#include "svpwm.h"

#include <float.h>
#include <math.h>
#include <stdint.h>

#include "math_utils.h"

/* 有限性判据：一条比较同时挡住 NaN 和 ±inf。
 *   NaN  <= FLT_MAX  → false（NaN 与任何数比较都为假）
 *   inf  <= FLT_MAX  → false
 * 比 (x == x) && fabsf(x) != INFINITY 少一次比较，也不用引 <math.h> 的 INFINITY
 * 宏（glibc 下它是 float，但别的 C 库不保证，-Wdouble-promotion 会抓出来）。 */
static bool finite_f(float x)
{
    return fabsf(x) <= FLT_MAX;
}

/* 真正的调制计算。输入是标幺值（已除过 vbus），单位无所谓，只要三相一致 */
static svpwm_status_t svpwm_core(float pa, float pb, float pc, float duty[3])
{
    /* 零序注入量。取 (max+min)/2 的负值，等价于让有效矢量在半个 PWM 周期内对称分布，
     * 也就是 SVPWM 的那个"鞍形"调制波。 */
    float vmax = pa;
    float vmin = pa;
    if (pb > vmax) vmax = pb;
    if (pb < vmin) vmin = pb;
    if (pc > vmax) vmax = pc;
    if (pc < vmin) vmin = pc;

    const float v0 = -0.5f * (vmax + vmin);

    /* 三相一起加同一个零序，线电压不变 —— 这正是零序注入合法的前提 */
    const float da = 0.5f + pa + v0;
    const float db = 0.5f + pb + v0;
    const float dc = 0.5f + pc + v0;

    float o[3] = { da, db, dc };
    bool saturated = false;

    for (uint8_t i = 0u; i < 3u; i++) {
        float d = o[i];
        if (d < 0.0f) {
            d = 0.0f;
            saturated = true;
        } else if (d > 1.0f) {
            d = 1.0f;
            saturated = true;
        }
        duty[i] = d;
    }

    return saturated ? SVPWM_SATURATED : SVPWM_OK;
}

void svpwm_zero(float duty[3])
{
    duty[0] = 0.5f;
    duty[1] = 0.5f;
    duty[2] = 0.5f;
}

svpwm_status_t svpwm_calc(float u_alpha, float u_beta, float vbus, float duty[3])
{
    /* vbus <= 0 会让下面的除法产生 ±inf，再进 core 就是 NaN 占空比。
     * NaN > 0 为假，所以这一条也顺带挡住了 NaN 的 vbus。 */
    if (!finite_f(u_alpha) || !finite_f(u_beta) || !(vbus > 0.0f) || !finite_f(vbus)) {
        svpwm_zero(duty);
        return SVPWM_INVALID;
    }

    /* 母线电压归一化就这一步，控制层不再做除法（架构 §7.2 的分工） */
    const float inv_vbus = 1.0f / vbus;
    const float ua = u_alpha * inv_vbus;
    const float ub = u_beta * inv_vbus;

    /* 反 Clarke（幅值不变）：ua = (2/3)(va − vb/2 − vc/2)，ub = (2/3)(√3/2)(vb − vc)
     * 逆过来即 va = ua，vb = −ua/2 + (√3/2)ub，vc = −ua/2 − (√3/2)ub */
    const float pa = ua;
    const float pb = -0.5f * ua + MATH_SQRT3_2 * ub;
    const float pc = -0.5f * ua - MATH_SQRT3_2 * ub;

    return svpwm_core(pa, pb, pc, duty);
}

svpwm_status_t svpwm_calc_norm(float u_alpha, float u_beta, float duty[3])
{
    if (!finite_f(u_alpha) || !finite_f(u_beta)) {
        svpwm_zero(duty);
        return SVPWM_INVALID;
    }

    /* −1~+1 ↔ 标幺：乘 1/√3 就等于先乘 vbus/√3 再除 vbus，
     * 那一步 vbus 自己约掉了，所以这个入口不需要 vbus 参数。 */
    const float ua = u_alpha * SVPWM_LINEAR_MAX_PU;
    const float ub = u_beta * SVPWM_LINEAR_MAX_PU;

    const float pa = ua;
    const float pb = -0.5f * ua + MATH_SQRT3_2 * ub;
    const float pc = -0.5f * ua - MATH_SQRT3_2 * ub;

    return svpwm_core(pa, pb, pc, duty);
}
