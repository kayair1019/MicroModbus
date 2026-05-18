#include "modbus_slave.h"

modbus_err_t modbus_slave_parse_request(const uint8_t *pdu, uint16_t pdu_len,
                                        modbus_pdu_data_t *pdu_data)
{
    if (pdu == NULL || pdu_data == NULL)
        return MODBUS_ERR_NULL_PTR;
    if (pdu_len < 2)
        return MODBUS_ERR_PDU_TOO_SHORT;

    uint8_t fc = pdu[0];
    pdu_data->function_code = fc;
    pdu_data->is_exception = false;
    pdu_data->exception_code = 0;
    pdu_data->byte_count = 0;

    switch (fc) {
    case MODBUS_FC_READ_HOLDING_REGISTERS:
        if (pdu_len != 5)
            return MODBUS_ERR_PDU_TOO_SHORT;
        pdu_data->data.registers.count = ((uint16_t)pdu[3] << 8) | pdu[4];
        // 地址信息也可以通过 union 或扩展字段暴露，这里只存数量，实际使用时可另取
        // 对 slave 而言，最重要的是起始地址和数量，我们可约定调用者用额外函数读取
        // 为了简洁，此处通过 data.registers.count 传递数量，起始地址可通过另一个接口获取
        // 或者扩展 pdu_data 结构。目前保持最小实现，可自行扩展。
        break;

    case MODBUS_FC_WRITE_SINGLE_REGISTER:
        if (pdu_len != 5)
            return MODBUS_ERR_PDU_TOO_SHORT;
        pdu_data->data.single.address = ((uint16_t)pdu[1] << 8) | pdu[2];
        pdu_data->data.single.value   = ((uint16_t)pdu[3] << 8) | pdu[4];
        break;

    default:
        return MODBUS_ERR_UNSUPPORTED_FC;
    }

    return MODBUS_OK;
}

int modbus_slave_build_read_holding_regs_response(uint8_t *pdu,
                                                  const uint16_t *reg_values,
                                                  uint16_t count)
{
    if (pdu == NULL || reg_values == NULL)
        return -MODBUS_ERR_NULL_PTR;
    if (count < 1 || count > 125)
        return -MODBUS_ERR_INVALID_COUNT;

    uint8_t byte_count = count * 2;
    pdu[0] = MODBUS_FC_READ_HOLDING_REGISTERS;
    pdu[1] = byte_count;
    for (uint16_t i = 0; i < count; i++) {
        modbus_write_uint16_be(&pdu[2 + i * 2], reg_values[i]);
    }
    return 2 + byte_count;
}

int modbus_slave_build_write_single_register_response(uint8_t *pdu,
                                                      uint16_t reg_addr,
                                                      uint16_t reg_value)
{
    if (pdu == NULL)
        return -MODBUS_ERR_NULL_PTR;

    pdu[0] = MODBUS_FC_WRITE_SINGLE_REGISTER;
    modbus_write_uint16_be(&pdu[1], reg_addr);
    modbus_write_uint16_be(&pdu[3], reg_value);
    return 5;
}

int modbus_slave_build_exception_response(uint8_t *pdu, uint8_t request_fc,
                                          uint8_t exception_code)
{
    if (pdu == NULL)
        return -MODBUS_ERR_NULL_PTR;

    pdu[0] = request_fc | 0x80;
    pdu[1] = exception_code;
    return 2;
}