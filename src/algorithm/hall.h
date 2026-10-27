#pragma once
/* 霍尔角度解算：两路线性霍尔 → 归一化 → 120° 重构 → 反正切 → 角度
 *
 * 依据：docs/云台固件架构设计.md §7.2.1
 *
 * 硬件事实（BOD12-11 电机板）：
 *   - 每轴 2 颗 MT9101ET 线性霍尔，机械夹角 120°，只有模拟输出，没有 A/B/Z 数字信号；
 *   - 输出是比例式（ratiometric），以 Vcc/2 为零点，ADC 原始码中点约 2048。
 *
 * 解算公式（120° 版，只此一套，不做 90°/120° 编译期切换）：
 *     a = sin θ
 *     b = sin(θ − 120°) = −0.5·sin θ − 0.866·cos θ
 *     ⇒ sin θ = a
 *       cos θ = −dir · (b + 0.5a) · 2/√3
 *       θ     = atan2(sin θ, cos θ)
 *
 * 三个容易写错的地方（都在 .c 里处理了）：
 *   1. 滤波必须滤 sin/cos 再 atan2，不能直接滤角度 —— 角度在 ±π 处会跳变 2π；
 *   2. 两路必须各自归一化，否则角度上会留下 2θ 周期误差（典型 1~5°）；
 *   3. dir 只作用于 cos 项，它处理的是"b 超前还是滞后 120°"，不是"电机反转"。
 */

#include <stdbool.h>
#include <stdint.h>

#include "filter.h"
#include "types.h"

/* 未标定时的默认值：12 位 ADC 中点与半幅 */
#define HALL_RAW_MID        2048.0f
#define HALL_RAW_HALF       2048.0f

/* 归一化后 √(sin²+cos²) 的合法区间。
 * 偏离 1 说明：磁铁掉了/偏心、霍尔增益变了、或者两路同时异常。
 * 正常装配下实测应在 0.9~1.1，这里放宽到 0.5~1.5 只做"明显坏了"的判据，
 * 免得一上电就误报。 */
#define HALL_MAG_MIN        0.5f
#define HALL_MAG_MAX        1.5f

/* ADC 贴轨判据。
 *
 * ⚠ 单通道贴轨时归一化幅值只有约 1.31 —— 落在 HALL_MAG_MIN..MAX 之内，
 *   幅值判据**发现不了**。这是写单测时算出来的，不是猜的：
 *   a = 4095 归一化后 1.137，b 正常，mag = √(1.137² + 0.657²) = 1.31。
 *   所以必须单独判轨，不能指望 mag。
 *
 * 贴轨的物理含义：输出被钉在电源轨（对 3.3 V 短路、或霍尔输出级失效）。
 * 线性霍尔正常工作点在 Vcc/2 ± 1 V，离轨至少 800 个码，这里留 32 码余量足够。
 * 断线悬空是另一回事 —— 读数会乱跳而不是贴轨，靠幅值判据 + 上位机观测发现。 */
#define HALL_RAW_RAIL_LO    32u
#define HALL_RAW_RAIL_HI    4063u

/* 默认滤波截止频率。8 kHz 采样下 300 Hz 对应相位滞后 45°（见 lpf_phase_lag_deg），
 * 位置环带宽一般 < 100 Hz，够用；转速高时可按需上调。 */
#define HALL_DEFAULT_LPF_HZ 300.0f

/* 初始化一轴。pole_pairs 用于 θ_elec = p · θ_mech，填 0 按 1 处理。
 * 必须在 hall_decode() 之前调用一次（三轴各一次）。 */
void hall_init(uint8_t axis, uint8_t pole_pairs);

/* 写入标定结果（由 calibration.c 转一整圈采 min/max 得到）
 *   offset[i] = (max + min) / 2
 *   gain[i]   = (max - min) / 2      —— 为 0 时会被拒绝，防止除零
 *   dir       = ±1，给正转看角度是否单调递增，反了就取 -1 */
void hall_set_calib(uint8_t axis, const float offset[2], const float gain[2], int8_t dir);

/* 设置解算前的一阶低通。fc_hz <= 0 表示不滤（测试用）。
 * fs_hz 传调用频率，控制环里就是 CTRL_FREQ_HZ。 */
void hall_set_lpf(uint8_t axis, float fc_hz, float fs_hz);

/* 输入两路原始码 raw[0]=A 相霍尔、raw[1]=B 相霍尔。
 * 返回 false 表示数据不可信，有两种原因：
 *   ① 归一化后幅值越界（HALL_MAG_MIN..MAX）；
 *   ② 任一通道贴轨（HALL_RAW_RAIL_LO..HI 之外）。
 * 无论哪种，角度都仍然更新 —— 调用方应据此累计故障计数，
 * 一次越界不算故障，持续越界才算（§11.5 的判据有效性边界）。 */
bool hall_decode(uint8_t axis, const uint16_t raw[2]);

/* 机械角，[-π, π)。情况 A（2 极角度磁环）下就是真实机械角 */
float hall_get_mech(uint8_t axis);

/* 电角度，[-π, π)，= wrap(p · θ_mech)。SVPWM 用这个 */
float hall_get_elec(uint8_t axis);

/* 归一化幅值，理想值 1.0，用于诊断 */
float hall_get_mag(uint8_t axis);

/* 取解算后的 sin/cos（滤波后），调试与标定用 */
void hall_get_sincos(uint8_t axis, float *sin_th, float *cos_th);
