#include "hall.h"

#include <math.h>
#include <stddef.h>

#include "config.h"
#include "math_utils.h"

typedef struct {
    float   offset[2];
    float   gain[2];
    int8_t  dir;
    uint8_t pole_pairs;

    lpf_t   lpf_s;
    lpf_t   lpf_c;

    float   sin_th;
    float   cos_th;
    float   theta_mech;
    float   theta_elec;
    float   mag;

    bool    inited;
} hall_axis_t;

static hall_axis_t g_hall[AXIS_COUNT];

static hall_axis_t *hall_get(uint8_t axis)
{
    if (axis >= AXIS_COUNT) {
        return NULL;
    }
    return &g_hall[axis];
}

void hall_init(uint8_t axis, uint8_t pole_pairs)
{
    hall_axis_t *h = hall_get(axis);
    if (h == NULL) {
        return;
    }

    h->offset[0] = HALL_RAW_MID;
    h->offset[1] = HALL_RAW_MID;
    h->gain[0] = HALL_RAW_HALF;
    h->gain[1] = HALL_RAW_HALF;
    h->dir = 1;
    h->pole_pairs = (pole_pairs == 0u) ? 1u : pole_pairs;

    /* 初始相位未知，先把滤波器输出置到"零角度"对应的 sin/cos，
     * 避免上电头几个周期从 (0,0) 爬出来导致 atan2 抖动。 */
    lpf_init(&h->lpf_s, HALL_DEFAULT_LPF_HZ, (float)CTRL_FREQ_HZ);
    lpf_init(&h->lpf_c, HALL_DEFAULT_LPF_HZ, (float)CTRL_FREQ_HZ);
    lpf_reset(&h->lpf_s, 0.0f);
    lpf_reset(&h->lpf_c, 1.0f);

    h->sin_th = 0.0f;
    h->cos_th = 1.0f;
    h->theta_mech = 0.0f;
    h->theta_elec = 0.0f;
    h->mag = 1.0f;
    h->inited = true;
}

void hall_set_calib(uint8_t axis, const float offset[2], const float gain[2], int8_t dir)
{
    hall_axis_t *h = hall_get(axis);
    if ((h == NULL) || (offset == NULL) || (gain == NULL)) {
        return;
    }

    /* 增益为 0 会让归一化除零。宁可保留默认值并让调用方从返回值发现，
     * 也不要在控制环里产生 inf。这里直接拒绝非法标定。 */
    if ((fabsf(gain[0]) < 1.0f) || (fabsf(gain[1]) < 1.0f)) {
        return;
    }

    h->offset[0] = offset[0];
    h->offset[1] = offset[1];
    h->gain[0] = gain[0];
    h->gain[1] = gain[1];
    h->dir = (dir < 0) ? -1 : 1;
}

void hall_set_lpf(uint8_t axis, float fc_hz, float fs_hz)
{
    hall_axis_t *h = hall_get(axis);
    if (h == NULL) {
        return;
    }
    lpf_init(&h->lpf_s, fc_hz, fs_hz);
    lpf_init(&h->lpf_c, fc_hz, fs_hz);
}

bool hall_decode(uint8_t axis, const uint16_t raw[2])
{
    hall_axis_t *h = hall_get(axis);
    if ((h == NULL) || (raw == NULL) || (!h->inited)) {
        return false;
    }

    /* ① 贴轨检查。必须用原始码判，不能放在归一化之后 —— 归一化会把它
     *    变成 1.137 这种"看起来还算正常"的数，再经 atan2 得到个假角度。
     *    单通道贴轨时 mag 只有 1.31，落在合法区间内，幅值判据抓不住。 */
    const bool rail = (raw[0] <= HALL_RAW_RAIL_LO) || (raw[0] >= HALL_RAW_RAIL_HI) ||
                      (raw[1] <= HALL_RAW_RAIL_LO) || (raw[1] >= HALL_RAW_RAIL_HI);

    /* ② 两路各自归一化。偏置/增益不同，不分开归一化会留下 2θ 周期误差。 */
    const float va = ((float)raw[0] - h->offset[0]) / h->gain[0];
    const float vb = ((float)raw[1] - h->offset[1]) / h->gain[1];

    /* ③ 120° 重构：sin 直接取 A，cos 由 a+2b 反解。
     *    dir 只乘在 cos 上 —— 它修正的是"b 超前 120° 而非滞后"，
     *    乘上去等价于把角度做一次镜像，方向随之反向。 */
    float s = va;
    float c = -(vb + 0.5f * va) * MATH_2_SQRT3 * (float)h->dir;

    /* ④ 滤 sin/cos 而不是滤角度：角度在 ±π 处跳变，直接滤会得到锯齿。 */
    s = lpf_update(&h->lpf_s, s);
    c = lpf_update(&h->lpf_c, c);

    h->sin_th = s;
    h->cos_th = c;

    const float mag = sqrtf(s * s + c * c);
    h->mag = mag;

    h->theta_mech = atan2f(s, c);

    /* ⑤ 电角度：情况 A 下 θ_mech 是机械角，乘极对数得到电角度。
     * 用 wrap_pi 而不是直接乘，避免 p 较大时数值变大后精度下降。 */
    h->theta_elec = wrap_pi((float)h->pole_pairs * h->theta_mech);

    return (!rail) && (mag >= HALL_MAG_MIN) && (mag <= HALL_MAG_MAX);
}

float hall_get_mech(uint8_t axis)
{
    hall_axis_t *h = hall_get(axis);
    return (h == NULL) ? 0.0f : h->theta_mech;
}

float hall_get_elec(uint8_t axis)
{
    hall_axis_t *h = hall_get(axis);
    return (h == NULL) ? 0.0f : h->theta_elec;
}

float hall_get_mag(uint8_t axis)
{
    hall_axis_t *h = hall_get(axis);
    return (h == NULL) ? 0.0f : h->mag;
}

void hall_get_sincos(uint8_t axis, float *sin_th, float *cos_th)
{
    hall_axis_t *h = hall_get(axis);
    if (h == NULL) {
        return;
    }
    if (sin_th != NULL) {
        *sin_th = h->sin_th;
    }
    if (cos_th != NULL) {
        *cos_th = h->cos_th;
    }
}
