#include "stm32f4xx_hal.h"

#include <stddef.h>

#include "board.h"
#include "pwm.h"
#include "types.h"
#include "error.h"

/* 占空比限幅：不能到 0 或 1，留一点余量避免芯片死区饱和 */
#define DUTY_MIN  0.02f
#define DUTY_MAX  0.98f

/* ARR 值：168 MHz / 24 kHz / 2（中心对齐） - 1 = 3499 */
/* 等等，中心对齐模式：频率 = SYSCLK / (PSC+1) / (2*ARR) */
/* 168e6 / 7000 / 2 = 12 kHz？不对... */
/* 重新算：中心对齐模式下，计数器从 0 到 ARR 再回到 0，一个完整周期 = 2*ARR 个计数 */
/* 频率 = CK_PSC / (2 * ARR)，所以 ARR = CK_PSC / (2 * f_pwm) */
/* CK_PSC = 168 MHz（APB2 定时器时钟 = APB2 × 2 = 84 × 2 = 168 MHz，对 TIM1 来说） */
/* 对 TIM3/TIM4（APB1）：CK_PSC = 42 × 2 = 84 MHz？不对 */
/* 等等，STM32F4 的 APB1 定时器时钟：如果 APB1 预分频器 = 1，则定时器时钟 = APB1 时钟 */
/*                     如果 APB1 预分频器 != 1，则定时器时钟 = APB1 时钟 × 2 */
/* 我们 APB1 预分频器是 /4（42 MHz），所以定时器时钟 = 42 × 2 = 84 MHz */
/* APB2 预分频器是 /2（84 MHz），定时器时钟 = 84 × 2 = 168 MHz */
/*
 * 所以：
 *   TIM1（APB2）：168 MHz 时钟
 *   TIM3（APB1）：84 MHz 时钟 ← 这就麻烦了，频率不一样！
 *   TIM4（APB1）：84 MHz 时钟
 *
 * 中心对齐模式 PWM 频率 = CK_INT / (PSC+1) / (2*ARR)
 *
 * 方案：TIM1 用 PSC=0, ARR=3499 → 168e6 / 7000 / 2 = 12 kHz？不对
 * 168e6 / (2*3499) ≈ 24006 Hz ≈ 24 kHz ✓  （2*3500=7000, 168e6/7000=24k）
 *
 * TIM3/TIM4 是 84 MHz，如果也用 ARR=3499：
 * 84e6 / 7000 / 2 = 6 kHz？不对
 * 84e6 / 7000 = 12 kHz（边沿对齐），中心对齐是一半 = 6 kHz？不对...
 *
 * 让我重新理解中心对齐模式：
 * 在中心对齐模式下，计数器先从 0 向上计数到 ARR，再向下计数到 0。
 * PWM 输出在向上计数时比较匹配置位，向下计数时比较匹配复位（或相反）。
 * 所以一个完整的 PWM 周期 = 2 × ARR 个定时器时钟周期。
 * PWM 频率 = f_tim / (2 × ARR)
 *
 * TIM1: f_tim = 168 MHz, 要 24 kHz → ARR = 168e6 / (2 × 24e3) = 168e6 / 48e3 = 3500 → ARR=3499
 * TIM3/TIM4: f_tim = 84 MHz, 要 24 kHz → ARR = 84e6 / 48e3 = 1750 → ARR=1749
 *
 * 但这样三轴的 ARR 不一样，占空比换算就不一样了...
 * 而且同步也会有问题。
 *
 * 等等，我再查一下：STM32F4 的定时器时钟
 * RCC_DCKCFGR 寄存器里有 TIMPRE 位，可以控制 APB1 定时器的时钟。
 * 默认情况下（TIMPRE=0）：
 *   - APB1 预分频器 = 1 → 定时器时钟 = APB1 时钟
 *   - APB1 预分频器 > 1 → 定时器时钟 = APB1 时钟 × 2
 * 我们 APB1 是 /4，所以定时器时钟 = 42 × 2 = 84 MHz
 *
 * 如果设置 TIMPRE=1：
 *   - APB1 预分频器 = 1 → 定时器时钟 = APB1 时钟
 *   - APB1 预分频器 > 1 → 定时器时钟 = APB1 时钟 × 4
 * 那就是 42 × 4 = 168 MHz，和 TIM1 一样了！
 *
 * 但这是 STM32F42xxx/43xxx/44xxx/46xxx/47xxx 才有的功能。
 * STM32F405 有没有 TIMPRE？查一下...
 *
 * 实际上 STM32F405 是有 RCC_DCKCFGR 寄存器的 TIMPRE 位的（参考手册 RM0090）。
 * 设置后 APB1 定时器时钟 = APB1 × 4 = 42 × 4 = 168 MHz，这样三轴就统一了。
 *
 * 不过... 先不急，W1 阶段先按各自频率算好 ARR，让波形出来就行。
 * W2 做同步的时候再统一调整。
 *
 * 或者更简单：TIM1 用 PSC=0 ARR=3499 → 24 kHz
 * TIM3/TIM4 用 PSC=0 ARR=1749 → 24 kHz（因为 84 MHz 时钟）
 * 占空比按比例换算，接口层还是 0~1 的 float。
 *
 * 这样 W1 能跑起来，W2 再优化。
 *
 * 但这引入了额外的复杂性。我还是用 TIMPRE 吧，让三个定时器都是 168 MHz。
 * 只需要在 clock_init 里加一句：
 *   __HAL_RCC_TIMCLKPRESCALER(RCC_TIMPRES_ACTIVATED);
 * 不对，HAL 库的宏是...
 *
 * 算了，W1 阶段不纠结这个，直接让 TIM1 跑 24 kHz，TIM3/TIM4 也跑 24 kHz，
 * 各自按自己的时钟算 ARR。表驱动里把 ARR 存进去就行。
 *
 * 反正占空比接口是 0~1 的 float，内部按比例换算成 CCR。
 */

/* 每轴 PWM 配置表 */
typedef struct {
    TIM_TypeDef *tim;
    GPIO_TypeDef *port;
    uint8_t af;
    uint16_t pin[PHASE_COUNT];       /* A/B/C 相对应的引脚 */
    uint32_t channel[PHASE_COUNT];   /* A/B/C 相对应的 TIM_CHANNEL_x */
    uint32_t arr;                    /* 自动重装载值（决定周期） */
    bool is_advanced;                /* 是否高级定时器（TIM1/8，需要 MOE 位） */
} pwm_axis_cfg_t;

static const pwm_axis_cfg_t s_axis[AXIS_COUNT] = {
    /* M1 = TIM4，APB1（84 MHz），倒序 CH3/CH2/CH1，PB8/7/6 */
    {
        .tim     = TIM4,
        .port    = BOARD_PWM_M1_PORT,
        .af      = BOARD_PWM_M1_AF,
        .pin     = { BOARD_PWM_M1_A_PIN, BOARD_PWM_M1_B_PIN, BOARD_PWM_M1_C_PIN },
        .channel = { TIM_CHANNEL_3, TIM_CHANNEL_2, TIM_CHANNEL_1 },
        .arr     = 1749u,   /* 84e6 / (2*1750) = 24 kHz */
        .is_advanced = false,
    },
    /* M2 = TIM1，APB2（168 MHz），正序 CH1/CH2/CH3，PA8/9/10 */
    {
        .tim     = TIM1,
        .port    = BOARD_PWM_M2_PORT,
        .af      = BOARD_PWM_M2_AF,
        .pin     = { BOARD_PWM_M2_A_PIN, BOARD_PWM_M2_B_PIN, BOARD_PWM_M2_C_PIN },
        .channel = { TIM_CHANNEL_1, TIM_CHANNEL_2, TIM_CHANNEL_3 },
        .arr     = 3499u,   /* 168e6 / (2*3500) = 24 kHz */
        .is_advanced = true,
    },
    /* M3 = TIM3，APB1（84 MHz），CH4/CH3/CH2，PC9/8/7 */
    {
        .tim     = TIM3,
        .port    = BOARD_PWM_M3_PORT,
        .af      = BOARD_PWM_M3_AF,
        .pin     = { BOARD_PWM_M3_A_PIN, BOARD_PWM_M3_B_PIN, BOARD_PWM_M3_C_PIN },
        .channel = { TIM_CHANNEL_4, TIM_CHANNEL_3, TIM_CHANNEL_2 },
        .arr     = 1749u,   /* 84e6 / (2*1750) = 24 kHz */
        .is_advanced = false,
    },
};

/* 三个定时器句柄 */
static TIM_HandleTypeDef s_htim[AXIS_COUNT];

/* HAL 在 HAL_TIM_PWM_Init 里回调本函数，做时钟与引脚配置 */
void HAL_TIM_PWM_MspInit(TIM_HandleTypeDef *htim)
{
    GPIO_InitTypeDef gpio = {0};
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_PULLDOWN;   /* PWM 引脚外部有下拉，内部也下拉更安全 */
    gpio.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;

    for (uint8_t i = 0; i < AXIS_COUNT; i++) {
        if (htim->Instance != s_axis[i].tim) {
            continue;
        }

        /* 开定时器时钟 */
        if (s_axis[i].tim == TIM1) {
            __HAL_RCC_TIM1_CLK_ENABLE();
            __HAL_RCC_GPIOA_CLK_ENABLE();
        } else if (s_axis[i].tim == TIM3) {
            __HAL_RCC_TIM3_CLK_ENABLE();
            __HAL_RCC_GPIOC_CLK_ENABLE();
        } else if (s_axis[i].tim == TIM4) {
            __HAL_RCC_TIM4_CLK_ENABLE();
            __HAL_RCC_GPIOB_CLK_ENABLE();
        }

        /* 配置三路 PWM 引脚 */
        gpio.Alternate = s_axis[i].af;
        for (uint8_t p = 0; p < PHASE_COUNT; p++) {
            gpio.Pin = s_axis[i].pin[p];
            HAL_GPIO_Init(s_axis[i].port, &gpio);
        }
        break;
    }
}

/* 配置单个定时器为中心对齐 PWM 模式 */
static void pwm_config_timer(uint8_t axis)
{
    TIM_HandleTypeDef *htim = &s_htim[axis];
    const pwm_axis_cfg_t *cfg = &s_axis[axis];

    htim->Instance               = cfg->tim;
    htim->Init.Prescaler         = 0;
    htim->Init.CounterMode       = TIM_COUNTERMODE_CENTERALIGNED1;  /* 向上时匹配置位 */
    htim->Init.Period            = cfg->arr;
    htim->Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim->Init.RepetitionCounter = 0;
    htim->Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    if (HAL_TIM_PWM_Init(htim) != HAL_OK) {
        Error_Handler();
    }

    /* 配置三路通道 */
    TIM_OC_InitTypeDef oc = {0};
    oc.OCMode     = TIM_OCMODE_PWM1;
    oc.Pulse      = cfg->arr / 2u;   /* 默认 50% 占空比 */
    oc.OCPolarity = TIM_OCPOLARITY_HIGH;
    oc.OCFastMode = TIM_OCFAST_DISABLE;

    for (uint8_t p = 0; p < PHASE_COUNT; p++) {
        if (HAL_TIM_PWM_ConfigChannel(htim, &oc, cfg->channel[p]) != HAL_OK) {
            Error_Handler();
        }
        HAL_TIM_PWM_Start(htim, cfg->channel[p]);
    }

    /* 高级定时器需要开主输出 */
    if (cfg->is_advanced) {
        __HAL_TIM_MOE_ENABLE(htim);
    }
}

void pwm_init(void)
{
    for (uint8_t i = 0; i < AXIS_COUNT; i++) {
        pwm_config_timer(i);
    }
}

void pwm_set_duty(uint8_t axis, const float duty[3])
{
    if ((axis >= AXIS_COUNT) || (duty == NULL)) {
        return;
    }

    const pwm_axis_cfg_t *cfg = &s_axis[axis];
    TIM_HandleTypeDef *htim = &s_htim[axis];

    for (uint8_t p = 0; p < PHASE_COUNT; p++) {
        float d = CLAMP(duty[p], DUTY_MIN, DUTY_MAX);
        uint32_t ccr = (uint32_t)(d * (float)cfg->arr);
        __HAL_TIM_SET_COMPARE(htim, cfg->channel[p], ccr);
    }
}

void pwm_set_zero(uint8_t axis)
{
    float zero[3] = { 0.5f, 0.5f, 0.5f };
    pwm_set_duty(axis, zero);
}

void pwm_set_all_zero(void)
{
    for (uint8_t i = 0; i < AXIS_COUNT; i++) {
        pwm_set_zero(i);
    }
}
