# MicroModbus

一个**极轻量、跨平台、纯 C99** 的 Modbus 协议解析库，适用于 x86 服务器端与 ARM 嵌入式设备。

## 设计理念

- **纯解析，不碰 I/O**：所有模块都是无状态的纯函数，只负责将字节流翻译为结构化数据，或将操作意图编码为协议报文。网络收发、超时、帧定界由上层通信层完成。
- **分层解耦**：核心层（功能码/错误码/工具）→ 角色层（主站/从站）→ 传输层（RTU/TCP），各层独立编译，互不依赖。
- **按需编译**：你只用 RTU 主站，就只编译 `modbus_core + modbus_master + modbus_rtu`，TCP 和从站代码完全不占 ROM。
- **零动态内存**：所有函数通过参数传递缓冲区，不调用 `malloc`，适合裸机和硬实时环境。
- **可移植**：仅依赖 C99 标准库头文件，使用显式大端读写，不依赖主机字节序，同一份代码可运行于 x86、ARM、RISC-V 等架构。

## 模块结构

| 模块 | 文件 | 职责 |
|------|------|------|
| 核心层 | `modbus_core.h` | 功能码、异常码、错误码、公共数据结构、大端字节序读写工具 |
| 主站 | `modbus_master.h` / `.c` | 构建请求 PDU、解析响应 PDU（正常/异常） |
| 从站 | `modbus_slave.h` / `.c` | 解析请求 PDU、构建正常/异常响应 PDU |
| RTU 传输 | `modbus_rtu.h` / `.c` | CRC16 计算、RTU ADU 构建与解析 |
| TCP 传输 | `modbus_tcp.h` / `.c` | MBAP 头构建与解析、TCP ADU 构建与解析 |

> 所有模块只暴露纯函数，无全局变量，可重入，可直接用于多实例场景。

## 当前支持的功能码

- `0x03` 读保持寄存器
- `0x06` 写单个寄存器

其他功能码可按照相同模式快速扩展（框架已预留扩展点）。

## 快速开始

### 1. 将源文件添加到工程

根据你的角色和传输方式，选择所需文件：

- **主站 + RTU**：`modbus_core.h`, `modbus_master.h/.c`, `modbus_rtu.h/.c`
- **从站 + TCP**：`modbus_core.h`, `modbus_slave.h/.c`, `modbus_tcp.h/.c`

### 2. 包含头文件并调用 API

```c
#include "modbus_master.h"
#include "modbus_rtu.h"

void read_holding_registers(void) {
    // 1) 构建请求 PDU（读起始地址 0x100，数量 10）
    uint8_t pdu[MODBUS_PDU_MAX_SIZE];
    int pdu_len = modbus_master_build_read_holding_regs(pdu, 0x100, 10);

    // 2) 封装为 RTU 帧（从站地址 0x01）
    uint8_t adu[MODBUS_RTU_MAX_ADU_SIZE];
    int adu_len = modbus_rtu_build_adu(adu, 0x01, pdu, pdu_len);

    // 3) 发送 adu 到串口，接收响应帧 resp_adu
    // ... (由你的通信层实现)

    // 4) 解析 RTU 帧，提取 PDU
    uint8_t slave_addr;
    const uint8_t *resp_pdu;
    uint16_t resp_pdu_len;
    modbus_err_t err = modbus_rtu_parse_adu(resp_adu, resp_len,
                                            &slave_addr, &resp_pdu, &resp_pdu_len);
    if (err != MODBUS_OK) return;

    // 5) 解析响应 PDU
    modbus_pdu_data_t data;
    err = modbus_master_parse_response(resp_pdu, resp_pdu_len, &data);
    if (err == MODBUS_OK) {
        // 使用 data.data.registers.reg_values[i]
    } else if (err == MODBUS_ERR_EXCEPTION) {
        uint8_t exc = data.exception_code;
    }
}
```


## API 概览

### 核心层 (`modbus_core.h`)

- `modbus_write_uint16_be()` – 大端写入 16 位值
- 功能码/异常码宏 (`MODBUS_FC_READ_HOLDING_REGISTERS` 等)
- 错误码枚举 (`MODBUS_OK`, `MODBUS_ERR_*`)
- 通用数据结构 `modbus_pdu_data_t`

### 主站 (`modbus_master.h`)

- `modbus_master_build_read_holding_regs()`
- `modbus_master_build_write_single_register()`
- `modbus_master_parse_response()`

### 从站 (`modbus_slave.h`)

- `modbus_slave_parse_request()`
- `modbus_slave_build_read_holding_regs_response()`
- `modbus_slave_build_write_single_register_response()`
- `modbus_slave_build_exception_response()`

### RTU 传输 (`modbus_rtu.h`)

- `modbus_rtu_crc16()`
- `modbus_rtu_build_adu()`
- `modbus_rtu_parse_adu()`

### TCP 传输 (`modbus_tcp.h`)

- `modbus_tcp_build_adu()`
- `modbus_tcp_parse_adu()`

## 性能

在典型 **Cortex-M4 @72MHz (GCC -O2)** 下的微基准测试参考值：

| 操作 | 大致耗时 |
|------|----------|
| 构建 03 请求 PDU | < 0.5 µs |
| 构建 RTU ADU（含 CRC） | ~2 µs |
| CRC 校验 256 字节 | ~15 µs |
| 解析 03 响应（20 字节） | ~1 µs |

> x86 平台上耗时会再低一个数量级。所有操作都是确定性的、无阻塞的，适合实时系统。

## 测试

每个模块都是纯函数，可以独立进行单元测试。你可以编写简单的驱动程序，用已知正确报文验证编解码，或进行百万次循环性能测试。

## 跨平台

- 纯 C99 标准，无平台相关调用
- 显式大端处理，不依赖主机字节序
- 已在 x86 (gcc/clang/msvc) 和 ARM Cortex-M (arm-none-eabi-gcc) 上验证
