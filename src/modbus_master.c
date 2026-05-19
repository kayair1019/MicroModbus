#include "modbus_master.h"

int modbus_master_build_read_holding_regs(uint8_t *pdu, uint16_t start_addr, uint16_t quantity)
{
    if (pdu == NULL)
        return -MODBUS_ERR_NULL_PTR;
    if (quantity < 1 || quantity > 125)
        return -MODBUS_ERR_INVALID_COUNT;
    if (start_addr > 0xFFFF)
        return -MODBUS_ERR_INVALID_ADDR;

    pdu[0] = MODBUS_FC_READ_HOLDING_REGISTERS;
    modbus_write_uint16_be(&pdu[1], start_addr);
    modbus_write_uint16_be(&pdu[3], quantity);
    return 5;
}

int modbus_master_build_write_single_register(uint8_t *pdu, uint16_t reg_addr, uint16_t reg_value)
{
    if (pdu == NULL)
        return -MODBUS_ERR_NULL_PTR;
    if (reg_addr > 0xFFFF)
        return -MODBUS_ERR_INVALID_ADDR;

    pdu[0] = MODBUS_FC_WRITE_SINGLE_REGISTER;
    modbus_write_uint16_be(&pdu[1], reg_addr);
    modbus_write_uint16_be(&pdu[3], reg_value);
    return 5;
}

modbus_err_t modbus_master_parse_response(const uint8_t *pdu, uint16_t pdu_len,
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

    // 异常响应
    if (fc & 0x80) {
        if (pdu_len < 2)
            return MODBUS_ERR_PDU_TOO_SHORT;
        pdu_data->is_exception = true;
        pdu_data->exception_code = pdu[1];
        return MODBUS_ERR_EXCEPTION;
    }

    // 正常响应
    switch (fc) {
    case MODBUS_FC_READ_HOLDING_REGISTERS:
    case MODBUS_FC_READ_INPUT_REGISTERS:
        if (pdu_len < 3)
            return MODBUS_ERR_PDU_TOO_SHORT;
        {
            uint8_t byte_count = pdu[1];
            if (pdu_len < (uint16_t)(2 + byte_count))
                return MODBUS_ERR_PDU_TOO_SHORT;
            if (byte_count % 2 != 0)
                return MODBUS_ERR_PDU_TOO_SHORT;
            pdu_data->byte_count = byte_count;
            pdu_data->data.registers.reg_values = (const uint16_t *)(&pdu[2]);
            pdu_data->data.registers.count = byte_count / 2;
        }
        break;

    case MODBUS_FC_WRITE_SINGLE_REGISTER:
    case MODBUS_FC_WRITE_SINGLE_COIL:
        if (pdu_len < 5)
            return MODBUS_ERR_PDU_TOO_SHORT;
        pdu_data->byte_count = 4;
        pdu_data->data.single.address = ((uint16_t)pdu[1] << 8) | pdu[2];
        pdu_data->data.single.value   = ((uint16_t)pdu[3] << 8) | pdu[4];
        break;

    default:
        return MODBUS_ERR_UNSUPPORTED_FC;
    }

    return MODBUS_OK;
}