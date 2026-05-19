#include "modbus_tcp.h"

// TCP_ADU 构建函数
int modbus_tcp_build_adu(uint8_t *adu_buf, uint16_t transaction_id,
                         uint8_t unit_id,
                         const uint8_t *pdu, uint16_t pdu_len)
{
    if (adu_buf == NULL || pdu == NULL)
        return -MODBUS_ERR_NULL_PTR;
    if (pdu_len == 0 || pdu_len > MODBUS_PDU_MAX_SIZE)
        return -MODBUS_ERR_DATA_TOO_LONG;

    // 事务 ID（大端序）
    adu_buf[0] = (uint8_t)(transaction_id >> 8);
    adu_buf[1] = (uint8_t)(transaction_id & 0xFF);

    // 协议 ID 固定为 0x0000（Modbus）
    adu_buf[2] = 0x00;
    adu_buf[3] = 0x00;

    // 长度字段 = 单元 ID(1) + PDU长度，大端序
    uint16_t length = 1 + pdu_len;
    adu_buf[4] = (uint8_t)(length >> 8);
    adu_buf[5] = (uint8_t)(length & 0xFF);

    // 单元 ID
    adu_buf[6] = unit_id;

    // PDU 数据
    for (uint16_t i = 0; i < pdu_len; i++) {
        adu_buf[7 + i] = pdu[i];
    }

    return 7 + pdu_len;
}

// TCP_ADU 解析函数
modbus_err_t modbus_tcp_parse_adu(const uint8_t *adu_data, uint16_t adu_len,
                                  uint16_t *transaction_id, uint8_t *unit_id,
                                  const uint8_t **pdu, uint16_t *pdu_len)
{
    if (adu_data == NULL || transaction_id == NULL ||
        unit_id == NULL || pdu == NULL || pdu_len == NULL)
        return MODBUS_ERR_NULL_PTR;

    // 最小长度：MBAP头 7 字节
    if (adu_len < MODBUS_TCP_MBAP_HEADER_SIZE)
        return MODBUS_ERR_PDU_TOO_SHORT;

    // 协议 ID 检查（必须为 0）
    if (adu_data[2] != 0x00 || adu_data[3] != 0x00)
        return MODBUS_ERR_UNSUPPORTED_FC; // 临时借用

    // 解析长度字段
    uint16_t length = ((uint16_t)adu_data[4] << 8) | adu_data[5];
    if (length < 1)  // 至少单元 ID 1 字节
        return MODBUS_ERR_PDU_TOO_SHORT;

    // 检查整个 ADU 长度是否匹配
    if (adu_len != 6 + length)  // 6 = 事务ID+协议ID+长度字段本身
        return MODBUS_ERR_PDU_TOO_SHORT;

    *transaction_id = ((uint16_t)adu_data[0] << 8) | adu_data[1];
    *unit_id = adu_data[6];
    *pdu = &adu_data[7];
    *pdu_len = length - 1; // 减去单元 ID 1 字节

    return MODBUS_OK;
}