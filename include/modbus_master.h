#ifndef MODBUS_MASTER_H
#define MODBUS_MASTER_H

#include "modbus_core.h"

#ifdef __cplusplus
extern "C" {
#endif

// 构建读保持寄存器的请求 PDU（功能码 0x03）
int modbus_master_build_read_holding_regs(uint8_t *pdu, uint16_t start_addr, uint16_t quantity);

// 构建写单个寄存器的请求 PDU（功能码 0x06）
int modbus_master_build_write_single_register(uint8_t *pdu, uint16_t reg_addr, uint16_t reg_value);

// 解析主站收到的响应 PDU
// pdu: 完整的响应 PDU 数据
// pdu_len: PDU 字节长度
// pdu_data: 输出解析结果，其中数据指针指向 pdu 内部
// 返回值: MODBUS_OK 表示正常响应，MODBUS_ERR_EXCEPTION 表示异常响应，其他为协议错误
modbus_err_t modbus_master_parse_response(const uint8_t *pdu, uint16_t pdu_len,
                                          modbus_pdu_data_t *pdu_data);

#ifdef __cplusplus
}
#endif

#endif // MODBUS_MASTER_H