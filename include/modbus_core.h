#ifndef MODBUS_CORE_H
#define MODBUS_CORE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// 标准 Modbus 功能码
#define MODBUS_FC_READ_COILS                0x01
#define MODBUS_FC_READ_DISCRETE_INPUTS      0x02
#define MODBUS_FC_READ_HOLDING_REGISTERS    0x03
#define MODBUS_FC_READ_INPUT_REGISTERS      0x04
#define MODBUS_FC_WRITE_SINGLE_COIL         0x05
#define MODBUS_FC_WRITE_SINGLE_REGISTER     0x06
#define MODBUS_FC_READ_EXCEPTION_STATUS     0x07
#define MODBUS_FC_WRITE_MULTIPLE_COILS      0x0F
#define MODBUS_FC_WRITE_MULTIPLE_REGISTERS  0x10
#define MODBUS_FC_READ_WRITE_MULTIPLE_REGS  0x17

// 标准 Modbus 异常码
#define MODBUS_EXCEPTION_ILLEGAL_FUNCTION           0x01
#define MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS       0x02
#define MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE         0x03
#define MODBUS_EXCEPTION_SLAVE_DEVICE_FAILURE       0x04
#define MODBUS_EXCEPTION_ACKNOWLEDGE                0x05
#define MODBUS_EXCEPTION_SLAVE_DEVICE_BUSY          0x06
#define MODBUS_EXCEPTION_MEMORY_PARITY_ERROR        0x08
#define MODBUS_EXCEPTION_GATEWAY_PATH_UNAVAILABLE   0x0A
#define MODBUS_EXCEPTION_GATEWAY_TARGET_NO_RESP     0x0B

// PDU 最大长度 (RTU 模式 253 字节)
#define MODBUS_PDU_MAX_SIZE     253

// 模块内部错误码 (主站/从站共用)
typedef enum {
    MODBUS_OK = 0,
    MODBUS_ERR_NULL_PTR,
    MODBUS_ERR_INVALID_ADDR,        // 起始地址超出范围
    MODBUS_ERR_INVALID_COUNT,       // 数量超出范围
    MODBUS_ERR_DATA_TOO_LONG,       // 数据过长
    MODBUS_ERR_PDU_TOO_SHORT,       // 收到的 PDU 长度不足
    MODBUS_ERR_MISMATCHED_FC,       // 响应的功能码与请求不匹配
    MODBUS_ERR_UNSUPPORTED_FC,      // 不支持的功能码
    MODBUS_ERR_EXCEPTION            // 设备返回异常 (具体异常码需另行获取)
} modbus_err_t;


// 解析请求后得到的数据视图（主从站都可用）
typedef struct {
    uint8_t  function_code;
    bool     is_exception;
    uint8_t  exception_code;
    uint16_t byte_count;
    union {
        struct {
            const uint16_t *reg_values;
            uint16_t        count;
        } registers;
        struct {
            uint16_t address;
            uint16_t value;
        } single;
    } data;
} modbus_pdu_data_t;

// 工具函数：将 16 位值以大端序写入缓冲区 (Modbus 专用)
static inline void modbus_write_uint16_be(uint8_t *buf, uint16_t val)
{
    buf[0] = (uint8_t)(val >> 8);
    buf[1] = (uint8_t)(val & 0xFF);
}

#ifdef __cplusplus
}
#endif

#endif // MODBUS_CORE_H