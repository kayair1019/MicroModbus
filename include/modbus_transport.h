#ifndef MODBUS_TRANSPORT_H
#define MODBUS_TRANSPORT_H

#include <stdint.h>
#include "modbus_core.h"

#ifdef __cplusplus
extern "C" {
#endif

// 平台相关发送函数：发送 data 中的 len 字节，返回实际发送的字节数，<0 表示错误
typedef int (*modbus_send_fn)(const uint8_t *data, uint16_t len, void *user_data);

// 平台相关接收函数：接收最多 max_len 字节到 buf，返回实际接收字节数，<0 表示错误
typedef int (*modbus_recv_fn)(uint8_t *buf, uint16_t max_len, void *user_data);

// 平台相关时间函数：返回单调递增的毫秒时间戳（用于超时）
typedef uint32_t (*modbus_time_fn)(void);

// 运输层公共配置
typedef struct {
    modbus_send_fn  send;
    modbus_recv_fn  recv;
    modbus_time_fn  time;
    void           *user_data;     // 用户自定义指针（如文件描述符）
    uint32_t        timeout_ms;    // 事务超时时间（毫秒），推荐 1000
    uint8_t         slave_addr;    // 默认从站地址（RTU 使用，TCP 为单元 ID）
} modbus_transport_t;

#ifdef __cplusplus
}
#endif

#endif // MODBUS_TRANSPORT_H