#include "filter.h"

#include <math.h>

#include "math_utils.h"

void lpf_init(lpf_t *f, float fc_hz, float fs_hz)
{
    if (f == 0) {
        return;
    }

    /* 直通条件：参数非法、或截止频率已经到奈奎斯特以上 */
    if ((fc_hz <= 0.0f) || (fs_hz <= 0.0f) || (fc_hz >= 0.5f * fs_hz)) {
        f->a = 1.0f;
        f->y = 0.0f;
        return;
    }

    /* 双线性/后向差分等效：a = 1 - exp(-2π·fc/fs)
     * 用 expf 而不是 1/(1+2π·fc/fs)，前者在 fc 接近 fs/2 时更准。 */
    f->a = 1.0f - expf(-2.0f * MATH_PI * fc_hz / fs_hz);
    f->y = 0.0f;
}

void lpf_reset(lpf_t *f, float y0)
{
    if (f != 0) {
        f->y = y0;
    }
}

float lpf_update(lpf_t *f, float in)
{
    if (f == 0) {
        return in;
    }
    f->y += f->a * (in - f->y);
    return f->y;
}

float lpf_phase_lag_deg(float f_hz, float fc_hz)
{
    if ((fc_hz <= 0.0f) || (f_hz <= 0.0f)) {
        return 0.0f;
    }
    return atanf(f_hz / fc_hz) * (180.0f / MATH_PI);
}
