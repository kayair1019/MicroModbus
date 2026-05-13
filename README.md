# MicroModbus

MicroModbus是一个轻量级的 Modbus 客户端库，专为资源受限的嵌入式系统设计，同时完整支持 RTU 和 TCP 两种传输模式。只依赖标准c库，能适配各种不同架构（arm/x86），只需编写平台抽象层（实现read和write函数）。


使用方法：

你需要提供两个底层函数：read 和 write，原型如下：
```c
int32_t my_read(uint8_t* buf, uint16_t count, int32_t byte_timeout_ms, void* arg);
int32_t my_write(const uint8_t* buf, uint16_t count, int32_t byte_timeout_ms, void* arg);
```
返回值：实际读/写的字节数，错误时返回 < 0。


创建客户端实例
```c
#include "micromodbus.h"

mmbs_t ctx;
mmbs_platform_conf conf;

// 初始化平台配置（必须）
mmbs_platform_conf_create(&conf);
conf.transport = MMBS_TRANSPORT_RTU;   // 或 MMBS_TRANSPORT_TCP
conf.read = my_read;
conf.write = my_write;
conf.arg = &my_serial_handle;          // 自定义参数，会传给 read/write

// 创建客户端
if (mmbs_client_create(&ctx, &conf) != MMBS_ERROR_NONE) {
    // 错误处理
}
```

配置超时
```c
mmbs_set_read_timeout(&ctx, 1000);   // 响应超时 1000ms
mmbs_set_byte_timeout(&ctx, 50);     // 字节间超时 50ms（-1 表示无限）
```

调用 Modbus 功能

示例：读取保持寄存器
```c
uint16_t regs[10];
mmbs_error err = mmbs_read_holding_registers(&ctx, 0x100, 10, regs);
if (err == MMBS_ERROR_NONE) {
    // 成功，regs[0]~regs[9] 包含读取的值
} else {
    printf("Error: %s\n", mmbs_strerror(err));
}
```

6. 错误处理
错误码定义在 mmbs_error 枚举中：
负值：库内部错误（如超时、CRC 校验失败、无效参数等）
正值：Modbus 异常（非法功能、非法数据地址等）
0  ：正常返回

```c
if (err < 0) {
    // 传输层或协议错误
} else if (err > 0) {
    // 从机返回的 Modbus 异常
    printf("Slave exception: %s\n", mmbs_strerror(err));
}
```
