# 交叉编译工具链：Arm GNU Toolchain (arm-none-eabi)
# 用法：cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-arm-none-eabi.cmake ...
# 或直接使用 CMakePresets.json 里的 arm preset

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR cortex-m4)

set(TOOLCHAIN_PREFIX "arm-none-eabi-" CACHE STRING "工具链可执行文件前缀")

find_program(ARM_GCC "${TOOLCHAIN_PREFIX}gcc")
if(NOT ARM_GCC)
  message(FATAL_ERROR
    "找不到 ${TOOLCHAIN_PREFIX}gcc。\n"
    "请安装 Arm GNU Toolchain 并把 bin 目录加入 PATH，\n"
    "或传 -DTOOLCHAIN_PREFIX=<绝对路径>/bin/arm-none-eabi- 指定。")
endif()

set(CMAKE_C_COMPILER   "${TOOLCHAIN_PREFIX}gcc")
set(CMAKE_ASM_COMPILER "${TOOLCHAIN_PREFIX}gcc")
set(CMAKE_OBJCOPY      "${TOOLCHAIN_PREFIX}objcopy")
set(CMAKE_SIZE         "${TOOLCHAIN_PREFIX}size")

# 交叉编译出的可执行文件无法在本机运行，编译器探测改用静态库
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# STM32F405RGT6 = Cortex-M4F，FPv4-SP 只有单精度
set(MCU_FLAGS "-mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard")

set(CMAKE_C_FLAGS_INIT   "${MCU_FLAGS} -ffunction-sections -fdata-sections -fno-common")
set(CMAKE_ASM_FLAGS_INIT "${MCU_FLAGS} -x assembler-with-cpp")
set(CMAKE_EXE_LINKER_FLAGS_INIT "${MCU_FLAGS}")
