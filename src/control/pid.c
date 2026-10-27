#include "pid.h"

#include <float.h>
#include <math.h>

#include "math_utils.h"

/* 同 svpwm.c：一条比较同时挡住 NaN 与 ±inf */
static bool finite_f(float x)
{
    return fabsf(x) <= FLT_MAX;
}

void pid_init(pid_t *p, float kp, float ki, float kd, float out_max)
{
    if (!p) {
        return;
    }

    /* 非有限增益一律按 0 处理，而不是原样存下来。
     * 参数来自 Flash 时可能是擦除态 0xFFFFFFFF，解成 float 就是 NaN。 */
    p->kp = finite_f(kp) ? kp : 0.0f;
    p->ki = finite_f(ki) ? ki : 0.0f;
    p->kd = finite_f(kd) ? kd : 0.0f;

    p->out_max = (finite_f(out_max) && (out_max > 0.0f)) ? out_max : 0.0f;
    p->i_max = p->out_max;

    lpf_init(&p->d_lpf, 0.0f, 0.0f);   /* fc<=0 → 直通 */

    pid_reset(p);
}

void pid_reset(pid_t *p)
{
    if (!p) {
        return;
    }
    p->i_term = 0.0f;
    p->prev_dsrc = 0.0f;
    p->p_term = 0.0f;
    p->d_term = 0.0f;
    p->out = 0.0f;
    p->primed = false;   /* 下一拍只对齐 prev，不算微分 */
    lpf_reset(&p->d_lpf, 0.0f);
}

bool pid_set_gains(pid_t *p, float kp, float ki, float kd)
{
    if (!p) {
        return false;
    }
    /* 三项要么全写要么全不写。只写一半会让参数区里出现"新 kp + 旧 ki"的中间态，
     * 上位机连续下发时这种中间态会真的跑进控制环。 */
    if (!finite_f(kp) || !finite_f(ki) || !finite_f(kd)) {
        return false;
    }
    p->kp = kp;
    p->ki = ki;
    p->kd = kd;
    return true;
}

void pid_set_i_limit(pid_t *p, float i_max)
{
    if (!p || !finite_f(i_max)) {
        return;
    }
    if (i_max < 0.0f) {
        i_max = -i_max;
    }
    p->i_max = i_max;
    /* 立刻夹一次：调小限幅时不能等下一拍才生效，那一拍可能已经把输出推出去 */
    p->i_term = clampf(p->i_term, -p->i_max, p->i_max);
}

void pid_set_d_lpf(pid_t *p, float fc_hz, float fs_hz)
{
    if (!p) {
        return;
    }
    lpf_init(&p->d_lpf, fc_hz, fs_hz);
    lpf_reset(&p->d_lpf, 0.0f);
}

/* 三种 pid_step 变体共用的核心。
 * dsrc 是"要对谁求导"的那个量：D 作用在误差上就传 err，微分先行就传 −meas。
 * 同一个 pid_t 上不要混用两种变体 —— prev_dsrc 的物理含义会跳。 */
static float pid_core(pid_t *p, float err, float dsrc, float dt)
{
    /* dt 非法：不改状态，输出停在上一个合法值。
     * 宁可多保持 125 µs，也不要把 1/0 = inf 灌进积分器 —— 那个 inf 会一直留在
     * i_term 里，除非有人显式 pid_reset()。 */
    if (!(dt > 0.0f) || !finite_f(dt)) {
        return p->out;
    }
    if (!finite_f(err)) {
        return p->out;
    }

    const float inv_dt = 1.0f / dt;

    /* ---------- P ---------- */
    const float p_term = p->kp * err;

    /* ---------- D ---------- */
    float d_raw = 0.0f;
    if (!p->primed) {
        /* 复位后第一拍：prev 还是 0 而信号可能不是 0，直接差分等于凭空造一个
         * 巨大微分（阶跃 / dt = 8000 倍的尖峰）。这里只对齐，不产生微分。 */
        p->prev_dsrc = dsrc;
        p->primed = true;
        lpf_reset(&p->d_lpf, 0.0f);
    } else if (finite_f(dsrc)) {
        d_raw = (dsrc - p->prev_dsrc) * inv_dt;
        p->prev_dsrc = dsrc;
    } else {
        /* dsrc 坏了：本拍微分按 0 算，prev 保持不动，等它恢复 */
    }

    const float d_term = lpf_update(&p->d_lpf, p->kd * d_raw);

    /* ---------- I（条件积分抗饱和） ---------- */
    /* 先算"如果把这一拍的积分也算进去，输出会往哪边跑"。
     * 已经顶在限幅上、且误差还在往同一方向推，就不累加。
     * 只夹积分项不够：夹住之后仍会持续累加，等误差反向时要很久才退饱和。 */
    const float out_pre = p_term + p->i_term + d_term;
    const bool push_hi = (out_pre >= p->out_max) && (err > 0.0f);
    const bool push_lo = (out_pre <= -p->out_max) && (err < 0.0f);

    if (!push_hi && !push_lo) {
        p->i_term += p->ki * err * dt;
        p->i_term = clampf(p->i_term, -p->i_max, p->i_max);
    }

    /* ---------- 输出 ---------- */
    const float out = clampf(p_term + p->i_term + d_term, -p->out_max, p->out_max);

    p->p_term = p_term;
    p->d_term = d_term;
    p->out = out;
    return out;
}

float pid_step(pid_t *p, float err, float dt)
{
    if (!p) {
        return 0.0f;
    }
    return pid_core(p, err, err, dt);
}

float pid_step_meas(pid_t *p, float err, float meas, float dt)
{
    if (!p) {
        return 0.0f;
    }
    return pid_core(p, err, -meas, dt);
}

bool pid_is_saturated(const pid_t *p)
{
    if (!p || !(p->out_max > 0.0f)) {
        return false;
    }
    /* out 是被 clampf 精确夹到 ±out_max 的，所以等号成立即饱和 */
    return fabsf(p->out) >= p->out_max;
}

void pid_get_terms(const pid_t *p, float *p_term, float *i_term, float *d_term)
{
    if (!p) {
        return;
    }
    if (p_term) {
        *p_term = p->p_term;
    }
    if (i_term) {
        *i_term = p->i_term;
    }
    if (d_term) {
        *d_term = p->d_term;
    }
}
