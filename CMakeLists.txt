cmake_minimum_required(VERSION 3.14)
project(MicroModbus C)

# 可选：设置 C 标准
set(CMAKE_C_STANDARD 99)

# 库目标
add_library(micromodbus
    src/modbus_master.c
    src/modbus_rtu.c
    src/modbus_tcp.c
    src/modbus_rtu_transport.c
    src/modbus_tcp_transport.c
    # 从站等按需添加
)
target_include_directories(micromodbus PUBLIC include)

# 启用警告
target_compile_options(micromodbus PRIVATE -Wall -Wextra -Wpedantic)

# 如果有测试
option(BUILD_TESTS "Build tests" OFF)
if(BUILD_TESTS)
    add_subdirectory(tests)
endif()

# 示例
option(BUILD_EXAMPLES "Build examples" OFF)
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()