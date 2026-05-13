# microMODBUS - 轻量级 Modbus 客户端库
cmake_minimum_required(VERSION 3.10)
project(micromodbus VERSION 1.0.0 LANGUAGES C)

# 选项：是否构建测试
option(BUILD_TESTING "Build unit tests" OFF)

# 选项：启用调试输出（会定义 MMBS_DEBUG 宏）
option(MMBS_DEBUG "Enable debug output (prints via printf)" OFF)

# 设置库的公共头文件目录
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 创建静态库
add_library(micromodbus STATIC
    micromodbus.c
)

# 添加头文件包含路径（当前目录）
target_include_directories(micromodbus PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# 设置库属性
set_target_properties(micromodbus PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
)

# 处理调试选项
if(MMBS_DEBUG)
    target_compile_definitions(micromodbus PRIVATE MMBS_DEBUG)
    # 调试模式下建议不优化，便于阅读日志
    target_compile_options(micromodbus PRIVATE -O0 -g)
endif()

# 可选：安装规则
install(TARGETS micromodbus
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)
install(FILES micromodbus.h DESTINATION include)

# 测试子目录
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()
