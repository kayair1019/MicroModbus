# 测试模块
add_executable(test_micromodbus
    test_micromodbus.c
)

target_link_libraries(test_micromodbus PRIVATE micromodbus)

# 让测试程序可以模拟平台函数（暂时不强制）
target_compile_definitions(test_micromodbus PRIVATE -DMMBS_TEST)

# 添加 CTest 测试
add_test(NAME MicromodbusBasicTest COMMAND test_micromodbus)
