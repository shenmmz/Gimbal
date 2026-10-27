#include "stm32f4xx_hal.h"

#include <errno.h>
#include <stddef.h>

#include "board.h"
#include "uart.h"

#define UART_TX_TIMEOUT_MS 100u

static UART_HandleTypeDef uart5_handle;

/* HAL 在 HAL_UART_Init() 里回调本函数，做时钟与引脚配置 */
void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
    if (huart->Instance != UART5) {
        return;
    }

    __HAL_RCC_UART5_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {0};
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_PULLUP;
    gpio.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = BOARD_DEBUG_UART_AF;

    gpio.Pin = BOARD_DEBUG_UART_TX_PIN;
    HAL_GPIO_Init(BOARD_DEBUG_UART_TX_PORT, &gpio);

    gpio.Pin = BOARD_DEBUG_UART_RX_PIN;
    HAL_GPIO_Init(BOARD_DEBUG_UART_RX_PORT, &gpio);
}

void uart_init(void)
{
    uart5_handle.Instance          = UART5;
    uart5_handle.Init.BaudRate     = BOARD_DEBUG_UART_BAUD;
    uart5_handle.Init.WordLength   = UART_WORDLENGTH_8B;
    uart5_handle.Init.StopBits     = UART_STOPBITS_1;
    uart5_handle.Init.Parity       = UART_PARITY_NONE;
    uart5_handle.Init.Mode         = UART_MODE_TX_RX;
    uart5_handle.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    uart5_handle.Init.OverSampling = UART_OVERSAMPLING_16;

    if (HAL_UART_Init(&uart5_handle) != HAL_OK) {
        __disable_irq();
        for (;;) {
        }
    }
}

void uart_write(const uint8_t *data, uint32_t length)
{
    if ((data == NULL) || (length == 0u)) {
        return;
    }
    (void)HAL_UART_Transmit(&uart5_handle, (uint8_t *)data, (uint16_t)length,
                            UART_TX_TIMEOUT_MS);
}

/* ===== newlib 重定向 ===== */

/* printf 输出走调试串口 */
int _write(int file, char *ptr, int len)
{
    (void)file;
    uart_write((const uint8_t *)ptr, (uint32_t)len);
    return len;
}

/* newlib 内部偶尔需要一小块堆（格式化缓冲）。
 * 应用层仍然禁止 malloc/free（架构文档 §14 第 6 条），这里只是防止库函数失败。 */
static uint8_t libc_heap[1024];
static size_t  libc_heap_used;

void *_sbrk(ptrdiff_t increment)
{
    if ((libc_heap_used + (size_t)increment) > sizeof(libc_heap)) {
        errno = ENOMEM;
        return (void *)-1;
    }
    void *ptr = &libc_heap[libc_heap_used];
    libc_heap_used += (size_t)increment;
    return ptr;
}
