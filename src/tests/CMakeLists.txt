# PC 端单元测试（架构文档 §14.4：algorithm/ 与 control/ 不得依赖 HAL）
# 只把这两个目录加入头文件搜索路径，任何一处 #include "stm32f4xx_hal.h" 都会在这里暴露

add_executable(test_smoke test_smoke.c)

target_include_directories(test_smoke PRIVATE
    ${CMAKE_SOURCE_DIR}/src/algorithm
    ${CMAKE_SOURCE_DIR}/src/control
)

target_compile_options(test_smoke PRIVATE
    $<$<C_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra -Wdouble-promotion>
    $<$<C_COMPILER_ID:MSVC>:/W4 /utf-8>   # /utf-8：源码是 UTF-8，否则中文注释触发 C4819
)

if(NOT MSVC)
    target_link_libraries(test_smoke PRIVATE m)
endif()

add_test(NAME smoke COMMAND test_smoke)
