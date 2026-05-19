#include "modbus_rtu_transport.h"
#include <string.h>

// 计算 3.5 字符时间对应的毫秒数（8数据位 + 1停止位 + 1起始位 = 11bit/字符）
// 如果波特率为 0，返回默认值 4ms
static uint32_t calculate_frame_timeout_ms(int baud_rate)
{
    if (baud_rate <= 0) return 4; // 默认值
    // 3.5 字符 * 11 bit/字符 / baud_rate * 1000 ms/s，至少 1ms
    uint32_t t = (35 * 1000) / (baud_rate / 100); // 避免浮点：3.5*11*1000 / baud = 38500/baud
    // 更精确：38500 / baud
    t = 38500 / baud_rate;
    if (t < 1) t = 1;
    return t;
}

void modbus_rtu_transport_init(modbus_rtu_transport_t *ctx,
                               modbus_send_fn send,
                               modbus_recv_fn recv,
                               modbus_time_fn time_fn,
                               void *user_data,
                               int baud_rate)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->base.send      = send;
    ctx->base.recv      = recv;
    ctx->base.time      = time_fn;
    ctx->base.user_data = user_data;
    ctx->base.timeout_ms = 1000;   // 默认事务超时 1s
    ctx->base.slave_addr = 1;      // 默认从站地址
    ctx->baud_rate       = baud_rate;
    ctx->frame_timeout_ms = calculate_frame_timeout_ms(baud_rate);
}

// 内部：从串口读数据并追加到 rx_buf，返回当前缓冲长度
static uint16_t read_to_buffer(modbus_rtu_transport_t *ctx)
{
    if (ctx->rx_len >= MODBUS_RTU_MAX_ADU_SIZE) {
        return ctx->rx_len; // 缓冲区满
    }
    uint16_t remaining = MODBUS_RTU_MAX_ADU_SIZE - ctx->rx_len;
    int n = ctx->base.recv(ctx->rx_buf + ctx->rx_len, remaining, ctx->base.user_data);
    if (n > 0) {
        ctx->rx_len += n;
        ctx->last_byte_time_ms = ctx->base.time();
    }
    return ctx->rx_len;
}

// 内部：等待一个完整 RTU 帧到达，返回完整帧数据的指针和长度（仍在 rx_buf 中）
// 若超时，返回 NULL
static const uint8_t* wait_for_frame(modbus_rtu_transport_t *ctx, uint16_t *out_len)
{
    uint32_t start_time = ctx->base.time();

    while (1) {
        uint16_t len_before = ctx->rx_len;
        read_to_buffer(ctx);

        // 如果收到了新字节，更新帧结束检测时间
        if (ctx->rx_len > 0) {
            uint32_t now = ctx->base.time();
            uint32_t idle_time = now - ctx->last_byte_time_ms;

            // 帧结束条件：空闲时间超过帧间隔，且至少收到了最小帧（4字节）
            if (idle_time >= ctx->frame_timeout_ms && ctx->rx_len >= 4) {
                *out_len = ctx->rx_len;
                return ctx->rx_buf;
            }
        }

        // 检查事务超时
        if ((ctx->base.time() - start_time) >= ctx->base.timeout_ms) {
            return NULL; // 超时
        }
    }
}

modbus_err_t modbus_rtu_master_transact(modbus_rtu_transport_t *ctx,
                                        const uint8_t *request_pdu,
                                        uint16_t request_pdu_len,
                                        uint8_t *response_pdu,
                                        uint16_t *response_pdu_len)
{
    if (!ctx || !request_pdu || !response_pdu || !response_pdu_len)
        return MODBUS_ERR_NULL_PTR;

    // 1. 构建 ADU
    uint8_t adu[MODBUS_RTU_MAX_ADU_SIZE];
    int adu_len = modbus_rtu_build_adu(adu, ctx->base.slave_addr, request_pdu, request_pdu_len);
    if (adu_len < 0) return -MODBUS_ERR_DATA_TOO_LONG;

    // 2. 清空接收缓冲区（之前可能遗留数据）
    ctx->rx_len = 0;
    ctx->last_byte_time_ms = 0;

    // 3. 发送
    int sent = ctx->base.send(adu, adu_len, ctx->base.user_data);
    if (sent != adu_len) return MODBUS_ERR_EXCEPTION; // 可增加发送失败错误码

    // 4. 等待响应帧
    uint16_t frame_len;
    const uint8_t *frame = wait_for_frame(ctx, &frame_len);
    if (!frame) {
        // 超时
        return MODBUS_ERR_EXCEPTION; // 可扩展 MODBUS_ERR_TIMEOUT
    }

    // 5. 解析 ADU，提取 PDU
    uint8_t slave_addr;
    const uint8_t *pdu;
    uint16_t pdu_len;
    modbus_err_t err = modbus_rtu_parse_adu(frame, frame_len,
                                            &slave_addr, &pdu, &pdu_len);
    if (err != MODBUS_OK) return err;

    // 6. 可选：检查从站地址是否匹配
    if (slave_addr != ctx->base.slave_addr) {
        // 地址不匹配，但某些场景可能允许，这里仅作为提示，仍返回 PDU
        // return MODBUS_ERR_MISMATCHED_FC;
    }

    // 7. 将提取的 PDU 复制到用户缓冲区
    if (pdu_len > MODBUS_PDU_MAX_SIZE) return MODBUS_ERR_PDU_TOO_SHORT;
    memcpy(response_pdu, pdu, pdu_len);
    *response_pdu_len = pdu_len;

    return MODBUS_OK;
}