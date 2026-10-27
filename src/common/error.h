#pragma once
/* 全局错误处理 —— 所有层都可以调用
 *
 * Error_Handler() 定义在 app/main.c（关中断 + 死循环），
 * 但 driver/algorithm/control 层不能直接包含 app 层的东西，
 * 所以在这里单独声明，链接时由 main.c 提供实现。
 *
 * 注意：只有"真的没救了"的错误才调用这个（比如时钟配置失败、DMA 初始化失败）。
 *       普通的参数错误、范围超限，用限幅或返回错误码处理，不要直接死循环。
 */

void Error_Handler(void);
