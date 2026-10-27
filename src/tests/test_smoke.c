/* 冒烟测试：验证 PC 端构建链可用、单精度浮点行为符合预期。
 *
 * 这不是算法验收。algorithm/ 与 control/ 的模块落地后，
 * 每个模块在这里补自己的用例（hall / svpwm / pid 优先）。
 */

#include <math.h>
#include <stdio.h>

int main(void)
{
    int failures = 0;

    if (sizeof(float) != 4u) {
        printf("FAIL: sizeof(float) = %u, 期望 4（FPv4-SP 只有单精度）\n",
               (unsigned)sizeof(float));
        failures++;
    }

    /* 单精度下 0.1f + 0.2f 与 0.3f 的差应在 1e-7 量级 */
    volatile float a = 0.1f;
    volatile float b = 0.2f;
    float sum = a + b;
    if (fabsf(sum - 0.3f) > 1e-6f) {
        printf("FAIL: 0.1f + 0.2f = %.9f\n", (double)sum);
        failures++;
    }

    /* atan2f 是 120° 霍尔解算的核心函数，先确认它在 host 上可用 */
    float angle = atan2f(1.0f, 0.0f) * 57.2957795f;
    if (fabsf(angle - 90.0f) > 0.01f) {
        printf("FAIL: atan2f(1,0) = %.3f°, 期望 90°\n", (double)angle);
        failures++;
    }

    if (failures == 0) {
        printf("smoke ok: 单精度浮点与 atan2f 正常\n");
        return 0;
    }
    return 1;
}
