#ifndef MODBUS_RTU_TRANSPORT_H
#define MODBUS_RTU_TRANSPORT_H

#include "modbus_transport.h"
#include "modbus_rtu.h"

#ifdef __cplusplus
extern "C" {
#endif

// RTU 运输层上下文
typedef struct {
    modbus_transport_t base;        // 公共部分

    // 内部状态（用户不应直接修改）
    uint8_t  rx_buf[MODBUS_RTU_MAX_ADU_SIZE];
    uint16_t rx_len;                // 当前已缓冲字节数
    uint32_t last_byte_time_ms;     // 上次收到字节的时间戳
    uint32_t frame_timeout_ms;      // 帧结束超时（3.5 字符时间，毫秒）
    int      baud_rate;             // 波特率，用于计算帧间隔
} modbus_rtu_transport_t;

// 初始化 RTU 运输层
// baud_rate: 实际通信波特率，用于自动计算 3.5 字符间隔（若为0则使用默认值：3.5ms）
void modbus_rtu_transport_init(modbus_rtu_transport_t *ctx,
                               modbus_send_fn send,
                               modbus_recv_fn recv,
                               modbus_time_fn time_fn,
                               void *user_data,
                               int baud_rate);

// 主站事务：发送请求 PDU，等待并解析响应 PDU
// request_pdu     : 输入，由 modbus_master_xxx 构建的请求 PDU
// request_pdu_len : 请求 PDU 字节数
// response_pdu    : 输出，接收到的响应 PDU（缓冲区由调用者提供，至少 MODBUS_PDU_MAX_SIZE 字节）
// response_pdu_len: 输出，响应 PDU 的实际长度
// 返回 MODBUS_OK 表示成功，其他错误码表示失败
modbus_err_t modbus_rtu_master_transact(modbus_rtu_transport_t *ctx,
                                        const uint8_t *request_pdu,
                                        uint16_t request_pdu_len,
                                        uint8_t *response_pdu,
                                        uint16_t *response_pdu_len);

#ifdef __cplusplus
}
#endif

#endif // MODBUS_RTU_TRANSPORT_H