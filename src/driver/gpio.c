#include "stm32f4xx_hal.h"

#include "board.h"
#include "gpio.h"
#include "types.h"

/* 三路 nSLEEP 的端口/引脚表（顺序 = axis 索引） */
static GPIO_TypeDef *const s_sleep_port[AXIS_COUNT] = {
    BOARD_SLEEP_M1_PORT,
    BOARD_SLEEP_M2_PORT,
    BOARD_SLEEP_M3_PORT,
};

static const uint16_t s_sleep_pin[AXIS_COUNT] = {
    BOARD_SLEEP_M1_PIN,
    BOARD_SLEEP_M2_PIN,
    BOARD_SLEEP_M3_PIN,
};

void gpio_init(void)
{
    GPIO_InitTypeDef gpio = {0};

    /* ---- nSLEEP：推挽输出，默认低（睡眠态，安全） ---- */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;

    for (uint8_t i = 0; i < AXIS_COUNT; i++) {
        HAL_GPIO_WritePin(s_sleep_port[i], s_sleep_pin[i], GPIO_PIN_RESET);
        gpio.Pin = s_sleep_pin[i];
        HAL_GPIO_Init(s_sleep_port[i], &gpio);
    }

    /* ---- 加热：推挽输出，默认关 ---- */
    __HAL_RCC_GPIOC_CLK_ENABLE();  /* 上面已经开了，保险起见再调一次也没事 */
    HAL_GPIO_WritePin(BOARD_HEATER_PORT, BOARD_HEATER_PIN, GPIO_PIN_RESET);
    gpio.Pin = BOARD_HEATER_PIN;
    HAL_GPIO_Init(BOARD_HEATER_PORT, &gpio);
}

void gpio_set_sleep(uint8_t axis, bool sleep)
{
    if (axis >= AXIS_COUNT) {
        return;
    }
    HAL_GPIO_WritePin(s_sleep_port[axis], s_sleep_pin[axis],
                      sleep ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

void gpio_set_all_sleep(bool sleep)
{
    for (uint8_t i = 0; i < AXIS_COUNT; i++) {
        gpio_set_sleep(i, sleep);
    }
}

void gpio_set_heater(bool on)
{
    HAL_GPIO_WritePin(BOARD_HEATER_PORT, BOARD_HEATER_PIN,
                      on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}
