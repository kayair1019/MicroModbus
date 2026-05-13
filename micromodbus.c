/*
    microMODBUS - 轻量级 MODBUS 客户端库（支持 RTU 与 TCP）
    
*/

#include "micromodbus.h"
#include <string.h>

/* 简单调试宏（可随时取消注释） */
/* #define MMBS_DEBUG */
#ifdef MMBS_DEBUG
#include <stdio.h>
#define MMBS_DEBUG_PRINT(...) printf(__VA_ARGS__)
#else
#define MMBS_DEBUG_PRINT(...) ((void)0)
#endif

/* 内部辅助函数 */
static uint8_t get_1(mmbs_t* ctx) {
    uint8_t v = ctx->msg.buf[ctx->msg.buf_idx];
    ctx->msg.buf_idx++;
    return v;
}

static void put_1(mmbs_t* ctx, uint8_t data) {
    ctx->msg.buf[ctx->msg.buf_idx] = data;
    ctx->msg.buf_idx++;
}

static void discard_1(mmbs_t* ctx) {
    ctx->msg.buf_idx++;
}

static uint16_t get_2(mmbs_t* ctx) {
    uint16_t v = (ctx->msg.buf[ctx->msg.buf_idx] << 8) | ctx->msg.buf[ctx->msg.buf_idx + 1];
    ctx->msg.buf_idx += 2;
    return v;
}

static void put_2(mmbs_t* ctx, uint16_t data) {
    ctx->msg.buf[ctx->msg.buf_idx] = (data >> 8) & 0xFF;
    ctx->msg.buf[ctx->msg.buf_idx + 1] = data & 0xFF;
    ctx->msg.buf_idx += 2;
}

static uint8_t* get_n(mmbs_t* ctx, uint16_t n) {
    uint8_t* p = ctx->msg.buf + ctx->msg.buf_idx;
    ctx->msg.buf_idx += n;
    return p;
}

static void put_regs(mmbs_t* ctx, const uint16_t* data, uint16_t n) {
    uint16_t* dst = (uint16_t*)(ctx->msg.buf + ctx->msg.buf_idx);
    ctx->msg.buf_idx += n * 2;
    for (uint16_t i = 0; i < n; i++)
        dst[i] = (data[i] << 8) | ((data[i] >> 8) & 0xFF);
}

static void swap_regs(uint16_t* data, uint16_t n) {
    for (uint16_t i = 0; i < n; i++)
        data[i] = (data[i] << 8) | ((data[i] >> 8) & 0xFF);
}

/* 接收数据 */
static mmbs_error recv(mmbs_t* ctx, uint16_t count) {
    if (ctx->msg.complete)
        return MMBS_ERROR_NONE;
    int32_t ret = ctx->platform.read(ctx->msg.buf + ctx->msg.buf_idx, count,
                                     ctx->byte_timeout_ms, ctx->platform.arg);
    if (ret == count)
        return MMBS_ERROR_NONE;
    if (ret < 0)
        return MMBS_ERROR_TRANSPORT;
    if (ret < count)
        return MMBS_ERROR_TIMEOUT;
    return MMBS_ERROR_TRANSPORT;
}

/* 发送数据 */
static mmbs_error send(mmbs_t* ctx, uint16_t count) {
    int32_t ret = ctx->platform.write(ctx->msg.buf, count,
                                      ctx->byte_timeout_ms, ctx->platform.arg);
    if (ret == count)
        return MMBS_ERROR_NONE;
    if (ret < 0)
        return MMBS_ERROR_TRANSPORT;
    if (ret < count)
        return MMBS_ERROR_TIMEOUT;
    return MMBS_ERROR_TRANSPORT;
}

/* 默认刷新函数：读取所有待处理数据并丢弃 */
static void default_flush(void* arg) {
    mmbs_t* ctx = (mmbs_t*)arg;
    ctx->platform.read(ctx->msg.buf, sizeof(ctx->msg.buf), 0, ctx->platform.arg);
}

/* 消息缓冲区重置 */
static void msg_buf_reset(mmbs_t* ctx) {
    ctx->msg.buf_idx = 0;
}

static void msg_state_reset(mmbs_t* ctx) {
    msg_buf_reset(ctx);
    ctx->msg.unit_id = 0;
    ctx->msg.fc = 0;
    ctx->msg.transaction_id = 0;
    ctx->msg.broadcast = false;
    ctx->msg.complete = false;
}

/* 准备请求头 */
static void msg_state_req(mmbs_t* ctx, uint8_t fc) {
    if (ctx->current_tid == UINT16_MAX)
        ctx->current_tid = 1;
    else
        ctx->current_tid++;
    /* 发送前刷新线路 */
    ctx->platform.flush(ctx->platform.arg);
    msg_state_reset(ctx);
    ctx->msg.unit_id = ctx->dest_address_rtu;
    ctx->msg.fc = fc;
    ctx->msg.transaction_id = ctx->current_tid;
    if (ctx->msg.unit_id == MMBS_BROADCAST_ADDRESS && ctx->platform.transport == MMBS_TRANSPORT_RTU)
        ctx->msg.broadcast = true;
}

static void put_req_header(mmbs_t* ctx, uint16_t data_len) {
    msg_buf_reset(ctx);
    if (ctx->platform.transport == MMBS_TRANSPORT_RTU) {
        put_1(ctx, ctx->msg.unit_id);
    } else {  // TCP
        put_2(ctx, ctx->msg.transaction_id);
        put_2(ctx, 0);                          // protocol id
        put_2(ctx, (uint16_t)(1 + 1 + data_len)); // length
        put_1(ctx, ctx->msg.unit_id);
    }
    put_1(ctx, ctx->msg.fc);
    MMBS_DEBUG_PRINT("MMBS req -> ");
    if (ctx->platform.transport == MMBS_TRANSPORT_RTU) {
        if (ctx->msg.broadcast)
            MMBS_DEBUG_PRINT("broadcast\t");
        else
            MMBS_DEBUG_PRINT("addr %d\t", ctx->dest_address_rtu);
    }
    MMBS_DEBUG_PRINT("fc %d\t", ctx->msg.fc);
}

static mmbs_error send_msg(mmbs_t* ctx) {
    if (ctx->platform.transport == MMBS_TRANSPORT_RTU) {
        uint16_t crc = ctx->platform.crc_calc(ctx->msg.buf, ctx->msg.buf_idx, ctx->platform.arg);
        put_2(ctx, crc);
    }
    return send(ctx, ctx->msg.buf_idx);
}

/* 接收响应头（共用） */
static mmbs_error recv_msg_header(mmbs_t* ctx, bool* first_byte_received) {
    int32_t old_byte_timeout = ctx->byte_timeout_ms;
    ctx->byte_timeout_ms = ctx->read_timeout_ms;
    msg_state_reset(ctx);
    *first_byte_received = false;

    mmbs_error err;
    if (ctx->platform.transport == MMBS_TRANSPORT_RTU) {
        err = recv(ctx, 1);
        ctx->byte_timeout_ms = old_byte_timeout;
        if (err != MMBS_ERROR_NONE)
            return err;
        *first_byte_received = true;
        ctx->msg.unit_id = get_1(ctx);
        err = recv(ctx, 1);
        if (err != MMBS_ERROR_NONE)
            return err;
        ctx->msg.fc = get_1(ctx);
    } else {  // TCP
        err = recv(ctx, 1);
        ctx->byte_timeout_ms = old_byte_timeout;
        if (err != MMBS_ERROR_NONE)
            return err;
        *first_byte_received = true;
        discard_1(ctx);  // skip first byte
        err = recv(ctx, 7);
        if (err != MMBS_ERROR_NONE)
            return err;
        msg_buf_reset(ctx);
        ctx->msg.transaction_id = get_2(ctx);
        uint16_t protocol_id = get_2(ctx);
        uint16_t length = get_2(ctx);
        ctx->msg.unit_id = get_1(ctx);
        ctx->msg.fc = get_1(ctx);
        if (length < 2 || length > 255)
            return MMBS_ERROR_INVALID_TCP_MBAP;
        err = recv(ctx, length - 2);
        if (err != MMBS_ERROR_NONE)
            return err;
        if (protocol_id != 0)
            return MMBS_ERROR_INVALID_TCP_MBAP;
        ctx->msg.complete = true;
    }
    return MMBS_ERROR_NONE;
}

static mmbs_error recv_msg_footer(mmbs_t* ctx) {
    if (ctx->platform.transport == MMBS_TRANSPORT_RTU) {
        uint16_t crc = ctx->platform.crc_calc(ctx->msg.buf, ctx->msg.buf_idx, ctx->platform.arg);
        mmbs_error err = recv(ctx, 2);
        if (err != MMBS_ERROR_NONE)
            return err;
        uint16_t recv_crc = get_2(ctx);
        if (recv_crc != crc)
            return MMBS_ERROR_CRC;
    }
    return MMBS_ERROR_NONE;
}

static mmbs_error recv_res_header(mmbs_t* ctx) {
    uint16_t req_tid = ctx->msg.transaction_id;
    uint8_t req_unit = ctx->msg.unit_id;
    uint8_t req_fc = ctx->msg.fc;

    bool first = false;
    mmbs_error err = recv_msg_header(ctx, &first);
    if (err != MMBS_ERROR_NONE)
        return err;

    if (ctx->platform.transport == MMBS_TRANSPORT_TCP) {
        if (ctx->msg.transaction_id != req_tid)
            return MMBS_ERROR_INVALID_TCP_MBAP;
    }
    if (ctx->platform.transport == MMBS_TRANSPORT_RTU && ctx->msg.unit_id != req_unit)
        return MMBS_ERROR_INVALID_UNIT_ID;

    if (ctx->msg.fc != req_fc) {
        if (ctx->msg.fc - 0x80 == req_fc) {
            err = recv(ctx, 1);
            if (err != MMBS_ERROR_NONE)
                return err;
            uint8_t ex = get_1(ctx);
            err = recv_msg_footer(ctx);
            if (err != MMBS_ERROR_NONE)
                return err;
            if (ex < 1 || ex > 4)
                return MMBS_ERROR_INVALID_RESPONSE;
            return (mmbs_error)ex;
        }
        return MMBS_ERROR_INVALID_RESPONSE;
    }
    return MMBS_ERROR_NONE;
}

/* 各种响应解析函数 */
static mmbs_error recv_read_discrete_res(mmbs_t* ctx, mmbs_bitfield out) {
    mmbs_error err = recv_res_header(ctx);
    if (err != MMBS_ERROR_NONE)
        return err;
    err = recv(ctx, 1);
    if (err != MMBS_ERROR_NONE)
        return err;
    uint8_t bytes = get_1(ctx);
    if (bytes > MMBS_BITFIELD_BYTES_MAX)
        return MMBS_ERROR_INVALID_RESPONSE;
    err = recv(ctx, bytes);
    if (err != MMBS_ERROR_NONE)
        return err;
    if (out) {
        for (uint8_t i = 0; i < bytes; i++)
            out[i] = get_1(ctx);
    } else {
        for (uint8_t i = 0; i < bytes; i++)
            discard_1(ctx);
    }
    return recv_msg_footer(ctx);
}

static mmbs_error recv_read_registers_res(mmbs_t* ctx, uint16_t quantity, uint16_t* out) {
    mmbs_error err = recv_res_header(ctx);
    if (err != MMBS_ERROR_NONE)
        return err;
    err = recv(ctx, 1);
    if (err != MMBS_ERROR_NONE)
        return err;
    uint8_t bytes = get_1(ctx);
    if (bytes > 250 || bytes != quantity * 2)
        return MMBS_ERROR_INVALID_RESPONSE;
    err = recv(ctx, bytes);
    if (err != MMBS_ERROR_NONE)
        return err;
    for (uint8_t i = 0; i < quantity; i++) {
        uint16_t r = get_2(ctx);
        if (out)
            out[i] = r;
    }
    return recv_msg_footer(ctx);
}

static mmbs_error recv_write_single_coil_res(mmbs_t* ctx, uint16_t addr_req, uint16_t val_req) {
    mmbs_error err = recv_res_header(ctx);
    if (err != MMBS_ERROR_NONE)
        return err;
    err = recv(ctx, 4);
    if (err != MMBS_ERROR_NONE)
        return err;
    uint16_t addr = get_2(ctx);
    uint16_t val = get_2(ctx);
    err = recv_msg_footer(ctx);
    if (err != MMBS_ERROR_NONE)
        return err;
    if (addr != addr_req || val != val_req)
        return MMBS_ERROR_INVALID_RESPONSE;
    return MMBS_ERROR_NONE;
}

static mmbs_error recv_write_single_register_res(mmbs_t* ctx, uint16_t addr_req, uint16_t val_req) {
    mmbs_error err = recv_res_header(ctx);
    if (err != MMBS_ERROR_NONE)
        return err;
    err = recv(ctx, 4);
    if (err != MMBS_ERROR_NONE)
        return err;
    uint16_t addr = get_2(ctx);
    uint16_t val = get_2(ctx);
    err = recv_msg_footer(ctx);
    if (err != MMBS_ERROR_NONE)
        return err;
    if (addr != addr_req || val != val_req)
        return MMBS_ERROR_INVALID_RESPONSE;
    return MMBS_ERROR_NONE;
}

static mmbs_error recv_write_multiple_coils_res(mmbs_t* ctx, uint16_t addr_req, uint16_t qty_req) {
    mmbs_error err = recv_res_header(ctx);
    if (err != MMBS_ERROR_NONE)
        return err;
    err = recv(ctx, 4);
    if (err != MMBS_ERROR_NONE)
        return err;
    uint16_t addr = get_2(ctx);
    uint16_t qty = get_2(ctx);
    err = recv_msg_footer(ctx);
    if (err != MMBS_ERROR_NONE)
        return err;
    if (addr != addr_req || qty != qty_req)
        return MMBS_ERROR_INVALID_RESPONSE;
    return MMBS_ERROR_NONE;
}

static mmbs_error recv_write_multiple_registers_res(mmbs_t* ctx, uint16_t addr_req, uint16_t qty_req) {
    mmbs_error err = recv_res_header(ctx);
    if (err != MMBS_ERROR_NONE)
        return err;
    err = recv(ctx, 4);
    if (err != MMBS_ERROR_NONE)
        return err;
    uint16_t addr = get_2(ctx);
    uint16_t qty = get_2(ctx);
    err = recv_msg_footer(ctx);
    if (err != MMBS_ERROR_NONE)
        return err;
    if (addr != addr_req || qty != qty_req)
        return MMBS_ERROR_INVALID_RESPONSE;
    return MMBS_ERROR_NONE;
}

static mmbs_error recv_read_file_record_res(mmbs_t* ctx, uint16_t* regs, uint16_t count) {
    mmbs_error err = recv_res_header(ctx);
    if (err != MMBS_ERROR_NONE)
        return err;
    err = recv(ctx, 1);
    if (err != MMBS_ERROR_NONE)
        return err;
    uint8_t resp_size = get_1(ctx);
    if (resp_size > 250)
        return MMBS_ERROR_INVALID_RESPONSE;
    err = recv(ctx, resp_size);
    if (err != MMBS_ERROR_NONE)
        return err;
    uint8_t sub_size = get_1(ctx) - 1;
    uint8_t ref_type = get_1(ctx);
    uint16_t* data = (uint16_t*)get_n(ctx, sub_size);
    err = recv_msg_footer(ctx);
    if (err != MMBS_ERROR_NONE)
        return err;
    if (regs) {
        if (ref_type != 6)
            return MMBS_ERROR_INVALID_RESPONSE;
        if (count != sub_size / 2)
            return MMBS_ERROR_INVALID_RESPONSE;
        swap_regs(data, count);
        memcpy(regs, data, sub_size);
    }
    return MMBS_ERROR_NONE;
}

static mmbs_error recv_write_file_record_res(mmbs_t* ctx, uint16_t file_req, uint16_t rec_req,
                                             const uint16_t* regs_req, uint16_t count_req) {
    mmbs_error err = recv_res_header(ctx);
    if (err != MMBS_ERROR_NONE)
        return err;
    err = recv(ctx, 1);
    if (err != MMBS_ERROR_NONE)
        return err;
    uint8_t resp_size = get_1(ctx);
    if (resp_size > 251)
        return MMBS_ERROR_INVALID_RESPONSE;
    err = recv(ctx, resp_size);
    if (err != MMBS_ERROR_NONE)
        return err;
    uint8_t ref = get_1(ctx);
    uint16_t file = get_2(ctx);
    uint16_t rec = get_2(ctx);
    uint16_t len = get_2(ctx);
    uint16_t* data = (uint16_t*)get_n(ctx, len * 2);
    err = recv_msg_footer(ctx);
    if (err != MMBS_ERROR_NONE)
        return err;
    if (regs_req) {
        if (ref != 6)
            return MMBS_ERROR_INVALID_RESPONSE;
        if (file != file_req || rec != rec_req || len != count_req)
            return MMBS_ERROR_INVALID_RESPONSE;
        swap_regs(data, len);
        if (memcmp(regs_req, data, len * 2) != 0)
            return MMBS_ERROR_INVALID_RESPONSE;
    }
    return MMBS_ERROR_NONE;
}

static mmbs_error recv_read_device_identification_res(mmbs_t* ctx,
    uint8_t buf_count, char** bufs, uint8_t buf_len, const uint8_t* order,
    uint8_t* ids_out, uint8_t* next_id_out, uint8_t* obj_cnt_out) {
    mmbs_error err = recv_res_header(ctx);
    if (err != MMBS_ERROR_NONE)
        return err;
    err = recv(ctx, 6);
    if (err != MMBS_ERROR_NONE)
        return err;
    if (get_1(ctx) != 0x0E)
        return MMBS_ERROR_INVALID_RESPONSE;
    uint8_t code = get_1(ctx);
    if (code < 1 || code > 4)
        return MMBS_ERROR_INVALID_RESPONSE;
    uint8_t conf = get_1(ctx);
    if (conf < 1 || (conf > 3 && conf < 0x81) || conf > 0x83)
        return MMBS_ERROR_INVALID_RESPONSE;
    uint8_t more = get_1(ctx);
    if (more != 0 && more != 0xFF)
        return MMBS_ERROR_INVALID_RESPONSE;
    uint8_t next_id = get_1(ctx);
    uint8_t obj_cnt = get_1(ctx);
    if (obj_cnt_out)
        *obj_cnt_out = obj_cnt;
    if (buf_count == 0)
        bufs = NULL;
    else if (obj_cnt > buf_count)
        return MMBS_ERROR_INVALID_ARGUMENT;
    if (more == 0)
        next_id = 0x7F;
    if (next_id_out)
        *next_id_out = next_id;

    for (uint8_t i = 0; i < obj_cnt; i++) {
        err = recv(ctx, 2);
        if (err != MMBS_ERROR_NONE)
            return err;
        uint8_t oid = get_1(ctx);
        uint8_t olen = get_1(ctx);
        err = recv(ctx, olen);
        if (err != MMBS_ERROR_NONE)
            return err;
        const char* str = (const char*)get_n(ctx, olen);
        if (ids_out)
            ids_out[i] = oid;
        uint8_t idx = order ? order[oid] : i;
        if (bufs && idx < buf_count) {
            strncpy(bufs[idx], str, buf_len);
            bufs[idx][buf_len - 1] = 0;
        }
    }
    return recv_msg_footer(ctx);
}

/* ---------- 公共 API ---------- */
void mmbs_platform_conf_create(mmbs_platform_conf* conf) {
    memset(conf, 0, sizeof(*conf));
    conf->crc_calc = mmbs_crc_calc;
    conf->flush = default_flush;
    conf->initialized = 0xFFFFDEBE;
}

mmbs_error mmbs_client_create(mmbs_t* ctx, const mmbs_platform_conf* conf) {
    if (!ctx || !conf || conf->initialized != 0xFFFFDEBE)
        return MMBS_ERROR_INVALID_ARGUMENT;
    if (conf->transport != MMBS_TRANSPORT_RTU && conf->transport != MMBS_TRANSPORT_TCP)
        return MMBS_ERROR_INVALID_ARGUMENT;
    if (!conf->read || !conf->write)
        return MMBS_ERROR_INVALID_ARGUMENT;
    memset(ctx, 0, sizeof(mmbs_t));
    ctx->byte_timeout_ms = -1;
    ctx->read_timeout_ms = -1;
    ctx->platform = *conf;
    return MMBS_ERROR_NONE;
}

void mmbs_set_destination_rtu_address(mmbs_t* ctx, uint8_t address) {
    ctx->dest_address_rtu = address;
}

void mmbs_set_platform_arg(mmbs_t* ctx, void* arg) {
    ctx->platform.arg = arg;
}

void mmbs_set_read_timeout(mmbs_t* ctx, int32_t timeout_ms) {
    ctx->read_timeout_ms = timeout_ms;
}

void mmbs_set_byte_timeout(mmbs_t* ctx, int32_t timeout_ms) {
    ctx->byte_timeout_ms = timeout_ms;
}

/* 读线圈 / 离散输入 */
static mmbs_error read_discrete(mmbs_t* ctx, uint8_t fc, uint16_t addr, uint16_t qty, mmbs_bitfield out) {
    if (qty < 1 || qty > MMBS_BITFIELD_MAX)
        return MMBS_ERROR_INVALID_ARGUMENT;
    if ((uint32_t)addr + qty > 0x10000)
        return MMBS_ERROR_INVALID_ARGUMENT;
    msg_state_req(ctx, fc);
    put_req_header(ctx, 4);
    put_2(ctx, addr);
    put_2(ctx, qty);
    mmbs_error err = send_msg(ctx);
    if (err != MMBS_ERROR_NONE)
        return err;
    return recv_read_discrete_res(ctx, out);
}

mmbs_error mmbs_read_coils(mmbs_t* ctx, uint16_t addr, uint16_t qty, mmbs_bitfield out) {
    return read_discrete(ctx, 1, addr, qty, out);
}

mmbs_error mmbs_read_discrete_inputs(mmbs_t* ctx, uint16_t addr, uint16_t qty, mmbs_bitfield out) {
    return read_discrete(ctx, 2, addr, qty, out);
}

/* 读寄存器 */
static mmbs_error read_registers(mmbs_t* ctx, uint8_t fc, uint16_t addr, uint16_t qty, uint16_t* out) {
    if (qty < 1 || qty > 125)
        return MMBS_ERROR_INVALID_ARGUMENT;
    if ((uint32_t)addr + qty > 0x10000)
        return MMBS_ERROR_INVALID_ARGUMENT;
    msg_state_req(ctx, fc);
    put_req_header(ctx, 4);
    put_2(ctx, addr);
    put_2(ctx, qty);
    mmbs_error err = send_msg(ctx);
    if (err != MMBS_ERROR_NONE)
        return err;
    return recv_read_registers_res(ctx, qty, out);
}

mmbs_error mmbs_read_holding_registers(mmbs_t* ctx, uint16_t addr, uint16_t qty, uint16_t* out) {
    return read_registers(ctx, 3, addr, qty, out);
}

mmbs_error mmbs_read_input_registers(mmbs_t* ctx, uint16_t addr, uint16_t qty, uint16_t* out) {
    return read_registers(ctx, 4, addr, qty, out);
}

/* 写单线圈 */
mmbs_error mmbs_write_single_coil(mmbs_t* ctx, uint16_t addr, bool val) {
    uint16_t val_net = val ? 0xFF00 : 0;
    msg_state_req(ctx, 5);
    put_req_header(ctx, 4);
    put_2(ctx, addr);
    put_2(ctx, val_net);
    mmbs_error err = send_msg(ctx);
    if (err != MMBS_ERROR_NONE)
        return err;
    if (ctx->msg.broadcast)
        return MMBS_ERROR_NONE;
    return recv_write_single_coil_res(ctx, addr, val_net);
}

/* 写单寄存器 */
mmbs_error mmbs_write_single_register(mmbs_t* ctx, uint16_t addr, uint16_t val) {
    msg_state_req(ctx, 6);
    put_req_header(ctx, 4);
    put_2(ctx, addr);
    put_2(ctx, val);
    mmbs_error err = send_msg(ctx);
    if (err != MMBS_ERROR_NONE)
        return err;
    if (ctx->msg.broadcast)
        return MMBS_ERROR_NONE;
    return recv_write_single_register_res(ctx, addr, val);
}

/* 写多线圈 */
mmbs_error mmbs_write_multiple_coils(mmbs_t* ctx, uint16_t addr, uint16_t qty, const mmbs_bitfield coils) {
    if (qty < 1 || qty > 0x07B0)
        return MMBS_ERROR_INVALID_ARGUMENT;
    if ((uint32_t)addr + qty > 0x10000)
        return MMBS_ERROR_INVALID_ARGUMENT;
    uint8_t bytes = (qty + 7) / 8;
    msg_state_req(ctx, 15);
    put_req_header(ctx, 5 + bytes);
    put_2(ctx, addr);
    put_2(ctx, qty);
    put_1(ctx, bytes);
    for (uint8_t i = 0; i < bytes; i++)
        put_1(ctx, coils[i]);
    mmbs_error err = send_msg(ctx);
    if (err != MMBS_ERROR_NONE)
        return err;
    if (ctx->msg.broadcast)
        return MMBS_ERROR_NONE;
    return recv_write_multiple_coils_res(ctx, addr, qty);
}

/* 写多寄存器 */
mmbs_error mmbs_write_multiple_registers(mmbs_t* ctx, uint16_t addr, uint16_t qty, const uint16_t* regs) {
    if (qty < 1 || qty > 0x007B)
        return MMBS_ERROR_INVALID_ARGUMENT;
    if ((uint32_t)addr + qty > 0x10000)
        return MMBS_ERROR_INVALID_ARGUMENT;
    uint8_t bytes = qty * 2;
    msg_state_req(ctx, 16);
    put_req_header(ctx, 5 + bytes);
    put_2(ctx, addr);
    put_2(ctx, qty);
    put_1(ctx, bytes);
    for (uint16_t i = 0; i < qty; i++)
        put_2(ctx, regs[i]);
    mmbs_error err = send_msg(ctx);
    if (err != MMBS_ERROR_NONE)
        return err;
    if (ctx->msg.broadcast)
        return MMBS_ERROR_NONE;
    return recv_write_multiple_registers_res(ctx, addr, qty);
}

/* 文件记录读写 */
mmbs_error mmbs_read_file_record(mmbs_t* ctx, uint16_t file, uint16_t rec, uint16_t* regs, uint16_t count) {
    if (file == 0 || rec > 0x270F || count > 124)
        return MMBS_ERROR_INVALID_ARGUMENT;
    msg_state_req(ctx, 20);
    put_req_header(ctx, 8);
    put_1(ctx, 7);
    put_1(ctx, 6);
    put_2(ctx, file);
    put_2(ctx, rec);
    put_2(ctx, count);
    mmbs_error err = send_msg(ctx);
    if (err != MMBS_ERROR_NONE)
        return err;
    return recv_read_file_record_res(ctx, regs, count);
}

mmbs_error mmbs_write_file_record(mmbs_t* ctx, uint16_t file, uint16_t rec, const uint16_t* regs, uint16_t count) {
    if (file == 0 || rec > 0x270F || count > 122)
        return MMBS_ERROR_INVALID_ARGUMENT;
    uint16_t data_bytes = count * 2;
    msg_state_req(ctx, 21);
    put_req_header(ctx, 8 + data_bytes);
    put_1(ctx, 7 + data_bytes);
    put_1(ctx, 6);
    put_2(ctx, file);
    put_2(ctx, rec);
    put_2(ctx, count);
    put_regs(ctx, regs, count);
    mmbs_error err = send_msg(ctx);
    if (err != MMBS_ERROR_NONE)
        return err;
    if (ctx->msg.broadcast)
        return MMBS_ERROR_NONE;
    return recv_write_file_record_res(ctx, file, rec, regs, count);
}

/* 读写多个寄存器 (FC23) */
mmbs_error mmbs_read_write_registers(mmbs_t* ctx,
    uint16_t read_addr, uint16_t read_qty, uint16_t* out,
    uint16_t write_addr, uint16_t write_qty, const uint16_t* regs) {
    if (read_qty < 1 || read_qty > 125)
        return MMBS_ERROR_INVALID_ARGUMENT;
    if (write_qty < 1 || write_qty > 121)
        return MMBS_ERROR_INVALID_ARGUMENT;
    if ((uint32_t)read_addr + read_qty > 0x10000)
        return MMBS_ERROR_INVALID_ARGUMENT;
    if ((uint32_t)write_addr + write_qty > 0x10000)
        return MMBS_ERROR_INVALID_ARGUMENT;
    uint8_t bytes = write_qty * 2;
    msg_state_req(ctx, 23);
    put_req_header(ctx, 9 + bytes);
    put_2(ctx, read_addr);
    put_2(ctx, read_qty);
    put_2(ctx, write_addr);
    put_2(ctx, write_qty);
    put_1(ctx, bytes);
    for (uint16_t i = 0; i < write_qty; i++)
        put_2(ctx, regs[i]);
    mmbs_error err = send_msg(ctx);
    if (err != MMBS_ERROR_NONE)
        return err;
    return recv_read_registers_res(ctx, read_qty, out);
}

/* 读设备识别 (FC43) */
mmbs_error mmbs_read_device_identification_basic(mmbs_t* ctx,
    char* vname, char* pcode, char* revision, uint8_t buf_len) {
    const uint8_t order[3] = {0,1,2};
    char* bufs[3] = {vname, pcode, revision};
    uint8_t next = 0;
    while (next != 0x7F) {
        msg_state_req(ctx, 43);
        put_req_header(ctx, 3);
        put_1(ctx, 0x0E);
        put_1(ctx, 1);
        put_1(ctx, next);
        mmbs_error err = send_msg(ctx);
        if (err)
            return err;
        uint8_t cnt = 0;
        err = recv_read_device_identification_res(ctx, 3, bufs, buf_len, order, NULL, &next, &cnt);
        if (err)
            return err;
        if (cnt == 0)
            return MMBS_ERROR_INVALID_RESPONSE;
    }
    return MMBS_ERROR_NONE;
}

mmbs_error mmbs_read_device_identification_regular(mmbs_t* ctx,
    char* vurl, char* pname, char* mname, char* uapp, uint8_t buf_len) {
    const uint8_t order[7] = {0,0,0,0,1,2,3};
    char* bufs[4] = {vurl, pname, mname, uapp};
    uint8_t next = 0x03;
    while (next != 0x7F) {
        msg_state_req(ctx, 43);
        put_req_header(ctx, 3);
        put_1(ctx, 0x0E);
        put_1(ctx, 2);
        put_1(ctx, next);
        mmbs_error err = send_msg(ctx);
        if (err)
            return err;
        uint8_t cnt = 0;
        err = recv_read_device_identification_res(ctx, 4, bufs, buf_len, order, NULL, &next, &cnt);
        if (err)
            return err;
        if (cnt == 0)
            return MMBS_ERROR_INVALID_RESPONSE;
    }
    return MMBS_ERROR_NONE;
}

mmbs_error mmbs_read_device_identification_extended(mmbs_t* ctx, uint8_t start_id,
    uint8_t* ids, char** buffers, uint8_t ids_len, uint8_t buf_len, uint8_t* obj_cnt_out) {
    if (start_id < 0x80)
        return MMBS_ERROR_INVALID_ARGUMENT;
    uint8_t total = 0, next = start_id;
    while (next != 0x7F) {
        msg_state_req(ctx, 43);
        put_req_header(ctx, 3);
        put_1(ctx, 0x0E);
        put_1(ctx, 3);
        put_1(ctx, next);
        mmbs_error err = send_msg(ctx);
        if (err)
            return err;
        uint8_t cnt = 0;
        err = recv_read_device_identification_res(ctx, ids_len - total, &buffers[total], buf_len,
                                                  NULL, &ids[total], &next, &cnt);
        if (err)
            return err;
        total += cnt;
    }
    *obj_cnt_out = total;
    return MMBS_ERROR_NONE;
}

mmbs_error mmbs_read_device_identification(mmbs_t* ctx, uint8_t oid, char* buf, uint8_t buf_len) {
    if ((oid <= 6 && oid >= 0x80) || (oid > 6 && oid < 0x80))
        return MMBS_ERROR_INVALID_ARGUMENT;
    msg_state_req(ctx, 43);
    put_req_header(ctx, 3);
    put_1(ctx, 0x0E);
    put_1(ctx, 4);
    put_1(ctx, oid);
    mmbs_error err = send_msg(ctx);
    if (err)
        return err;
    char* tmp[1] = {buf};
    return recv_read_device_identification_res(ctx, 1, tmp, buf_len, NULL, NULL, NULL, NULL);
}

/* 原始 PDU 收发 */
mmbs_error mmbs_send_raw_pdu(mmbs_t* ctx, uint8_t fc, const uint8_t* data, uint16_t len) {
    msg_state_req(ctx, fc);
    put_req_header(ctx, len);
    for (uint16_t i = 0; i < len; i++)
        put_1(ctx, data[i]);
    return send_msg(ctx);
}

mmbs_error mmbs_receive_raw_pdu_response(mmbs_t* ctx, uint8_t* data_out, uint8_t out_len) {
    mmbs_error err = recv_res_header(ctx);
    if (err)
        return err;
    err = recv(ctx, out_len);
    if (err)
        return err;
    if (data_out) {
        for (uint8_t i = 0; i < out_len; i++)
            data_out[i] = get_1(ctx);
    } else {
        for (uint8_t i = 0; i < out_len; i++)
            discard_1(ctx);
    }
    return recv_msg_footer(ctx);
}

/* CRC 计算 */
uint16_t mmbs_crc_calc(const uint8_t* data, uint32_t len, void* arg) {
    (void)arg;
    uint16_t crc = 0xFFFF;
    for (uint32_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xA001;
            else
                crc >>= 1;
        }
    }
    return (crc << 8) | (crc >> 8);
}

/* 错误描述 */
const char* mmbs_strerror(mmbs_error err) {
    switch (err) {
        case MMBS_ERROR_INVALID_RESPONSE:      return "invalid response";
        case MMBS_ERROR_INVALID_TCP_MBAP:      return "invalid TCP MBAP";
        case MMBS_ERROR_INVALID_UNIT_ID:       return "invalid unit ID";
        case MMBS_ERROR_CRC:                   return "CRC mismatch";
        case MMBS_ERROR_TRANSPORT:             return "transport error";
        case MMBS_ERROR_TIMEOUT:               return "timeout";
        case MMBS_ERROR_INVALID_ARGUMENT:      return "invalid argument";
        case MMBS_ERROR_INVALID_REQUEST:       return "invalid request";
        case MMBS_ERROR_NONE:                  return "no error";
        case MMBS_EXCEPTION_ILLEGAL_FUNCTION:      return "exception 01: illegal function";
        case MMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS:  return "exception 02: illegal data address";
        case MMBS_EXCEPTION_ILLEGAL_DATA_VALUE:    return "exception 03: illegal data value";
        case MMBS_EXCEPTION_SERVER_DEVICE_FAILURE: return "exception 04: slave device failure";
        default: return "unknown error";
    }
}