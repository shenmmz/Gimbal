#pragma once
/* 串口：调试口 UART5（J4），115200 8N1
 * 同时提供 printf 重定向所需的 _write 实现。
 */

#include <stdint.h>

void uart_init(void);
void uart_write(const uint8_t *data, uint32_t length);
