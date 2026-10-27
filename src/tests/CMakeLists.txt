# PC 端单元测试（架构文档 §14 第 4 条：algorithm/ 与 control/ 不得依赖 HAL）
#
# 只把 common / algorithm / control 三个目录加入头文件搜索路径。
# stm32/hal/inc 与 stm32/cmsis 故意不在路径上 —— 任何一处
# #include "stm32f4xx_hal.h" 都会在这里编译失败，这正是这条约束的执行方式。

set(TEST_INCLUDES
    ${CMAKE_SOURCE_DIR}/src/common        # types.h / config.h / math_utils.h（纯类型与纯数学）
    ${CMAKE_SOURCE_DIR}/src/algorithm
    ${CMAKE_SOURCE_DIR}/src/control
)

# 被测源码逐个列出，不用 file(GLOB)。
# 理由和固件那边一样：将来往 algorithm/ 里加一个带 HAL 依赖的文件时，
# 必须有人显式把它写进这张表，否则它根本不会被测试到 —— 沉默的漏测比编译错误危险。
set(TEST_SOURCES
    ${CMAKE_SOURCE_DIR}/src/common/math_utils.c
    ${CMAKE_SOURCE_DIR}/src/algorithm/filter.c
    ${CMAKE_SOURCE_DIR}/src/algorithm/hall.c
    ${CMAKE_SOURCE_DIR}/src/algorithm/svpwm.c
    ${CMAKE_SOURCE_DIR}/src/control/pid.c
)

function(gimbal_add_test name)
    add_executable(${name} ${name}.c ${TEST_SOURCES})

    target_include_directories(${name} PRIVATE ${TEST_INCLUDES})

    target_compile_options(${name} PRIVATE
        $<$<C_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra -Wdouble-promotion>
        $<$<C_COMPILER_ID:MSVC>:/W4 /utf-8>   # /utf-8：源码是 UTF-8，否则中文注释触发 C4819
    )

    if(NOT MSVC)
        target_link_libraries(${name} PRIVATE m)   # expf / atan2f / sqrtf
    endif()

    add_test(NAME ${name} COMMAND ${name})
endfunction()

gimbal_add_test(test_smoke)       # 构建链与单精度浮点行为
gimbal_add_test(test_algorithm)   # math_utils / filter / hall / svpwm
gimbal_add_test(test_control)     # pid
