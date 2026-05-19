#ifndef MODBUS_TCP_H
#define MODBUS_TCP_H

#include "modbus_core.h"

#ifdef __cplusplus
extern "C" {
#endif

// MBAP 头长度（事务 ID + 协议 ID + 长度 + 单元 ID）
#define MODBUS_TCP_MBAP_HEADER_SIZE  7

// TCP ADU 最大长度：MBAP头 7 + PDU 253 = 260 字节
#define MODBUS_TCP_MAX_ADU_SIZE      260

// 构建 Modbus TCP ADU
// adu_buf：输出缓冲区，调用方保证至少 MODBUS_TCP_MAX_ADU_SIZE 字节
// transaction_id：事务标识符，由调用方管理
// unit_id：单元标识符（通常等同于从站地址）
// pdu：已构建好的 PDU 数据
// pdu_len：PDU 字节长度
// 返回值：完整 ADU 的字节数，失败返回负错误码
int modbus_tcp_build_adu(uint8_t *adu_buf, uint16_t transaction_id,
                         uint8_t unit_id,
                         const uint8_t *pdu, uint16_t pdu_len);

// 解析 Modbus TCP ADU（调用方需确保已传入一个完整 ADU，无沾包/半包）
// adu_data：完整 TCP ADU 数据指针
// adu_len：ADU 字节数
// transaction_id：输出事务 ID
// unit_id：输出单元 ID
// pdu：输出 PDU 数据指针（指向 adu_data 内部）
// pdu_len：输出 PDU 字节数
// 返回值：MODBUS_OK 或错误码（长度不匹配、协议 ID 非零等）
modbus_err_t modbus_tcp_parse_adu(const uint8_t *adu_data, uint16_t adu_len,
                                  uint16_t *transaction_id, uint8_t *unit_id,
                                  const uint8_t **pdu, uint16_t *pdu_len);

#ifdef __cplusplus
}
#endif

#endif // MODBUS_TCP_H