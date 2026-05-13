/*
    microMODBUS - 轻量级 MODBUS 客户端库（仅客户端，支持 RTU 与 TCP）
*/

#ifndef MICROMODBUS_H
#define MICROMODBUS_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 库错误码（负值）与 Modbus 异常（正值） */
typedef enum mmbs_error {
    // 库错误
    MMBS_ERROR_INVALID_RESPONSE   = -8,
    MMBS_ERROR_INVALID_TCP_MBAP   = -7,
    MMBS_ERROR_INVALID_UNIT_ID    = -6,
    MMBS_ERROR_CRC                = -5,
    MMBS_ERROR_TRANSPORT          = -4,
    MMBS_ERROR_TIMEOUT            = -3,
    MMBS_ERROR_INVALID_ARGUMENT   = -2,
    MMBS_ERROR_INVALID_REQUEST    = -1,
    MMBS_ERROR_NONE               = 0,

    // Modbus 异常
    MMBS_EXCEPTION_ILLEGAL_FUNCTION        = 1,
    MMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS    = 2,
    MMBS_EXCEPTION_ILLEGAL_DATA_VALUE      = 3,
    MMBS_EXCEPTION_SERVER_DEVICE_FAILURE   = 4,
} mmbs_error;

/* 错误是否为 Modbus 异常 */
#define mmbs_error_is_exception(e) ((e) > 0 && (e) < 5)

/* 位域最大线圈/离散量数量（必须被 8 整除）*/
#ifndef MMBS_BITFIELD_MAX
#define MMBS_BITFIELD_MAX 2000
#endif

#if ((MMBS_BITFIELD_MAX & 7) > 0)
#error "MMBS_BITFIELD_MAX must be divisible by 8"
#endif

#define MMBS_BITFIELD_BYTES_MAX (MMBS_BITFIELD_MAX / 8)

typedef uint8_t mmbs_bitfield[MMBS_BITFIELD_BYTES_MAX];
typedef uint8_t mmbs_bitfield_256[32];

#define mmbs_bitfield_read(bf, b) ((bool)((bf)[(b) >> 3] & (0x1 << ((b) & 7))))
#define mmbs_bitfield_set(bf, b)   ((bf)[(b) >> 3] |=  (0x1 << ((b) & 7)))
#define mmbs_bitfield_unset(bf, b) ((bf)[(b) >> 3] &= ~(0x1 << ((b) & 7)))
#define mmbs_bitfield_write(bf, b, v) \
    ((bf)[(b) >> 3] = ((bf)[(b) >> 3] & ~(1 << ((b) & 7))) | ((v) << ((b) & 7)))
#define mmbs_bitfield_reset(bf) memset((bf), 0, sizeof(bf))

/* 传输类型 */
typedef enum mmbs_transport {
    MMBS_TRANSPORT_RTU = 1,
    MMBS_TRANSPORT_TCP = 2,
} mmbs_transport;

/* 平台配置（读写函数等） */
typedef struct mmbs_platform_conf {
    mmbs_transport transport;
    int32_t (*read)(uint8_t* buf, uint16_t count, int32_t byte_timeout_ms, void* arg);
    int32_t (*write)(const uint8_t* buf, uint16_t count, int32_t byte_timeout_ms, void* arg);
    uint16_t (*crc_calc)(const uint8_t* data, uint32_t length, void* arg);
    void (*flush)(void* arg);
    void* arg;
    uint32_t initialized;   // 内部使用，必须通过 mmbs_platform_conf_create 初始化
} mmbs_platform_conf;

/* MODBUS 客户端实例（私有成员） */
typedef struct mmbs_t {
    struct {
        uint8_t buf[260];
        uint16_t buf_idx;
        uint8_t unit_id;
        uint8_t fc;
        uint16_t transaction_id;
        bool broadcast;
        bool complete;
    } msg;

    int32_t byte_timeout_ms;
    int32_t read_timeout_ms;
    mmbs_platform_conf platform;
    uint8_t dest_address_rtu;
    uint16_t current_tid;
} mmbs_t;

/* 广播地址 */
static const uint8_t MMBS_BROADCAST_ADDRESS = 0;

/* ----- 公共 API ----- */
void mmbs_platform_conf_create(mmbs_platform_conf* conf);

mmbs_error mmbs_client_create(mmbs_t* ctx, const mmbs_platform_conf* conf);
void mmbs_set_destination_rtu_address(mmbs_t* ctx, uint8_t address);
void mmbs_set_platform_arg(mmbs_t* ctx, void* arg);
void mmbs_set_read_timeout(mmbs_t* ctx, int32_t timeout_ms);
void mmbs_set_byte_timeout(mmbs_t* ctx, int32_t timeout_ms);

/* 功能码 01/02 */
mmbs_error mmbs_read_coils(mmbs_t* ctx, uint16_t address, uint16_t quantity, mmbs_bitfield coils_out);
mmbs_error mmbs_read_discrete_inputs(mmbs_t* ctx, uint16_t address, uint16_t quantity, mmbs_bitfield inputs_out);

/* 功能码 03/04 */
mmbs_error mmbs_read_holding_registers(mmbs_t* ctx, uint16_t address, uint16_t quantity, uint16_t* registers_out);
mmbs_error mmbs_read_input_registers(mmbs_t* ctx, uint16_t address, uint16_t quantity, uint16_t* registers_out);

/* 功能码 05/06 */
mmbs_error mmbs_write_single_coil(mmbs_t* ctx, uint16_t address, bool value);
mmbs_error mmbs_write_single_register(mmbs_t* ctx, uint16_t address, uint16_t value);

/* 功能码 15/16 */
mmbs_error mmbs_write_multiple_coils(mmbs_t* ctx, uint16_t address, uint16_t quantity, const mmbs_bitfield coils);
mmbs_error mmbs_write_multiple_registers(mmbs_t* ctx, uint16_t address, uint16_t quantity, const uint16_t* registers);

/* 功能码 20 / 21 */
mmbs_error mmbs_read_file_record(mmbs_t* ctx, uint16_t file_number, uint16_t record_number,
                                 uint16_t* registers, uint16_t count);
mmbs_error mmbs_write_file_record(mmbs_t* ctx, uint16_t file_number, uint16_t record_number,
                                  const uint16_t* registers, uint16_t count);

/* 功能码 23 */
mmbs_error mmbs_read_write_registers(mmbs_t* ctx,
                                     uint16_t read_address, uint16_t read_quantity, uint16_t* registers_out,
                                     uint16_t write_address, uint16_t write_quantity, const uint16_t* registers);

/* 功能码 43 (MEI 14) 读设备识别 */
mmbs_error mmbs_read_device_identification_basic(mmbs_t* ctx,
    char* vendor_name, char* product_code, char* major_minor_revision, uint8_t buf_len);
mmbs_error mmbs_read_device_identification_regular(mmbs_t* ctx,
    char* vendor_url, char* product_name, char* model_name, char* user_application_name, uint8_t buf_len);
mmbs_error mmbs_read_device_identification_extended(mmbs_t* ctx, uint8_t object_id_start,
    uint8_t* ids, char** buffers, uint8_t ids_len, uint8_t buffer_len, uint8_t* objects_count_out);
mmbs_error mmbs_read_device_identification(mmbs_t* ctx, uint8_t object_id, char* buffer, uint8_t buffer_len);

/* 原始 PDU 收发 */
mmbs_error mmbs_send_raw_pdu(mmbs_t* ctx, uint8_t fc, const uint8_t* data, uint16_t data_len);
mmbs_error mmbs_receive_raw_pdu_response(mmbs_t* ctx, uint8_t* data_out, uint8_t data_out_len);

/* 工具函数 */
uint16_t mmbs_crc_calc(const uint8_t* data, uint32_t length, void* arg);
const char* mmbs_strerror(mmbs_error error);

#ifdef __cplusplus
}
#endif

#endif  // MICROMODBUS_H