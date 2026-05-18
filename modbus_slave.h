#ifndef MODBUS_SLAVE_H
#define MODBUS_SLAVE_H

#include "modbus_core.h"

#ifdef __cplusplus
extern "C" {
#endif

// 解析从站收到的请求 PDU
// pdu: 完整的请求 PDU
// pdu_len: PDU 字节数
// pdu_data: 输出解析结果，标明功能码及参数
modbus_err_t modbus_slave_parse_request(const uint8_t *pdu, uint16_t pdu_len,
                                        modbus_pdu_data_t *pdu_data);

// 构建 0x03 正常响应
// pdu: 输出缓冲区
// reg_values: 要返回的寄存器值数组
// count: 寄存器数量 (1~125)
// 返回值: PDU 字节数，失败返回负错误码
int modbus_slave_build_read_holding_regs_response(uint8_t *pdu,
                                                  const uint16_t *reg_values,
                                                  uint16_t count);

// 构建 0x06 正常响应（回显）
int modbus_slave_build_write_single_register_response(uint8_t *pdu,
                                                      uint16_t reg_addr,
                                                      uint16_t reg_value);

// 构建异常响应
// pdu: 输出缓冲区
// request_fc: 原始请求的功能码
// exception_code: 异常码
// 返回值: PDU 字节数
int modbus_slave_build_exception_response(uint8_t *pdu, uint8_t request_fc,
                                          uint8_t exception_code);

#ifdef __cplusplus
}
#endif

#endif // MODBUS_SLAVE_H