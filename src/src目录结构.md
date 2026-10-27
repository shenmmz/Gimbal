# src 目录结构

依据《云台固件架构设计.md》§5。加 ★ 的是已经建好、能编进去的文件。

```
src/
├── app/                 入口与调度：只调用下层，不做计算
│   ├── ★ main.c         入口、初始化顺序、主循环
│   ├── fsm.c/.h         系统状态机
│   ├── mode.c/.h        工作模式调度
│   ├── protocol.c/.h    上位机协议收发
│   ├── params.c/.h      参数读写与校验
│   ├── protect.c/.h     保护与故障处理
│   └── thermal.c/.h     IMU 加热恒温
├── control/             控制算法：纯计算，可脱离硬件在 PC 上跑
│   ├── pid.c/.h         PID 通用实现
│   ├── axis.c/.h        单轴串级控制
│   ├── gimbal.c/.h      三轴协调、轴映射
│   └── feedforward.c/.h 前馈补偿
├── algorithm/           信号处理：纯计算，可脱离硬件在 PC 上跑
│   ├── hall.c/.h        霍尔角度解算
│   ├── svpwm.c/.h       SVPWM 与占空比生成
│   ├── imu.c/.h         IMU 读取与姿态解算
│   ├── filter.c/.h      低通 / 陷波 / 互补滤波
│   └── calibration.c/.h 标定算法
├── driver/              驱动：只读写寄存器，不做业务判断
│   ├── ★ pwm.c/.h       三轴 PWM 与同步（TIM1 主 + TIM3/TIM4 从）
│   ├── ★ adc.c/.h       霍尔 + 母线电压采集（6 路注入组 + vbus）
│   ├── spi.c/.h         IMU SPI 读写
│   ├── ★ uart.c/.h      串口收发
│   ├── ★ gpio.c/.h      nSLEEP、加热、LED
│   ├── nvm.c/.h         Flash 参数存储
│   └── ★ timebase.c/.h  微秒延时、时间戳、8 kHz 调度计数
├── common/              公共设施
│   ├── ★ board.h        引脚映射与硬件常量（单一事实来源）
│   ├── ★ config.h       编译期开关、频率配置
│   ├── ★ types.h        公共类型、枚举
│   ├── ★ stm32f4xx_hal_conf.h  裁剪 STM32 HAL 模块（HAL 硬编码此名，改不得）
│   ├── ★ error.h        错误码与 ASSERT 宏
│   ├── math_utils.c/.h  角度折返、最短角差、限幅
│   └── ring_buffer.c/.h 环形缓冲
└── tests/               PC 端单元测试（只测 control/ 和 algorithm/）
    ├── ★ CMakeLists.txt
    └── ★ test_smoke.c
```

## 三条必须记住的规则

1. **依赖只能自上往下**：`app → control/algorithm → driver → stm32/`。反向引用一次都不行。
2. **`control/` 和 `algorithm/` 里不许出现 `stm32f4xx_hal.h`**。`tests/` 的头文件路径只给这两个目录，一耦合就编译失败——这是故意的。
3. **include 从 `src/` 起算**：`#include "algorithm/hall.h"`，不写 `../../../`。

## 当前进度

已建：`app/main.c`、`common/`（6 个 .h）、`driver/` 的 `uart` `gpio` `pwm` `adc` `timebase`、`tests/`。
下一步是 `algorithm/` 的 `hall` 与 `svpwm`（纯计算，先在 `tests/` 里跑通再上板）。
