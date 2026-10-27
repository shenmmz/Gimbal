#pragma once
/* 极简断言框架 —— 只给 src/tests/ 用
 *
 * 不引 Unity/CMocka：这些测试要跑在 PC 上，但风格得跟固件一致（无动态分配、
 * 无异常），而且失败时要能一眼看到是哪个文件哪一行、实际值多少。
 *
 * 每个测试文件自己是一个 main()，末尾 TEST_END() 返回非 0 表示有失败，
 * CTest 据此判 pass/fail。
 */

#include <math.h>
#include <stdio.h>

static int t_checks = 0;
static int t_fails = 0;

#define TEST_BEGIN(name) \
    do { printf("[%s]\n", (name)); } while (0)

/* 条件断言。失败时打印表达式原文，省得回头翻代码 */
#define CHECK(cond)                                                          \
    do {                                                                     \
        t_checks++;                                                          \
        if (!(cond)) {                                                       \
            t_fails++;                                                       \
            printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);          \
        }                                                                    \
    } while (0)

/* 浮点近似断言。全部转 double 再算，避免在 float 上做差丢精度 */
#define CHECK_NEAR(actual, expect, tol)                                      \
    do {                                                                     \
        t_checks++;                                                          \
        const double a_ = (double)(actual);                                   \
        const double e_ = (double)(expect);                                   \
        const double t_ = (double)(tol);                                      \
        if (!(fabs(a_ - e_) <= t_)) {                                         \
            t_fails++;                                                       \
            printf("  FAIL %s:%d  %s = %.7f, 期望 %.7f ±%.1e\n",              \
                   __FILE__, __LINE__, #actual, a_, e_, t_);                  \
        }                                                                    \
    } while (0)

/* 角差断言：比较两个角在圆周上的最短距离，避免 ±π 处的假失败 */
#define CHECK_ANGLE(actual, expect, tol)                                     \
    do {                                                                     \
        t_checks++;                                                          \
        double d_ = fmod((double)(actual) - (double)(expect) + 3.14159265358979323846, \
                         6.28318530717958647692);                            \
        if (d_ < 0.0) {                                                      \
            d_ += 6.28318530717958647692;                                    \
        }                                                                    \
        d_ -= 3.14159265358979323846;                                        \
        if (!(fabs(d_) <= (double)(tol))) {                                   \
            t_fails++;                                                       \
            printf("  FAIL %s:%d  %s = %.6f, 期望 %.6f (差 %.6f rad)\n",      \
                   __FILE__, __LINE__, #actual, (double)(actual),             \
                   (double)(expect), d_);                                    \
        }                                                                    \
    } while (0)

#define TEST_END()                                                           \
    do {                                                                     \
        printf("  %d 项检查, %d 项失败\n", t_checks, t_fails);                \
        return (t_fails == 0) ? 0 : 1;                                       \
    } while (0)
