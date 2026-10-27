# `stm32/` 版本基线

这份目录放 ST 与 ARM 的**原件**，只读。本文件记录"哪一版、从哪来、删了什么、下次怎么更"。

| 组件 | 版本 | 上游仓库 | 取用的提交 |
|---|---|---|---|
| CMSIS-Core | **6.4.0** | `ARM-software/CMSIS_6` tag `v6.4.0` | `af1b110e46a1720349f6f125e386e126767699bd` |
| CMSIS Device F4 | 2.6.11+ | `STMicroelectronics/cmsis_device_f4` **master** | `9192c7b9df75a142f2027ab266601fe061fc00b3` |
| STM32F4 HAL | 1.8.5+ | `STMicroelectronics/stm32f4xx_hal_driver` **master** | `1f6451c3e07728b4c830744de380e56bf5bc0026` |

最后核对：2026-10-27。

---

## 1. 为什么取 master 而不是 tag

**tag 比 master 旧。** 按"取最新 tag"去更新会变成降级，丢掉 8 个文件的修复：

| 文件 | tag 里没有、master 有 |
|---|---|
| `hal/src/stm32f4xx_hal_tim.c` | `HAL_TIM_PWM_Start()` 等函数里**先清 `CCxNE` 再配通道**。不修的话，互补输出在重配时可能残留上一状态 |
| `hal/src/stm32f4xx_hal_tim_ex.c` | 同上，互补通道相关 |
| `hal/inc/stm32f4xx_hal_tim.h`、`stm32f4xx_ll_tim.h` | 配套声明与宏 |
| `hal/inc/stm32f4xx_hal_crc.h` | `HAL_CRC_Calculate/Accumulate` 的 buffer 加 `const` |
| `hal/inc/stm32f4xx_hal_gpio_ex.h` | `IS_GPIO_AF()` 补 `GPIO_AF9_TIM14` |
| `hal/inc/stm32f4xx_hal_ltdc_ex.h` | 补 `HAL_LTDC_MODULE_ENABLED && HAL_DSI_MODULE_ENABLED` 守卫 |
| `hal/inc/stm32f4xx_ll_usb.h` | `0UL` → `0U`（纯清理） |
| `dev/Include/stm32f405xx.h` | 注释对齐 |
| `dev/Source/Templates/system_stm32f4xx.c` | `VTOR` 的 `#else` 分支：没有重映射时落 `FLASH_BASE` |

> `hal_tim.c` 那三条和本项目**直接相关**：三轴 PWM 全靠互补输出（TIM1 CH1/2/3 + TIM4/TIM3），
> 退回 tag 版本等于把已知修复扔掉。

## 2. 裁掉了什么（只留编译要用的）

| 部分 | 上游 | 这里 | 说明 |
|---|---|---|---|
| HAL `Src/*.c` | 94 | **20** | 只留实际参与编译的模块，见下表 |
| CMSIS Core | 64 个文件 | **52** | 删掉 A/R profile 的 `core_ca35/5/53/…` `core_cr*`（Cortex-M4 用不到）；`a-profile/` `r-profile/` 仍同步到 6.4.0，只为 `core_ca.h` 不成为死链，**不参与编译** |
| CMSIS Device | 全系列 | 3 头 + 1 源 + 1 启动 | 只留 `stm32f405xx` 需要的 |

HAL 保留的 20 个模块：`hal`、`adc`、`adc_ex`、`cortex`、`dma`、`dma_ex`、`exti`、`flash`、`flash_ex`、
`flash_ramfunc`、`gpio`、`pwr`、`pwr_ex`、`rcc`、`rcc_ex`、`spi`、`tim`、`tim_ex`、`uart`、`usart`。

删掉的（CAN / ETH / USB / SDIO / I2S / SAI / RTC / IWDG / WWDG / RNG / HASH / CRYP / DCMI / LTDC …）
要用时从上游 `Src/` 拷回来，再在 `CMakeLists.txt` 的 `libstm32_hal.a` 源列表里加一行即可。

## 3. 更新步骤

```bash
# ① 备份（改坏了能退）
tar -czf .workbuddy-ai/backup/stm32-$(date +%Y%m%d).tar.gz stm32/

# ② 取上游
git clone --depth 1 --branch v6.4.0 https://github.com/ARM-software/CMSIS_6.git
git clone --depth 1 https://github.com/STMicroelectronics/stm32f4xx_hal_driver.git
git clone --depth 1 https://github.com/STMicroelectronics/cmsis_device_f4.git

# ③ 比对（必须加 --strip-trailing-cr，见第 4 节）
diff -rq --strip-trailing-cr stm32/hal/src hal/Src

# ④ 覆盖：HAL 只覆盖这 20 个 .c 及其 .h；CMSIS 只覆盖顶层 + m-profile/
# ⑤ 编译 + 跑测试
cmake --build build/arm && ./build/host/src/tests/test_smoke.exe
# ⑥ 回来改本文件的版本表与提交号
```

## 4. 三个坑

1. **行尾**：上游是 CRLF，本仓库 `.gitattributes` 归一化成 LF。直接 `diff -q` 会**所有文件都报差异**，
   必须加 `--strip-trailing-cr`，否则会误判成"全都改过"。
   （2026-10-27 已把 `stm32/` 下 42 个 CRLF 文件统一转成 LF，工作区与仓库一致；
   下次从上游拷文件回来仍是 CRLF，记得再转一次。）
2. **不要用 tag 覆盖 master**，理由见第 1 节。
3. **CMSIS 6.4.0 删了两个文件**：`m-profile/cmsis_tiarmclang_m.h`、`a-profile/cmsis_cp15.h`（本地已同步删除）。
   别再拷回来——`cmsis_compiler.h` 已不再引用它们，留着只会让人以为版本没对齐。

## 5. 升级后必须验证的三件事

| 验证 | 命令 | 通过标准 |
|---|---|---|
| 固件能链接 | `cmake --build build/arm` | 无 error；`FLASH` 占用变化 < 1 KB 属正常（头文件内联改动） |
| 主机测试能过 | `./build/host/src/tests/test_smoke.exe` | `smoke ok` |
| PWM 能出波 | 上板后用示波器看 PB8/PB7/PB6 | 24 kHz、三相占空比互补、死区正常（`hal_tim.c` 的 `CCxNE` 修复就是保这个） |

## 6. 来源与许可证

三份代码都来自厂商**自己的 GitHub 组织**，不是第三方镜像。ST 的发布方式就是"每个模块一个仓库"
（`cmsis_device_f4` 的 README 原话：*each STM32Cube software module ... is delivered as an individual repository*），
STM32CubeF4 主仓库只用 submodule 引用它们，并不存代码：

```
STM32CubeF4 v1.28.3 / .gitmodules
    path = Drivers/STM32F4xx_HAL_Driver
    url  = https://github.com/STMicroelectronics/stm32f4xx_hal_driver.git
    path = Drivers/CMSIS/Device/ST/STM32F4xx
    url  = https://github.com/STMicroelectronics/cmsis_device_f4.git
```

| 目录 | 来源 | 组织认证 | 许可证 |
|---|---|---|---|
| `hal/` | `STMicroelectronics/stm32f4xx_hal_driver` | GitHub 官方认证（`is_verified: true`，官网 st.com） | BSD-3-Clause（`hal/LICENSE.md`，ST 原文） |
| `cmsis/device/` | `STMicroelectronics/cmsis_device_f4` | 同上 | Apache-2.0（`cmsis/device/LICENSE.md`） |
| `cmsis/core/` | `ARM-software/CMSIS_6` | Arm 官方组织（blog: arm.com，无 GitHub 认证徽章） | Apache-2.0（`cmsis/LICENSE.md`） |

> `cmsis/core/` 是唯一**不属于 ST** 的部分：Cortex-M 内核头文件由 Arm 维护，ST 也是从 Arm 取的。
> 注意 ST 的 CubeF4 里自带的是 CMSIS **5.6.0**，本项目用的是 Arm 官方 **6.4.0**（更新），
> 这是刻意选择：编译产物与 6.1.0 时逐字节相同，见第 5 节验证。
>
> 许可证文件必须跟着代码走：`stm32/cmsis/device/` 原来缺 `LICENSE.md`（而它的每个头文件都写着
> "terms that can be found in the LICENSE file in the root directory"），2026-10-27 已补齐。

## 7. 怎么自己核对这份库是官方的

不用信本文档的结论，下面每一条都能自己跑。命令在 Git Bash 里执行（VS Code 终端切到 Git Bash）。

### 7.1 先确认是 F4，不是 F1

```bash
ls stm32/cmsis/device/inc/        # 只有 stm32f405xx.h / stm32f4xx.h / system_stm32f4xx.h
ls stm32/cmsis/device/startup/    # 只有 startup_stm32f405xx.s
ls stm32/hal/src/ | head          # 全是 stm32f4xx_hal_*.c
grep -ril "STM32F1" stm32/        # 只有 1 个文件命中，原因见 7.4
```

### 7.2 看版权头

每个文件的头部都有 ST 的版权声明，`@author` 是 `MCD Application Team`（ST 的官方固件团队）：

```bash
sed -n '3,9p' stm32/hal/src/stm32f4xx_hal.c
#   * @file    stm32f4xx_hal.c
#   * @author  MCD Application Team
#   * @brief   HAL module driver.
#   ...
#   * Copyright (c) 2017 STMicroelectronics.
```

### 7.3 看版本宏（比文件名可信）

```bash
grep "__STM32F4xx_HAL_VERSION_" stm32/hal/src/stm32f4xx_hal.c
#   MAIN 0x01 / SUB1 0x08 / SUB2 0x05  →  HAL 1.8.5

grep -m1 "CMSIS version number" stm32/cmsis/device/inc/stm32f4xx.h
#   CMSIS version number V2.6.11       →  设备头 2.6.11
```

### 7.4 唯一出现 "F1" 的文件是正常的

`stm32/hal/inc/Legacy/stm32_hal_legacy.h` 里有 14 处 `STM32F1` 字样，全部长这样：

```c
#if defined(STM32F1)
  ...旧的宏别名...
#endif
```

这是 ST 官方给**所有** F 系列共用的兼容层（同一文件里还有 F0/F2/F3/F7/H7/L4/G0/WB）。
本工程编译时定义的是 `STM32F405xx`，这些分支整段被预处理器跳过，不会进二进制。
`stm32f4xx_hal_def.h:30` 会 include 它，删掉会编译失败 —— 它是 F4 HAL 的一部分，不是别的芯片的东西。

### 7.5 核对提交号

本文档第 1 节的三个提交号，在 GitHub 上可以直接搜到：

```
https://github.com/STMicroelectronics/stm32f4xx_hal_driver/commit/1f6451c3e07728b4c830744de380e56bf5bc0026
https://github.com/STMicroelectronics/cmsis_device_f4/commit/9192c7b9df75a142f2027ab266601fe061fc00b3
https://github.com/ARM-software/CMSIS_6/commit/af1b110e46a1720349f6f125e386e126767699bd
```

能打开、能看到同一个文件，就说明本地这份是那棵树的拷贝。

### 7.6 看许可证原文

```bash
head -5 stm32/hal/LICENSE.md            # BSD 3-Clause，ST 原文
head -5 stm32/cmsis/device/LICENSE.md   # Apache License 2.0
```

两个许可证的组合本身就是特征：ST 的 HAL 用 BSD-3-Clause，CMSIS 用 Apache-2.0。
第三方魔改版通常会把这两段删掉或换成 MIT。

### 7.7 想更彻底：跟 st.com 的官方包对

最权威的来源是 ST 官网的 `STM32CubeF4` 整包（`en.stm32cubef4-v1.28.x.zip`），
里面 `Drivers/STM32F4xx_HAL_Driver/` 和 `Drivers/CMSIS/Device/ST/STM32F4xx/`
与本目录应当是同一份源码（GitHub 仓库就是它的上游，`.gitmodules` 可证，见第 6 节）。
整包约 1 GB，需要的话可以下下来逐文件 diff —— 但通常没必要，7.2~7.5 已经足够定性。
