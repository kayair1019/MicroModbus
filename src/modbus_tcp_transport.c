#include "modbus_tcp_transport.h"
#include <string.h>

void modbus_tcp_transport_init(modbus_tcp_transport_t *ctx,
                               modbus_send_fn send,
                               modbus_recv_fn recv,
                               modbus_time_fn time_fn,
                               void *user_data)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->base.send      = send;
    ctx->base.recv      = recv;
    ctx->base.time      = time_fn;
    ctx->base.user_data = user_data;
    ctx->base.timeout_ms = 1000;
    ctx->base.slave_addr = 1; // TCP 作为单元 ID
}

static modbus_err_t wait_for_tcp_frame(modbus_tcp_transport_t *ctx)
{
    uint32_t start_time = ctx->base.time();
    ctx->rx_len = 0;
    ctx->expected_len = 0;
    ctx->header_parsed = false;

    while (1) {
        // 尝试读取更多字节
        uint16_t to_read = MODBUS_TCP_MAX_ADU_SIZE - ctx->rx_len;
        if (to_read > 0) {
            int n = ctx->base.recv(ctx->rx_buf + ctx->rx_len, to_read, ctx->base.user_data);
            if (n > 0) {
                ctx->rx_len += n;
            }
        }

        // 解析 MBAP 头获取长度
        if (!ctx->header_parsed && ctx->rx_len >= MODBUS_TCP_MBAP_HEADER_SIZE) {
            uint16_t length = ((uint16_t)ctx->rx_buf[4] << 8) | ctx->rx_buf[5];
            if (length < 1 || length > (MODBUS_PDU_MAX_SIZE + 1)) {
                return MODBUS_ERR_PDU_TOO_SHORT; // 非法长度
            }
            ctx->expected_len = MODBUS_TCP_MBAP_HEADER_SIZE + length - 1;
            ctx->header_parsed = true;
        }

        // 是否已收全完整 ADU？
        if (ctx->header_parsed && ctx->rx_len >= ctx->expected_len) {
            return MODBUS_OK;
        }

        // 超时检查
        if ((ctx->base.time() - start_time) >= ctx->base.timeout_ms) {
            return MODBUS_ERR_EXCEPTION; // 超时
        }
    }
}

modbus_err_t modbus_tcp_master_transact(modbus_tcp_transport_t *ctx,
                                        uint16_t transaction_id,
                                        const uint8_t *request_pdu,
                                        uint16_t request_pdu_len,
                                        uint8_t *response_pdu,
                                        uint16_t *response_pdu_len)
{
    if (!ctx || !request_pdu || !response_pdu || !response_pdu_len)
        return MODBUS_ERR_NULL_PTR;

    // 1. 构建 TCP ADU
    uint8_t adu[MODBUS_TCP_MAX_ADU_SIZE];
    int adu_len = modbus_tcp_build_adu(adu, transaction_id,
                                       ctx->base.slave_addr,
                                       request_pdu, request_pdu_len);
    if (adu_len < 0) return -MODBUS_ERR_DATA_TOO_LONG;

    // 2. 发送
    int sent = ctx->base.send(adu, adu_len, ctx->base.user_data);
    if (sent != adu_len) return MODBUS_ERR_EXCEPTION;

    // 3. 接收完整 ADU
    modbus_err_t err = wait_for_tcp_frame(ctx);
    if (err != MODBUS_OK) return err;

    // 4. 解析 ADU，提取 PDU
    uint16_t recv_trans_id;
    uint8_t  recv_unit_id;
    const uint8_t *pdu;
    uint16_t pdu_len;
    err = modbus_tcp_parse_adu(ctx->rx_buf, ctx->rx_len,
                               &recv_trans_id, &recv_unit_id,
                               &pdu, &pdu_len);
    if (err != MODBUS_OK) return err;

    // 可选：校验事务 ID
    // if (recv_trans_id != transaction_id) return MODBUS_ERR_MISMATCHED_FC;

    // 5. 复制 PDU
    if (pdu_len > MODBUS_PDU_MAX_SIZE) return MODBUS_ERR_PDU_TOO_SHORT;
    memcpy(response_pdu, pdu, pdu_len);
    *response_pdu_len = pdu_len;

    return MODBUS_OK;
}