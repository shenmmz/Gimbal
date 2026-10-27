/* 控制层单元测试：pid
 *
 * 重点验证四个"写错了也能跑、但会在板子上表现出来"的地方：
 *   ① 积分饱和 —— 退饱和要多久
 *   ② 微分冲击 —— 目标阶跃时输出会不会瞬间打满
 *   ③ 复位后第一拍 —— prev=0 造成的假微分
 *   ④ 非有限输入 —— NaN 会不会永久污染积分器
 */

#include <math.h>
#include <stdint.h>

#include "pid.h"
#include "test_util.h"

#define DT  (1.0f / 8000.0f)   /* 8 kHz 控制环 */

static void test_pid_proportional(void)
{
    TEST_BEGIN("pid: 纯比例");

    pid_t p;
    pid_init(&p, 2.0f, 0.0f, 0.0f, 10.0f);

    CHECK_NEAR(pid_step(&p, 1.0f, DT), 2.0f, 1e-6f);
    CHECK_NEAR(pid_step(&p, -0.5f, DT), -1.0f, 1e-6f);
    CHECK_NEAR(pid_step(&p, 0.0f, DT), 0.0f, 1e-6f);

    /* 输出限幅 */
    pid_init(&p, 100.0f, 0.0f, 0.0f, 10.0f);
    CHECK_NEAR(pid_step(&p, 1.0f, DT), 10.0f, 0.0f);
    CHECK(pid_is_saturated(&p));
    CHECK_NEAR(pid_step(&p, -1.0f, DT), -10.0f, 0.0f);
    CHECK(pid_is_saturated(&p));

    /* out_max = 0 等于关掉输出，而不是"不限幅" */
    pid_init(&p, 100.0f, 0.0f, 0.0f, 0.0f);
    CHECK_NEAR(pid_step(&p, 1.0f, DT), 0.0f, 0.0f);
    CHECK(!pid_is_saturated(&p));
}

static void test_pid_integral(void)
{
    TEST_BEGIN("pid: 积分与抗饱和");

    pid_t p;

    /* 纯积分：i_term 应等于 ki · Σerr·dt */
    pid_init(&p, 0.0f, 100.0f, 0.0f, 10.0f);
    for (int i = 0; i < 100; i++) {
        pid_step(&p, 0.1f, DT);
    }
    CHECK_NEAR(p.i_term, 100.0f * 0.1f * DT * 100.0f, 1e-4f);
    CHECK_NEAR(pid_step(&p, 0.0f, DT), 0.125f, 1e-4f);

    /* 积分项限幅：i_max 夹住之后输出不再增长 */
    pid_init(&p, 0.0f, 1000.0f, 0.0f, 10.0f);
    pid_set_i_limit(&p, 0.5f);
    for (int i = 0; i < 2000; i++) {
        pid_step(&p, 1.0f, DT);
    }
    CHECK_NEAR(p.i_term, 0.5f, 1e-5f);
    CHECK_NEAR(pid_step(&p, 0.0f, DT), 0.5f, 1e-5f);

    /* 抗饱和的核心用例：P 已经顶到限幅，积分不能再涨。
     * 没有条件积分的话 i_term 会累到 i_max=out_max=1，
     * 误差反向之后要 1/(ki·|err|·dt) 拍才退得出来。 */
    pid_init(&p, 1.0f, 500.0f, 0.0f, 1.0f);
    for (int i = 0; i < 1000; i++) {
        pid_step(&p, 1.0f, DT);
    }
    CHECK_NEAR(p.i_term, 0.0f, 1e-6f);           /* 全程被条件积分冻住 */
    CHECK_NEAR(p.out, 1.0f, 0.0f);

    /* 误差反向的第一拍就必须立刻响应，不能还黏在正限幅上 */
    const float back = pid_step(&p, -0.5f, DT);
    CHECK(back < 0.0f);
    CHECK_NEAR(back, -0.5f + 500.0f * (-0.5f) * DT, 1e-4f);

    /* 反向推同样要被条件积分挡住 */
    pid_init(&p, 1.0f, 500.0f, 0.0f, 1.0f);
    for (int i = 0; i < 1000; i++) {
        pid_step(&p, -1.0f, DT);
    }
    CHECK_NEAR(p.i_term, 0.0f, 1e-6f);
    CHECK_NEAR(p.out, -1.0f, 0.0f);
}

static void test_pid_derivative(void)
{
    TEST_BEGIN("pid: 微分与冲击");

    pid_t p;

    /* 复位后第一拍只对齐 prev，不产生微分 */
    pid_init(&p, 0.0f, 0.0f, 1.0f, 100.0f);
    CHECK_NEAR(pid_step(&p, 5.0f, DT), 0.0f, 0.0f);   /* 不是 5000 */
    CHECK_NEAR(pid_step(&p, 5.0f, DT), 0.0f, 0.0f);   /* 稳态无微分 */

    /* 误差变化才产生微分：Δerr = 1 在 DT 上 → 8000，被限幅到 100 */
    CHECK_NEAR(pid_step(&p, 6.0f, DT), 100.0f, 0.0f);

    /* 微分低通：同样的误差阶跃，滤过之后微分项小得多。
     * out_max 要放得比未滤的尖峰更大，否则两者都被限幅到同一个值，看不出差别。 */
    pid_init(&p, 0.0f, 0.0f, 1.0f, 10000.0f);
    pid_set_d_lpf(&p, 200.0f, 8000.0f);
    pid_step(&p, 0.0f, DT);                            /* primed */
    const float d_lpf = pid_step(&p, 1.0f, DT);
    /* a = 1 − exp(−2π·200/8000) = 0.145434；阶跃斜率 8000 → 首拍 1163.5 */
    CHECK_NEAR(d_lpf, 8000.0f * 0.145434f, 5.0f);
    CHECK(d_lpf < 8000.0f);                            /* 未滤时是 8000 */

    /* pid_step_meas：目标阶跃但测量连续 → 无微分冲击。
     * 这是串级控制里推荐的做法，位置环下目标角跳变很常见。 */
    pid_init(&p, 0.0f, 0.0f, 1.0f, 100.0f);
    pid_step_meas(&p, 0.0f, 0.0f, DT);                 /* primed */
    CHECK_NEAR(pid_step_meas(&p, 5.0f, 0.0f, DT), 0.0f, 1e-6f);
    /* 测量值变化才出微分 */
    CHECK_NEAR(pid_step_meas(&p, 5.0f, -1.0f, DT), 100.0f, 0.0f);
}

static void test_pid_guards(void)
{
    TEST_BEGIN("pid: 非法输入与复位");

    pid_t p;

    /* dt <= 0：不改状态，返回上次输出 */
    pid_init(&p, 2.0f, 10.0f, 1.0f, 10.0f);
    const float first = pid_step(&p, 1.0f, DT);
    CHECK_NEAR(first, 2.0f + 10.0f * DT, 1e-6f);
    const float keep = pid_step(&p, 1.0f, 0.0f);
    CHECK_NEAR(keep, first, 0.0f);
    CHECK_NEAR(p.i_term, 10.0f * DT, 1e-7f);           /* 积分没动 */
    CHECK_NEAR(pid_step(&p, 1.0f, -DT), first, 0.0f);

    /* err = NaN：返回上次输出，积分器不能被污染。
     * volatile 挡住常量折叠，否则 MSVC 报 C2124（除以零）。 */
    volatile float zero_v = 0.0f;
    const float nan_v = zero_v / zero_v;
    const float held = pid_step(&p, nan_v, DT);
    CHECK_NEAR(held, first, 0.0f);
    CHECK(isfinite(p.i_term));

    /* 之后还能正常工作 —— 说明 NaN 没有留下永久损伤 */
    const float after = pid_step(&p, 0.5f, DT);
    CHECK(isfinite(after));

    /* 复位：清状态保参数 */
    pid_reset(&p);
    CHECK_NEAR(p.i_term, 0.0f, 0.0f);
    CHECK_NEAR(p.out, 0.0f, 0.0f);
    CHECK_NEAR(p.kp, 2.0f, 0.0f);
    /* 复位后第一拍重新对齐，不出微分尖峰：输出只该是 P + 这一拍的 I */
    CHECK_NEAR(pid_step(&p, 3.0f, DT), 6.0f + 10.0f * 3.0f * DT, 1e-6f);

    /* 参数校验：非有限增益整组拒绝，不做部分写入 */
    CHECK(!pid_set_gains(&p, nan_v, 1.0f, 1.0f));
    CHECK_NEAR(p.kp, 2.0f, 0.0f);
    CHECK(pid_set_gains(&p, 3.0f, 4.0f, 5.0f));
    CHECK_NEAR(p.kp, 3.0f, 0.0f);
    CHECK_NEAR(p.kd, 5.0f, 0.0f);

    /* 初始化时传 NaN 增益要落到 0，不能原样存下来（Flash 擦除态就是 0xFFFFFFFF） */
    pid_init(&p, nan_v, nan_v, nan_v, 10.0f);
    CHECK_NEAR(p.kp, 0.0f, 0.0f);
    CHECK_NEAR(p.ki, 0.0f, 0.0f);
    CHECK_NEAR(p.kd, 0.0f, 0.0f);
    CHECK_NEAR(pid_step(&p, 1.0f, DT), 0.0f, 0.0f);

    /* 负数 i_max 按绝对值处理，且立刻夹住已有积分 */
    pid_init(&p, 0.0f, 1000.0f, 0.0f, 10.0f);
    for (int i = 0; i < 100; i++) {
        pid_step(&p, 1.0f, DT);
    }
    CHECK(p.i_term > 0.0f);
    pid_set_i_limit(&p, -0.1f);
    CHECK_NEAR(p.i_term, 0.1f, 1e-6f);
}

static void test_pid_debug_readout(void)
{
    TEST_BEGIN("pid: 分项读取");

    pid_t p;
    /* out_max 取得足够大：这里要看的是三个分项各自的值，
     * 限幅一旦介入，条件积分就会冻结，i_term 反而读不到预期值 */
    pid_init(&p, 2.0f, 10.0f, 0.5f, 100000.0f);
    pid_set_d_lpf(&p, 0.0f, 0.0f);                     /* 直通，便于手算 */

    pid_step(&p, 0.0f, DT);                            /* primed */
    pid_step(&p, 1.0f, DT);

    float tp = 0.0f;
    float ti = 0.0f;
    float td = 0.0f;
    pid_get_terms(&p, &tp, &ti, &td);

    CHECK_NEAR(tp, 2.0f, 1e-6f);
    CHECK_NEAR(ti, 10.0f * DT, 1e-7f);
    CHECK_NEAR(td, 0.5f * (1.0f / DT), 1e-2f);

    /* 只读需要的分项时，其余指针传 NULL 不能崩 */
    pid_get_terms(&p, NULL, NULL, &td);
    CHECK(td > 0.0f);
}

int main(void)
{
    test_pid_proportional();
    test_pid_integral();
    test_pid_derivative();
    test_pid_guards();
    test_pid_debug_readout();
    TEST_END();
}
