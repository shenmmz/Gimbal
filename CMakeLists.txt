# 云台固件 —— 顶层构建
#
# 两个目标，靠是否交叉编译区分：
#   arm  preset  → build/arm   STM32F405 固件（elf/bin/hex/map）
#   host preset  → build/host  PC 端单元测试（algorithm/ 与 control/ 可脱离硬件运行）
#
# 常用命令：
#   cmake --preset arm  && cmake --build --preset arm
#   cmake --preset host && cmake --build --preset host && ctest --preset host

cmake_minimum_required(VERSION 3.20)
project(gimbal LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
# 保留 gnu11：HAL 与 CMSIS 依赖 __attribute__、内联汇编等 GNU 扩展

if(NOT CMAKE_CROSSCOMPILING)
    message(STATUS "构建 PC 端单元测试（未指定交叉工具链）")
    enable_testing()
    add_subdirectory(src/tests)
    return()
endif()

message(STATUS "构建 STM32F405 固件")

enable_language(ASM)   # 只有固件需要汇编（startup 文件），主机端不需要

set(VENDOR "${CMAKE_CURRENT_SOURCE_DIR}/stm32")
set(SRC    "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(LD_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/ld/STM32F405RGTx_FLASH.ld")

set(CMAKE_EXECUTABLE_SUFFIX ".elf")

# ---------- ST 官方 HAL + CMSIS（原样引入，禁止修改） ----------
add_library(stm32_hal STATIC
    ${VENDOR}/cmsis/device/src/system_stm32f4xx.c
    ${VENDOR}/hal/src/stm32f4xx_hal.c
    ${VENDOR}/hal/src/stm32f4xx_hal_adc.c
    ${VENDOR}/hal/src/stm32f4xx_hal_adc_ex.c
    ${VENDOR}/hal/src/stm32f4xx_hal_cortex.c
    ${VENDOR}/hal/src/stm32f4xx_hal_dma.c
    ${VENDOR}/hal/src/stm32f4xx_hal_dma_ex.c
    ${VENDOR}/hal/src/stm32f4xx_hal_exti.c
    ${VENDOR}/hal/src/stm32f4xx_hal_flash.c
    ${VENDOR}/hal/src/stm32f4xx_hal_flash_ex.c
    ${VENDOR}/hal/src/stm32f4xx_hal_flash_ramfunc.c
    ${VENDOR}/hal/src/stm32f4xx_hal_gpio.c
    ${VENDOR}/hal/src/stm32f4xx_hal_pwr.c
    ${VENDOR}/hal/src/stm32f4xx_hal_pwr_ex.c
    ${VENDOR}/hal/src/stm32f4xx_hal_rcc.c
    ${VENDOR}/hal/src/stm32f4xx_hal_rcc_ex.c
    ${VENDOR}/hal/src/stm32f4xx_hal_spi.c
    ${VENDOR}/hal/src/stm32f4xx_hal_tim.c
    ${VENDOR}/hal/src/stm32f4xx_hal_tim_ex.c
    ${VENDOR}/hal/src/stm32f4xx_hal_uart.c
    ${VENDOR}/hal/src/stm32f4xx_hal_usart.c
)

target_include_directories(stm32_hal PUBLIC
    ${VENDOR}/cmsis/core
    ${VENDOR}/cmsis/device/inc
    ${VENDOR}/hal/inc
    ${SRC}/common          # hal_conf.h（HAL 通过 shim 找到它）
)

# stm32/ 里的官方代码不参与 -Werror，只给最小必要宏
target_compile_definitions(stm32_hal PUBLIC
    STM32F405xx
    USE_HAL_DRIVER
)

# ---------- 固件 ----------
add_executable(gimbal
    ${VENDOR}/cmsis/device/startup/startup_stm32f405xx.s
    ${SRC}/app/main.c
    ${SRC}/driver/uart.c
    ${SRC}/driver/gpio.c
    ${SRC}/driver/pwm.c
    ${SRC}/driver/adc.c
    ${SRC}/driver/timebase.c
    # 算法层与控制层：目前 main.c 还没调用它们，会被 --gc-sections 整体丢掉。
    # 仍然列进来，是为了让它们始终以 ARM 目标 + -Wdouble-promotion 编译一次 ——
    # 只在 host 上编过的代码，到板子上才发现用了 double 就晚了。
    ${SRC}/common/math_utils.c
    ${SRC}/algorithm/filter.c
    ${SRC}/algorithm/hall.c
    ${SRC}/algorithm/svpwm.c
    ${SRC}/control/pid.c
)

target_include_directories(gimbal PRIVATE
    ${SRC}/app
    ${SRC}/driver
    ${SRC}/common
    ${SRC}/control
    ${SRC}/algorithm
)

target_link_libraries(gimbal PRIVATE stm32_hal)

# 只对本工程代码开严格告警；-Wdouble-promotion 专治本项目最常见的 double 陷阱
target_compile_options(gimbal PRIVATE
    $<$<COMPILE_LANGUAGE:C>:-Wall -Wextra -Wshadow -Wdouble-promotion>
)

target_link_options(gimbal PRIVATE
    -T${LD_SCRIPT}
    --specs=nano.specs
    --specs=nosys.specs
    -u _printf_float          # printf 支持 %f，否则静默输出乱码
    -Wl,--gc-sections
    -Wl,--no-warn-rwx-segments  # 单段裸机镜像必然 RWX，没有 MPU 时该告警无意义
    -Wl,--print-memory-usage
    -Wl,-Map=$<TARGET_FILE_DIR:gimbal>/gimbal.map
)

add_custom_command(TARGET gimbal POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:gimbal> $<TARGET_FILE_DIR:gimbal>/gimbal.bin
    COMMAND ${CMAKE_OBJCOPY} -O ihex   $<TARGET_FILE:gimbal> $<TARGET_FILE_DIR:gimbal>/gimbal.hex
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:gimbal>
    # ninja 会把 COMMENT 原样按 UTF-8 字节回显，中文在 GBK 控制台必乱码，这里只用 ASCII
    COMMENT "objcopy -> gimbal.bin / gimbal.hex, then print size"
)
