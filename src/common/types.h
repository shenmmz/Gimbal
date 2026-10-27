#pragma once
/* 公共类型与枚举 —— driver 以上所有层都可以包含
 *
 * 规则：这里只放"纯类型"，不放任何硬件相关的常量（那些放 board.h）。
 *       algorithm/ 与 control/ 也可以安全地包含本文件。
 */

#include <stdint.h>
#include <stdbool.h>

/* ===== 轴索引 —— 全项目统一使用 0/1/2 =====
 * 驱动层的"轴"就是电机序号：M1/M2/M3（板丝印）。
 * yaw/pitch/roll 的映射是开放决策 Q2（等板子实测丝印/装配后拍板），
 * 拍板后由 control/gimbal.c 的映射表定义，本文件不预定，防止把未定项写死。
 */
#define AXIS_M1     0u
#define AXIS_M2     1u
#define AXIS_M3     2u
#define AXIS_COUNT  3u

/* ===== 三相索引 ===== */
#define PHASE_A     0u
#define PHASE_B     1u
#define PHASE_C     2u
#define PHASE_COUNT 3u

/* ===== 常用限幅宏 ===== */
#define CLAMP(x, lo, hi)  ((x) < (lo) ? (lo) : ((x) > (hi) ? (hi) : (x)))
#define MIN(a, b)         ((a) < (b) ? (a) : (b))
#define MAX(a, b)         ((a) > (b) ? (a) : (b))
