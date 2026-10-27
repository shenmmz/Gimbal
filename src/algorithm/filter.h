#pragma once
/* 滤波器：一阶低通
 *
 * 只做纯计算，不依赖硬件。二阶陷波（抑制减振球 10–30 Hz 共振）等 IMU 上线时再加，
 * 现在没有 IMU 数据，加了也没法验证。
 */

typedef struct {
    float a;   /* 系数，1.0 = 直通（不滤） */
    float y;   /* 上次输出 */
} lpf_t;

/* fc_hz <= 0 或 fs_hz <= 0 时按直通处理（a=1），方便测试与"临时关掉滤波" */
void  lpf_init(lpf_t *f, float fc_hz, float fs_hz);

/* 重置输出，避免上电第一个周期从 0 缓慢爬升 */
void  lpf_reset(lpf_t *f, float y0);

float lpf_update(lpf_t *f, float in);

/* 一阶低通在频率 f_hz 处的相位滞后（度）。
 * 用来评估"霍尔滤波 300 Hz"这类参数会吃掉多少相位裕度：
 * 在 300 Hz 处正好 45°，在 100 Hz 处约 18°。 */
float lpf_phase_lag_deg(float f_hz, float fc_hz);
