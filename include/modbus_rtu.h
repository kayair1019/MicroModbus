#ifndef MODBUS_RTU_H
#define MODBUS_RTU_H

#include "modbus_core.h"

#ifdef __cplusplus
extern "C" {
#endif

// RTU ADU 最大长度：地址 1 + PDU 最大 253 + CRC 2 = 256 字节
#define MODBUS_RTU_MAX_ADU_SIZE  256

// 计算 CRC16（Modbus RTU 使用 CRC16-IBM 算法）
uint16_t modbus_rtu_crc16(const uint8_t *data, uint16_t length);

// 构建完整 RTU ADU
// adu_buf：输出缓冲区，调用方保证至少 MODBUS_RTU_MAX_ADU_SIZE 字节
// slave_addr：从站地址
// pdu：已构建好的 PDU 数据
// pdu_len：PDU 字节长度
// 返回值：完整 ADU 的字节数，失败返回负错误码
int modbus_rtu_build_adu(uint8_t *adu_buf, uint8_t slave_addr,
                         const uint8_t *pdu, uint16_t pdu_len);

// 解析完整 RTU ADU（调用方需确保已传入一个完整帧，无沾包/半包）
// adu_data：完整 RTU 帧数据指针
// adu_len：帧字节数
// slave_addr：输出从站地址
// pdu：输出 PDU 数据指针（指向 adu_data 内部）
// pdu_len：输出 PDU 字节数
// 返回值：MODBUS_OK 或错误码（CRC 错误、长度不足等）
modbus_err_t modbus_rtu_parse_adu(const uint8_t *adu_data, uint16_t adu_len,
                                  uint8_t *slave_addr,
                                  const uint8_t **pdu, uint16_t *pdu_len);

#ifdef __cplusplus
}
#endif

#endif // MODBUS_RTU_H