enable_testing()

# 解析层测试
add_executable(test_parse test_parse.c)
target_link_libraries(test_parse micromodbus)
add_test(NAME parse COMMAND test_parse)

# RTU 运输层测试
add_executable(test_rtu_transport test_transport_rtu.c)
target_link_libraries(test_rtu_transport micromodbus)
add_test(NAME rtu_transport COMMAND test_rtu_transport)