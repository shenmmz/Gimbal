#pragma once
/* 通用 PID：位置式，带积分限幅、输出限幅、抗饱和、微分低通
 *
 * 依据：docs/云台固件架构设计.md §7.3
 * 用法（单轴串级里的速度环）：
 *     pid_t spd;
 *     pid_init(&spd, KP, KI, KD, OUT_MAX);
 *     pid_set_d_lpf(&spd, 200.0f, CTRL_FREQ_HZ);   // 微分先滤再乘 kd
 *     ...
 *     float vcmd = pid_step(&spd, target_rate - rate, CTRL_FREQ_HZ_PERIOD);
 *
 * 三个容易被忽略但会真出事的地方，本实现都处理了：
 *
 *   1. 积分饱和。只夹积分项不够 —— 夹住之后误差还在同一方向推，退出饱和要等很久。
 *      这里用"条件积分"：输出已经顶到限幅、且误差还往同一方向推时，本周期不累加。
 *
 *   2. 微分冲击。D 作用在误差上，目标角阶跃（切模式、上位机下发新角度）会让
 *      d(err)/dt 出现尖峰，输出瞬间打满。位置环建议只用 P，需要阻尼就把 D 加在
 *      速度环、并调 pid_step_meas() 让 D 只看测量值（见下）。
 *
 *   3. 复位后的第一次调用。prev 值是 0 而实际信号不是 0，直接算差分等于凭空造一个
 *      巨大微分。pid_reset() 之后第一次 pid_step() 只做"初始化 prev"，不产生微分。
 *
 * 增益与限幅在运行时会被上位机改写（协议 0x20 SET_PARAM），也可能来自 Flash 里的
 * 旧参数。所以 setter 会拒绝非有限值 —— 一个 NaN 增益能让输出永久 NaN，
 * 而 NaN 写进 CCR 是什么后果没人愿意试。
 */

#include <stdbool.h>

#include "filter.h"

typedef struct {
    /* ===== 参数（运行时可变） ===== */
    float kp;
    float ki;
    float kd;
    float out_max;   /* 输出限幅 ±out_max */
    float i_max;     /* 积分项限幅 ±i_max，单位与输出相同（已含 ki） */

    lpf_t d_lpf;     /* 微分低通；a = 1 时直通 */

    /* ===== 状态 ===== */
    float i_term;
    float prev_dsrc; /* 上一拍的微分源，用于差分 */
    float p_term;    /* 最近一次的分项，调试与遥测用 */
    float d_term;
    float out;       /* 最近一次输出；dt 非法或输入非有限时原样返回它 */
    bool  primed;    /* 复位后是否已经完成第一拍初始化 */
} pid_t;

/* 初始化。i_max 默认取 out_max（大多数场合够用），d_lpf 默认直通。
 * out_max <= 0 视为 0，此时输出恒为 0 —— 不要用 0 表示"不限幅"，
 * 裸机控制环里没有限幅等于没有保险。 */
void pid_init(pid_t *p, float kp, float ki, float kd, float out_max);

/* 清状态但保留参数。切换模式、从故障恢复、重新使能时调用。 */
void pid_reset(pid_t *p);

/* 运行时改参数。任何一项非有限值都会被忽略（保持原值），返回 false 提示调用方。 */
bool pid_set_gains(pid_t *p, float kp, float ki, float kd);

/* 单独设积分项限幅。传负数按绝对值处理，传 0 等于关掉积分。 */
void pid_set_i_limit(pid_t *p, float i_max);

/* 微分低通。fc_hz <= 0 或 fs_hz <= 0 表示不滤。
 * 建议 fc 取控制频率的 1/10 ~ 1/20：8 kHz 环用 200~500 Hz。 */
void pid_set_d_lpf(pid_t *p, float fc_hz, float fs_hz);

/* 一步计算。err = 目标 − 实测，dt 单位秒（8 kHz 环传 CTRL_PERIOD_S）。
 *
 * dt <= 0 或非有限、err 非有限时：不改任何状态，返回上次输出。
 * 这不是"容错"而是"止损"—— 让输出停在上一个合法值，比灌一个 inf 进积分器好。
 * 调用方应自己保证 dt 正确，把返回值异常当故障处理。 */
float pid_step(pid_t *p, float err, float dt);

/* 微分先行版本：D 作用在 −meas 上，而不是 err 上。
 * 目标阶跃时 meas 是连续的，所以没有微分冲击，这是串级控制里推荐的做法。
 * err 仍然照常参与 P 和 I。 */
float pid_step_meas(pid_t *p, float err, float meas, float dt);

/* 输出是否顶在限幅上。§11.5 的堵转判据"指令饱和 + 角度不跟随"里，
 * 前半句就靠这个判断（注意是这里的 PID 饱和，不是 svpwm 的 SVPWM_SATURATED）。 */
bool pid_is_saturated(const pid_t *p);

/* 读三个分项，调试与串口上报用（CONFIG_CTRL_DEBUG） */
void pid_get_terms(const pid_t *p, float *p_term, float *i_term, float *d_term);
