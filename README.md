# Gimbal · 无人机三轴相机云台固件

STM32F405 裸机固件，驱动三轴 BLDC 云台（Yaw / Pitch / Roll）。
8 kHz 控制环、24 kHz PWM、电压模式 FOC、线性霍尔直接解绝对角。

> **状态：开发环境已就绪，固件处于 W1（板级自检）阶段。**
> 已完成：168 MHz 时钟树、串口 printf、构建与测试链路。
> **未完成：上板验证**（开发板尚未到手，见《开发执行计划》W1）。

---

## 硬件

| 板 | 型号 | 数量 | 核心器件 |
|---|---|---|---|
| 主控板 | IUO112 | 1 | STM32F405RGT6（LQFP-64 / 168 MHz / 1 MB Flash / 192 KB RAM / FPv4-SP）+ 3× SS6343M 三相驱动 |
| 电机端板 | BOD12-11 | 3 | 2× MT9101ET 线性（模拟）霍尔 |
| IMU 板 | R14AQ | 1 | ICM-42688-P（4 线 SPI）+ DS8242-33S3 LDO + 加热回路 |
| 内部总线 | 40P FPC | — | 主板到三块子板的唯一通道 |

**硬件已冻结，不改板。** 以下限制直接决定了固件形态，不可绕开：

| 硬件事实 | 固件后果 |
|---|---|
| 无相电流采样（LSS 直接接地） | 只能做**电压模式 FOC**，没有电流环 |
| 霍尔输出模拟电压（绝对角） | 上电即知位置，**不需要预定位 / 开环强拖** |
| 3 路 nFAULT 悬空 | 保护只能靠"指令饱和 + 角度不跟随"判堵转 |
| 母线分压 1:2 超量程（7.2 V → 3.6 V） | 只能做欠压判别，前馈用固定常量 |
| IMU 的 INT1/INT2 悬空 | 只能轮询，**控制时基必须由定时器决定** |
| 三轴 PWM 通道顺序各不相同 | 必须**表驱动**，不能假设顺序一致 |

---

## 快速开始

环境只需装一个包（Arm GNU Toolchain），CMake / Ninja / MSVC 复用 Visual Studio 自带的。

```bat
:: 1) 装工具链（一次性，约 323 MB）
scripts\setup-toolchain.cmd

:: 2) 每次开新终端，先注入环境
scripts\env.cmd

:: 3) 编固件
cmake --preset arm
cmake --build --preset arm

:: 4) 跑 PC 端单元测试（算法层不依赖硬件）
cmake --preset host && cmake --build --preset host && ctest --preset host

:: 5) 烧录（ST-Link + OpenOCD）
openocd -f interface/stlink.cfg -c "transport select swd" -f target/stm32f4x.cfg ^
        -c "program build/arm/gimbal.elf verify reset exit"
```

产物在 `build/arm/`：`gimbal.elf` / `gimbal.bin` / `gimbal.hex` / `gimbal.map`。
当前占用 **FLASH 23,152 B（2.52%）· RAM 6,032 B（4.60%）**。

---

## 目录结构

```
Gimbal/
├── stm32/        ST 官方 HAL + CMSIS（原样引入，禁止修改）
├── src/          只有自己写的代码
│   ├── app/      入口、状态机、协议、参数、保护
│   ├── control/  PID、单轴串级、三轴协调、前馈
│   ├── algorithm/ 霍尔解算、SVPWM、IMU、滤波、标定
│   ├── driver/   pwm / adc / spi / uart / gpio / nvm / timebase
│   ├── common/   board.h（引脚唯一事实来源）、hal_conf.h
│   └── tests/    PC 端单元测试
├── cmake/        交叉编译工具链文件
├── ld/           链接脚本（FLASH 896 KB + 末 128 KB 参数区）
├── scripts/      env.cmd / setup-toolchain.cmd
├── external/     原理图与规格书
└── docs/         设计文档
```

---

## 文档

| 文档 | 回答什么 |
|---|---|
| [云台固件架构设计](docs/云台固件架构设计.md) | 代码怎么组织：5 层分层、22 个模块、状态机、保护策略、编码约定 |
| [FOC技术栈](docs/FOC技术栈.md) | 信号怎么走：从霍尔到 PWM 的 9 层，一个 125 µs 周期的完整演算 |
| [开发执行计划](docs/开发执行计划.md) | 13 周每周做什么、每天做什么、验收清单 |
| [开发环境链路](docs/开发环境链路.md) | 代码怎么变成烧进芯片的字节：7 环链路、断链定位表 |
| [硬件设计总结](docs/硬件设计总结.md) | 系统由哪些板组成、电源怎么走、关键参数 |
| [IUO112_引脚配置清单](docs/IUO112_引脚配置清单.md) | 64 个引脚逐个怎么用 |

**建议阅读顺序**：硬件设计总结 → 引脚配置清单 → 架构设计 → FOC技术栈 → 开发执行计划。

---

## 许可

[MIT](LICENSE)
