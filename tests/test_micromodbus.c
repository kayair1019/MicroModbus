#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "micromodbus.h"

// 测试位域操作
static void test_bitfield(void) {
    mmbs_bitfield bf = {0};
    mmbs_bitfield_write(bf, 0, 1);
    mmbs_bitfield_write(bf, 7, 1);
    mmbs_bitfield_write(bf, 8, 1);
    assert(mmbs_bitfield_read(bf, 0) == 1);
    assert(mmbs_bitfield_read(bf, 7) == 1);
    assert(mmbs_bitfield_read(bf, 8) == 1);
    mmbs_bitfield_unset(bf, 7);
    assert(mmbs_bitfield_read(bf, 7) == 0);
    printf("Bitfield test passed.\n");
}

// 测试 CRC 计算
static void test_crc(void) {
    uint8_t data[] = {0x02, 0x07};
    uint16_t crc = mmbs_crc_calc(data, sizeof(data), NULL);
    // Modbus 标准 CRC 示例：0x02,0x07 的 CRC 应为 0x1241
    assert(crc == 0x1241);
    printf("CRC test passed.\n");
}

int main(void) {
    test_bitfield();
    test_crc();
    printf("All microMODBUS tests passed.\n");
    return 0;
}
