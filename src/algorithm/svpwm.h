#pragma once
/* SVPWM：α-β 电压矢量 → 三相占空比
 *
 * 依据：docs/云台固件架构设计.md §7.2、§9 第 ⑤ 步
 *
 * 用"最大最小值注入"（min-max injection）实现，不查扇区表：
 *   1) 由 α-β 反 Clarke 得到三相线-中性点电压（幅值不变变换）
 *   2) 注入零序 v0 = −(max + min) / 2，把三相压到以 0 为中心
 *   3) duty_i = 0.5 + (v_i + v0) / vbus
 *
 * 这与教科书里"判断扇区 + 算相邻两矢量作用时间"的结果逐点相同，
 * 但省掉了 atan2、扇区判断和除法。8 kHz 环里每轴省几十个周期，三轴就是几百。
 *
 * 为什么必须注入零序：不注入时 duty = 0.5 + v/vbus，线性区只到 |u| = 0.5·vbus；
 * 注入后线性区扩到 |u| = vbus/√3 ≈ 0.577·vbus，多出 15.5% 的电压裕度。
 * 这就是"SVPWM 比 SPWM 多出 15% 母线利用率"的来源。
 *
 * ⚠ 满线性调制时占空比会**顶到 0 和 1**，不是留在中间某段。
 *   注入后的调制波峰值随角度变化：矢量对准某相轴时峰值 0.433（占空比 0.067/0.933），
 *   在相间 30° 处峰值 0.5（占空比正好 0 和 1）。所以 [0,1] 就是完整的线性区，
 *   没有余量。pwm.c 里 0.02~0.98 的限幅会吃掉约 17% 的调制范围 —— 那是死区和
 *   自举电容的代价，不是 bug。想让满调制不被削，就把电压指令限制在 0.83 以内。
 *
 * ⚠ 母线电压（vbus）在本项目里传的是**常量 7.2 V**，不是实测值：
 *   分压比 1:2，6.6 V 就满量程（附录 B 的 B3），PC0 读数在工作点恒为 4095。
 *   所以这里的"归一化"抵消不了母线波动，只保证占空比标定一致。
 */

#include <stdbool.h>

/* 线性调制上限系数：|u| = vbus · SVPWM_LINEAR_MAX_PU 就是线性区边界。
 * 1/√3，正好也是 svpwm_calc_norm() 里 "−1~+1 ↔ 伏特" 的换算系数。 */
#define SVPWM_LINEAR_MAX_PU  0.57735026918962576451f

typedef enum {
    SVPWM_OK = 0,        /* 正常，占空比落在 [0, 1] 内 */
    SVPWM_SATURATED = 1, /* 过调制：占空比被限幅（能力到顶，不是故障） */
    SVPWM_INVALID = 2    /* 输入非有限值或 vbus <= 0，已输出零矢量 0.5 */
} svpwm_status_t;

/* 注意 SVPWM_SATURATED 的触发点随角度变化：矢量对准相轴时 |u| = 2/3·vbus 才开始限幅，
 * 相间 30° 处 |u| = vbus/√3 就顶满了。判"过调制"要用状态位，别自己比幅值。 */

/* 输入 α-β 分量（伏特，线-中性点幅值），输出 0~1 的三相占空比。
 * 母线电压归一化在这里做，控制层不碰除法。
 *
 * 返回 SVPWM_INVALID 时 duty 已被置为 0.5/0.5/0.5（零电压矢量，电机不受力），
 * 这是有意的：宁可让电机软下来，也不要把 NaN 写进 CCR 让桥臂乱翻。
 * 调用方该做的是记一次故障，而不是继续用这组占空比。
 *
 * ⚠ SVPWM_SATURATED 不能当作"堵转"判据。§11.5 的堵转判据是"**指令**饱和"，
 *   指 PID 输出顶到 out_max，那发生在控制层（见 pid_is_saturated）。
 *   这里的饱和只说明电压矢量超出了 SVPWM 能表达的范围，二者不是一回事。 */
svpwm_status_t svpwm_calc(float u_alpha, float u_beta, float vbus, float duty[3]);

/* 归一化版本：u_alpha/u_beta 取 −1 ~ +1，1.0 对应线性调制上限 vbus/√3。
 * 控制层（axis.c）输出的是无量纲电压指令，直接调这个，不必知道 vbus。
 *
 * 数值上恒等于 svpwm_calc(u_alpha · vbus/√3, u_beta · vbus/√3, vbus, duty)，
 * 两个入口只是量纲不同，别混用。 */
svpwm_status_t svpwm_calc_norm(float u_alpha, float u_beta, float duty[3]);

/* 三相占空比置零矢量（0.5），电机自由但三相短接有弱阻尼。
 * 上电、故障、IDLE 都调它 —— 比 pwm_set_all_zero() 少一层调用，且不依赖驱动层。 */
void svpwm_zero(float duty[3]);
