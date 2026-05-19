#ifndef MODBUS_TCP_TRANSPORT_H
#define MODBUS_TCP_TRANSPORT_H

#include "modbus_transport.h"
#include "modbus_tcp.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    modbus_transport_t base;

    // 内部缓冲（用于拼接 ADU）
    uint8_t  rx_buf[MODBUS_TCP_MAX_ADU_SIZE];
    uint16_t rx_len;
    uint16_t expected_len; // 期望的完整 ADU 长度（根据 MBAP 长度字段算出）
    bool     header_parsed;
} modbus_tcp_transport_t;

void modbus_tcp_transport_init(modbus_tcp_transport_t *ctx,
                               modbus_send_fn send,
                               modbus_recv_fn recv,
                               modbus_time_fn time_fn,
                               void *user_data);

// 主站事务：发送请求 PDU，返回响应 PDU
// transaction_id 由调用者管理（调用者保证唯一性）
modbus_err_t modbus_tcp_master_transact(modbus_tcp_transport_t *ctx,
                                        uint16_t transaction_id,
                                        const uint8_t *request_pdu,
                                        uint16_t request_pdu_len,
                                        uint8_t *response_pdu,
                                        uint16_t *response_pdu_len);

#ifdef __cplusplus
}
#endif

#endif // MODBUS_TCP_TRANSPORT_H