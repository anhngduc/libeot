#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../ctf/parseCTF.h" // For populateVdmx, read255UShort etc.
#include "../util/stream.h"
#include <libeot/libeot.h>   // For EOTError codes
#include "../ctf/SFNTContainer.h" // For SFNTTable

// Helper to compare buffers - useful for deep verification
// This can be shared if tests are compiled together, or duplicated. For now, duplicate for standalone file.
static int compare_buffers(const uint8_t* buf1, const uint8_t* buf2, size_t size) {
    if (buf1 == NULL && buf2 == NULL) return 0; // Both NULL, considered equal
    if (buf1 == NULL || buf2 == NULL) return 1; // One NULL, one not, considered different
    return memcmp(buf1, buf2, size);
}

// Helper function to read a uint16_t from a buffer (Big Endian)
static uint16_t read_u16_be(const uint8_t* buf) {
    return (uint16_t)(buf[0] << 8) | buf[1];
}

// Helper function to read a int16_t from a buffer (Big Endian)
static int16_t read_s16_be(const uint8_t* buf) {
    return (int16_t)((uint16_t)(buf[0] << 8) | buf[1]);
}

// Helper function to read a uint32_t from a buffer (Big Endian)
static uint32_t read_u32_be(const uint8_t* buf) {
    return ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) | ((uint32_t)buf[2] << 8) | (uint32_t)buf[3];
}


// --- Test Case 1: Minimal Valid MTX VDMX (Ratio Flags 00b - No Ratios, No Groups) ---
static void test_vdmx_minimal_no_ratios_no_groups() {
    printf("Running test_vdmx_minimal_no_ratios_no_groups...\n");

    // MTX Data:
    // MTXTableVersionInfo: {major=1, minor=0, format=2 (VDMX)}
    // VdmxControlByte: 0x00 (no resolution records, Ratio Flags = 00b -> no ratio data)
    // No further data as no ratios means no groups are expected by populateVdmx's current logic.
    uint8_t mtx_vdmx_data[] = {
        1, 0, 2, // VersionInfo: major=1, minor=0, format=2
        0x00       // ControlByte: no res records, ratio_flags=00b
    };

    struct Stream mtx_stream = constructStream(mtx_vdmx_data, sizeof(mtx_vdmx_data));
    struct SFNTTable vdmx_table;
    memset(&vdmx_table, 0, sizeof(vdmx_table));
    vdmx_table.offset = 0;
    vdmx_table.bufSize = sizeof(mtx_vdmx_data);
    vdmx_table.buf = NULL;

    enum EOTError err = populateVdmx(&vdmx_table, &mtx_stream);

    assert(err == EOT_SUCCESS);
    assert(vdmx_table.buf != NULL); // Should produce a minimal standard header

    // Expected Standard VDMX output:
    // Header: version=1 (u16), numRecs=0 (u16), numRatios=0 (u16)
    // No RatioRange array, No Offset16 array, No VDMXGroup tables
    uint8_t expected_std_vdmx[] = {
        0, 1, // version = 1
        0, 0, // numRecs = 0
        0, 0  // numRatios = 0
    };
    assert(vdmx_table.bufSize == sizeof(expected_std_vdmx));
    assert(compare_buffers(vdmx_table.buf, expected_std_vdmx, sizeof(expected_std_vdmx)) == 0);

    free(vdmx_table.buf);
    printf("PASSED\n");
}

// --- Test Case 2: Invalid MTX VDMX Version ---
static void test_vdmx_invalid_version() {
    printf("Running test_vdmx_invalid_version...\n");
    uint8_t mtx_vdmx_data_bad_version[] = {
        2, 0, 2, // VersionInfo: major=2 (invalid)
        0x00
    };
    struct Stream mtx_stream = constructStream(mtx_vdmx_data_bad_version, sizeof(mtx_vdmx_data_bad_version));
    struct SFNTTable vdmx_table;
    memset(&vdmx_table, 0, sizeof(vdmx_table));
    vdmx_table.offset = 0;
    vdmx_table.bufSize = sizeof(mtx_vdmx_data_bad_version);
    vdmx_table.buf = NULL;

    enum EOTError err = populateVdmx(&vdmx_table, &mtx_stream);

    assert(err == EOT_VDMX_MTX_CORRUPT);
    assert(vdmx_table.buf == NULL);
    printf("PASSED\n");
}

// --- Test Case 3: VDMX Ratio Flags 01b (1:1), one group ---
// MTX VDMX: Version 1.0, Format 2
// Control Byte: 0x01 (no res records, Ratio Flags = 01b -> 1 RatioRecord (1:1), 1 VdmxGroup)
// VdmxGroup 1:
//   recs: 1 (1 vTable entry) (read255UShort, e.g., direct '1')
//   startsz: 10 (read255UShort)
//   endsz: 10 (read255UShort)
//   vTable Entry 1:
//     yPelHeight: 10 (read255UShort)
//     yMax: 8 (read255Short, e.g., direct '8')
//     yMin: -2 (read255Short, e.g., 0xFA for code, then -1 for sign, then code 2 for value -> needs actual read255Short encoding)
//     For yMin = -2: read255Short: 250 (sign bit), 2 (value)
static void test_vdmx_ratio_flags_01_one_group() {
    printf("Running test_vdmx_ratio_flags_01_one_group...\n");
    uint8_t mtx_vdmx_data[] = {
        1, 0, 2, // VersionInfo
        0x01,      // ControlByte (ratio_flags=01b)
        // VdmxGroup 1 Data:
        1,         // recs = 1
        10,        // startsz = 10
        10,        // endsz = 10
        // vTable Entry 1:
        10,        // yPelHeight = 10
        8,         // yMax = 8
        250, 2     // yMin = -2 (read255Short: 250 is sign, 2 is value)
    };

    struct Stream mtx_stream = constructStream(mtx_vdmx_data, sizeof(mtx_vdmx_data));
    struct SFNTTable vdmx_table;
    memset(&vdmx_table, 0, sizeof(vdmx_table));
    vdmx_table.offset = 0;
    vdmx_table.bufSize = sizeof(mtx_vdmx_data);
    vdmx_table.buf = NULL;

    enum EOTError err = populateVdmx(&vdmx_table, &mtx_stream);
    assert(err == EOT_SUCCESS);
    assert(vdmx_table.buf != NULL);

    // Expected Standard VDMX:
    // Header: version=1 (u16), numRecs=1 (u16), numRatios=1 (u16)
    // RatioRange[0]: bCharSet=1, xRatio=1, yStartRatio=1, yEndRatio=1 (4 bytes)
    // vdmxGroupOffsets[0]: offset to group1 (u16). Header (6) + RatioRange (4) + OffsetArray (2) = 12. So offset is 12.
    // VDMXGroup1 Table:
    //   recs=1 (u16), startsz=10 (u8), endsz=10 (u8) (4 bytes)
    //   vTableEntry[0]: yPelHeight=10 (u16), yMax=8 (s16), yMin=-2 (s16) (6 bytes)
    // Total size: 6 (header) + 4 (ratioRange) + 2 (offset) + 4 (group header) + 6 (vtable entry) = 22 bytes

    const uint8_t* buf = vdmx_table.buf;
    assert(vdmx_table.bufSize == 22);

    // Header
    assert(read_u16_be(buf) == 1); // version
    assert(read_u16_be(buf + 2) == 1); // numRecs
    assert(read_u16_be(buf + 4) == 1); // numRatios

    // RatioRange[0]
    assert(buf[6] == 1); // bCharSet
    assert(buf[7] == 1); // xRatio
    assert(buf[8] == 1); // yStartRatio
    assert(buf[9] == 1); // yEndRatio

    // vdmxGroupOffsets[0]
    assert(read_u16_be(buf + 10) == 12); // Offset to group1 table

    // VDMXGroup1 Table (at offset 12)
    assert(read_u16_be(buf + 12) == 1); // recs
    assert(buf[14] == 10); // startsz
    assert(buf[15] == 10); // endsz

    // vTableEntry[0] (at offset 12+4=16)
    assert(read_u16_be(buf + 16) == 10); // yPelHeight
    assert(read_s16_be(buf + 18) == 8);  // yMax
    assert(read_s16_be(buf + 20) == -2); // yMin

    free(vdmx_table.buf);
    printf("PASSED\n");
}

// TODO: Add more VDMX tests as per UNIT_TESTS_MTX.md

/*
int main() {
    test_vdmx_minimal_no_ratios_no_groups();
    test_vdmx_invalid_version();
    test_vdmx_ratio_flags_01_one_group();
    // ... call other tests ...
    printf("All VDMX tests finished.\n");
    return 0;
}
*/
