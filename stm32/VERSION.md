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
2. **不要用 tag 覆盖 master**，理由见第 1 节。
3. **CMSIS 6.4.0 删了两个文件**：`m-profile/cmsis_tiarmclang_m.h`、`a-profile/cmsis_cp15.h`（本地已同步删除）。
   本地若还留着不会报错，但别再从别处拷回来（`cmsis_compiler.h` 已经不再引用它们）。

## 5. 升级后必须验证的三件事

| 验证 | 命令 | 通过标准 |
|---|---|---|
| 固件能链接 | `cmake --build build/arm` | 无 error；`FLASH` 占用变化 < 1 KB 属正常（头文件内联改动） |
| 主机测试能过 | `./build/host/src/tests/test_smoke.exe` | `smoke ok` |
| PWM 能出波 | 上板后用示波器看 PB8/PB7/PB6 | 24 kHz、三相占空比互补、死区正常（`hal_tim.c` 的 `CCxNE` 修复就是保这个） |
