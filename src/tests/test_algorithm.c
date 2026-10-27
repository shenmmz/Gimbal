/* 算法层单元测试：math_utils / filter / hall / svpwm
 *
 * 这一层不碰硬件，所以能在 PC 上完整验证。用例的重点不是"跑通"，
 * 而是把几处最容易写错、且在板子上表现为"能转但角度有偏差"的地方钉死：
 *   - 霍尔：120° 重构的符号与 dir 的作用位置
 *   - SVPWM：Clarke 往返一致（符号错会变成电机抖动而不是报错）
 */

#include <math.h>
#include <stdint.h>

#include "filter.h"
#include "hall.h"
#include "math_utils.h"
#include "svpwm.h"
#include "test_util.h"
#include "types.h"

/* ===================== math_utils ===================== */

static void test_math_utils(void)
{
    TEST_BEGIN("math_utils");

    /* wrap_pi 是左闭右开：π 要折成 −π，不能留在 π */
    CHECK_NEAR(wrap_pi(0.0f), 0.0f, 1e-7f);
    CHECK_NEAR(wrap_pi(MATH_PI), -MATH_PI, 1e-6f);
    CHECK_NEAR(wrap_pi(-MATH_PI), -MATH_PI, 1e-6f);
    CHECK_NEAR(wrap_pi(MATH_PI + 0.1f), -MATH_PI + 0.1f, 1e-6f);
    CHECK_NEAR(wrap_pi(MATH_TWO_PI * 3.0f + 0.5f), 0.5f, 1e-4f);
    CHECK_NEAR(wrap_pi(-MATH_TWO_PI * 3.0f - 0.5f), -0.5f, 1e-4f);

    /* 结果必须落在 [-π, π)，用一批散点扫一遍边界 */
    for (int i = -100; i <= 100; i++) {
        const float a = (float)i * 0.37f;
        const float w = wrap_pi(a);
        CHECK((w >= -MATH_PI) && (w < MATH_PI));
    }

    CHECK_NEAR(wrap_2pi(-0.1f), MATH_TWO_PI - 0.1f, 1e-5f);
    CHECK_NEAR(wrap_2pi(MATH_TWO_PI), 0.0f, 1e-5f);

    /* angle_diff 必须在 ±π 处给出 0.1 而不是 2π−0.1 —— 这就是求速度不能用
     * 直接相减的原因 */
    CHECK_NEAR(angle_diff(0.05f, -0.05f), 0.1f, 1e-6f);
    CHECK_NEAR(angle_diff(-MATH_PI + 0.05f, MATH_PI - 0.05f), 0.1f, 1e-5f);
    CHECK_NEAR(angle_diff(MATH_PI - 0.05f, -MATH_PI + 0.05f), -0.1f, 1e-5f);

    CHECK_NEAR(clampf(5.0f, -1.0f, 1.0f), 1.0f, 0.0f);
    CHECK_NEAR(clampf(-5.0f, -1.0f, 1.0f), -1.0f, 0.0f);
    CHECK_NEAR(clampf(0.3f, -1.0f, 1.0f), 0.3f, 0.0f);

    /* NaN 必须原样透传：被悄悄夹成合法值会让故障藏起来。
     * 不能用 0.0f/0.0f 字面量 —— MSVC 直接报 C2124（除以零），
     * 所以用 volatile 挡住常量折叠，运行时生成真正的 NaN。 */
    volatile float zero_v = 0.0f;
    const float nan_v = zero_v / zero_v;
    CHECK(isnan(clampf(nan_v, -1.0f, 1.0f)));
}

/* ===================== filter ===================== */

static void test_filter(void)
{
    TEST_BEGIN("filter");

    lpf_t f;

    /* fc<=0 或 fs<=0 按直通处理 */
    lpf_init(&f, 0.0f, 8000.0f);
    CHECK_NEAR(f.a, 1.0f, 0.0f);
    CHECK_NEAR(lpf_update(&f, 3.5f), 3.5f, 1e-6f);

    lpf_init(&f, 300.0f, 0.0f);
    CHECK_NEAR(f.a, 1.0f, 0.0f);

    /* fc >= fs/2 也按直通（否则 expf 参数过小，系数失去意义） */
    lpf_init(&f, 5000.0f, 8000.0f);
    CHECK_NEAR(f.a, 1.0f, 0.0f);

    /* 300 Hz @ 8 kHz：a = 1 − exp(−2π·300/8000) = 0.209919
     * （不是 1/(1+2πfc/fs) = 0.20944，两者差 5e-4，用近似式会带来可见的截止频率偏差） */
    lpf_init(&f, 300.0f, 8000.0f);
    CHECK_NEAR(f.a, 0.2099187f, 1e-6f);

    /* 阶跃响应：一阶低通第 n 拍的输出 = 1 − (1−a)^n */
    lpf_reset(&f, 0.0f);
    float y = 0.0f;
    for (int i = 0; i < 100; i++) {
        y = lpf_update(&f, 1.0f);
    }
    const float expect = 1.0f - powf(1.0f - f.a, 100.0f);
    CHECK_NEAR(y, expect, 1e-5f);

    /* 直流增益必须为 1：低通滤掉的是波动，不是把信号整体缩小 */
    lpf_reset(&f, 0.0f);
    for (int i = 0; i < 5000; i++) {
        y = lpf_update(&f, 2.0f);
    }
    CHECK_NEAR(y, 2.0f, 1e-4f);

    /* 相位滞后：截止频率处 45°，低于截止频率时更小 */
    CHECK_NEAR(lpf_phase_lag_deg(300.0f, 300.0f), 45.0f, 0.1f);
    CHECK(lpf_phase_lag_deg(100.0f, 300.0f) < 20.0f);
    CHECK(lpf_phase_lag_deg(1000.0f, 300.0f) > 70.0f);
}

/* ===================== hall ===================== */

#define HALL_OFF   2048.0f
#define HALL_GAIN  1800.0f
#define TWO_PI_3   2.09439510239319549231f   /* 120° */

/* 由机械角生成两路霍尔原始码：a = sinθ，b = sin(θ−120°) */
static void hall_synth(float theta, uint16_t raw[2])
{
    const float a = sinf(theta);
    const float b = sinf(theta - TWO_PI_3);
    raw[0] = (uint16_t)(HALL_OFF + HALL_GAIN * a + 0.5f);
    raw[1] = (uint16_t)(HALL_OFF + HALL_GAIN * b + 0.5f);
}

static void test_hall(void)
{
    TEST_BEGIN("hall");

    const uint8_t ax = AXIS_M1;
    const float off[2] = { HALL_OFF, HALL_OFF };
    const float gain[2] = { HALL_GAIN, HALL_GAIN };

    hall_init(ax, 1u);
    hall_set_lpf(ax, 0.0f, 0.0f);           /* 静态精度测试，先不滤 */
    hall_set_calib(ax, off, gain, 1);

    /* 整圈往返：误差只来自 ADC 量化，1 LSB ≈ 0.03° */
    uint16_t raw[2];
    float worst = 0.0f;
    for (int i = -180; i < 180; i++) {
        const float th = (float)i * MATH_DEG2RAD;
        hall_synth(th, raw);
        const bool ok = hall_decode(ax, raw);
        CHECK(ok);

        const float got = hall_get_mech(ax);
        float d = angle_diff(got, th);
        if (d < 0.0f) {
            d = -d;
        }
        if (d > worst) {
            worst = d;
        }
        CHECK_ANGLE(got, th, 0.005f);       /* 0.29°，量化上限约 0.03° */
    }
    CHECK(worst < 0.005f);

    /* 幅值应为 1（两路增益一致、正交重构正确） */
    hall_synth(0.7f, raw);
    hall_decode(ax, raw);
    CHECK_NEAR(hall_get_mag(ax), 1.0f, 0.01f);

    /* 电角度 = wrap(p · θ_mech)。极对数 7 时，机械角转 2π/7 电角度才转一圈 */
    hall_init(ax, 7u);
    hall_set_lpf(ax, 0.0f, 0.0f);
    hall_set_calib(ax, off, gain, 1);
    hall_synth(0.3f, raw);
    hall_decode(ax, raw);
    CHECK_ANGLE(hall_get_elec(ax), wrap_pi(7.0f * 0.3f), 0.01f);

    /* dir = −1 时角度方向反转：θ 增大，读数减小。
     * 这条能抓住"dir 乘到了 sin 项而不是 cos 项"这种错误。 */
    hall_init(ax, 1u);
    hall_set_lpf(ax, 0.0f, 0.0f);
    hall_set_calib(ax, off, gain, -1);
    for (int i = -170; i < 170; i += 10) {
        const float th = (float)i * MATH_DEG2RAD;
        hall_synth(th, raw);
        hall_decode(ax, raw);
        CHECK_ANGLE(hall_get_mech(ax), wrap_pi(MATH_PI - th), 0.01f);
    }

    /* 单通道贴轨：幅值只有 1.31，落在 HALL_MAG_MIN..MAX 之内，
     * 必须靠独立的贴轨判据抓住 —— 这条用例就是那条判据的存在理由。
     * 角度仍然要更新（上层靠连续计数判故障，不是靠单次 false）。 */
    hall_init(ax, 1u);
    hall_set_lpf(ax, 0.0f, 0.0f);
    hall_set_calib(ax, off, gain, 1);
    raw[0] = 4095u;
    raw[1] = 2048u;
    CHECK(!hall_decode(ax, raw));
    CHECK(hall_get_mag(ax) > HALL_MAG_MIN);
    CHECK(hall_get_mag(ax) < HALL_MAG_MAX);      /* 幅值判据在这里失效 */
    CHECK(isfinite(hall_get_mech(ax)));

    /* 两路同时贴轨：幅值判据也能抓到（2.27 > 1.5） */
    raw[0] = 4095u;
    raw[1] = 4095u;
    CHECK(!hall_decode(ax, raw));
    CHECK(hall_get_mag(ax) > HALL_MAG_MAX);

    /* 正常范围内不该误报 */
    hall_synth(0.4f, raw);
    CHECK(hall_decode(ax, raw));

    /* 增益为 0 必须被拒绝，否则归一化里会除零 */
    hall_init(ax, 1u);
    const float bad_gain[2] = { 0.0f, HALL_GAIN };
    hall_set_calib(ax, off, bad_gain, 1);
    hall_synth(0.0f, raw);
    hall_decode(ax, raw);
    CHECK(isfinite(hall_get_mech(ax)));

    /* 滤波打开后，静止输入应收敛到同一角度（滤 sin/cos 不改变稳态） */
    hall_init(ax, 1u);
    hall_set_lpf(ax, HALL_DEFAULT_LPF_HZ, 8000.0f);
    hall_set_calib(ax, off, gain, 1);
    hall_synth(1.1f, raw);
    for (int i = 0; i < 2000; i++) {
        hall_decode(ax, raw);
    }
    CHECK_ANGLE(hall_get_mech(ax), 1.1f, 0.005f);
}

/* ===================== svpwm ===================== */

static void test_svpwm(void)
{
    TEST_BEGIN("svpwm");

    const float vbus = 7.2f;
    float duty[3];

    /* 零矢量：三相都是 50% */
    CHECK(svpwm_calc(0.0f, 0.0f, vbus, duty) == SVPWM_OK);
    CHECK_NEAR(duty[0], 0.5f, 1e-6f);
    CHECK_NEAR(duty[1], 0.5f, 1e-6f);
    CHECK_NEAR(duty[2], 0.5f, 1e-6f);

    /* 整圈扫一遍，验证两个不变量：
     *   ① 逆 Clarke 回去必须还原输入的 u_alpha/u_beta（零序注入在 Clarke 里自然消掉）
     *   ② max(duty) + min(duty) = 1（min-max 注入的定义式）
     * 幅值取 0.99 倍线性上限：满幅时占空比正好顶到 0/1，浮点舍入会让
     * 边界在 OK / SATURATED 之间抖，留 1% 余量让用例稳定。 */
    const float m_max = SVPWM_LINEAR_MAX_PU * vbus;
    const float m_sweep = 0.99f * m_max;
    float worst_ua = 0.0f;
    float worst_ub = 0.0f;
    for (int i = 0; i < 360; i++) {
        const float th = (float)i * MATH_DEG2RAD;
        const float ua = m_sweep * cosf(th);
        const float ub = m_sweep * sinf(th);

        CHECK(svpwm_calc(ua, ub, vbus, duty) == SVPWM_OK);

        /* duty → 线-中性点电压 → Clarke */
        const float va = (duty[0] - 0.5f) * vbus;
        const float vb = (duty[1] - 0.5f) * vbus;
        const float vc = (duty[2] - 0.5f) * vbus;
        const float ua2 = (2.0f / 3.0f) * (va - 0.5f * vb - 0.5f * vc);
        const float ub2 = (2.0f / 3.0f) * MATH_SQRT3_2 * (vb - vc);

        CHECK_NEAR(ua2, ua, 1e-4f);
        CHECK_NEAR(ub2, ub, 1e-4f);
        if (fabsf(ua2 - ua) > worst_ua) {
            worst_ua = fabsf(ua2 - ua);
        }
        if (fabsf(ub2 - ub) > worst_ub) {
            worst_ub = fabsf(ub2 - ub);
        }

        float mn = duty[0];
        float mx = duty[0];
        for (int k = 1; k < 3; k++) {
            if (duty[k] < mn) {
                mn = duty[k];
            }
            if (duty[k] > mx) {
                mx = duty[k];
            }
        }
        CHECK_NEAR(mn + mx, 1.0f, 1e-5f);

        /* 0.99 倍线性上限内，占空比不会碰到边界 */
        CHECK((mn >= 0.004f) && (mx <= 0.996f));
    }
    CHECK(worst_ua < 1e-4f);
    CHECK(worst_ub < 1e-4f);

    /* 单相满幅（矢量对准 A 相轴）：da = 0.933、db = dc = 0.067。
     * 这里峰值是 √3/4 = 0.433，不是 0.5 —— 相轴处最"省"占空比。 */
    CHECK(svpwm_calc(SVPWM_LINEAR_MAX_PU * vbus, 0.0f, vbus, duty) == SVPWM_OK);
    CHECK_NEAR(duty[0], 0.933013f, 1e-4f);
    CHECK_NEAR(duty[1], 0.066987f, 1e-4f);
    CHECK_NEAR(duty[2], 0.066987f, 1e-4f);

    /* 相间 30° 处满幅：占空比正好顶到 1 和 0。
     * 这一条钉住"满线性调制会用满 [0,1]，没有余量"这个结论 ——
     * 写代码时如果以为留了余量，就会在满调制时被 pwm.c 的 0.02~0.98 悄悄削掉。 */
    {
        const float th30 = 30.0f * MATH_DEG2RAD;
        CHECK(svpwm_calc(m_max * cosf(th30), m_max * sinf(th30), vbus, duty) == SVPWM_OK);
        CHECK_NEAR(duty[0], 1.0f, 1e-4f);
        CHECK_NEAR(duty[1], 0.5f, 1e-4f);
        CHECK_NEAR(duty[2], 0.0f, 1e-4f);
    }

    /* 过调制：|u| = 0.7·vbus > 2/3·vbus，占空比要被夹住并报饱和 */
    CHECK(svpwm_calc(0.7f * vbus, 0.0f, vbus, duty) == SVPWM_SATURATED);
    CHECK_NEAR(duty[0], 1.0f, 0.0f);
    CHECK_NEAR(duty[1], 0.0f, 0.0f);

    /* 归一化入口：1.0 对应 vbus/√3，与伏特入口必须逐点等价 */
    float d1[3];
    float d2[3];
    for (int i = 0; i < 36; i++) {
        const float th = (float)i * 10.0f * MATH_DEG2RAD;
        const float na = 0.9f * cosf(th);
        const float nb = 0.9f * sinf(th);
        CHECK(svpwm_calc_norm(na, nb, d1) ==
              svpwm_calc(na * m_max, nb * m_max, vbus, d2));
        CHECK_NEAR(d1[0], d2[0], 1e-6f);
        CHECK_NEAR(d1[1], d2[1], 1e-6f);
        CHECK_NEAR(d1[2], d2[2], 1e-6f);
    }

    /* 非法输入：vbus = 0 / NaN / inf 都必须输出零矢量，不能把 NaN 写进 CCR */
    CHECK(svpwm_calc(1.0f, 0.0f, 0.0f, duty) == SVPWM_INVALID);
    CHECK_NEAR(duty[0], 0.5f, 0.0f);
    CHECK_NEAR(duty[2], 0.5f, 0.0f);

    CHECK(svpwm_calc(1.0f, 0.0f, -7.2f, duty) == SVPWM_INVALID);
    CHECK_NEAR(duty[0], 0.5f, 0.0f);

    /* NaN / inf 用 volatile 现算，理由同 math_utils 那一段 */
    volatile float zero_v = 0.0f;
    const float nan_v = zero_v / zero_v;
    const float inf_v = 1.0f / zero_v;

    CHECK(svpwm_calc(nan_v, 0.0f, vbus, duty) == SVPWM_INVALID);
    CHECK_NEAR(duty[0], 0.5f, 0.0f);

    CHECK(svpwm_calc(inf_v, 0.0f, vbus, duty) == SVPWM_INVALID);
    CHECK_NEAR(duty[1], 0.5f, 0.0f);

    CHECK(svpwm_calc_norm(nan_v, 0.0f, duty) == SVPWM_INVALID);
    CHECK_NEAR(duty[2], 0.5f, 0.0f);
}

int main(void)
{
    test_math_utils();
    test_filter();
    test_hall();
    test_svpwm();
    TEST_END();
}
