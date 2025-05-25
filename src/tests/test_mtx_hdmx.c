#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../ctf/parseCTF.h" // For populateHdmx, read255UShort etc.
#include "../ctf/parseTTF.h" // For TTFmaxpData
#include "../util/stream.h"
#include <libeot/libeot.h>   // For EOTError codes
#include "../ctf/SFNTContainer.h" // For SFNTTable (though parseCTF.h might bring it)


// Helper to compare buffers - useful for deep verification
static int compare_buffers(const uint8_t* buf1, const uint8_t* buf2, size_t size) {
    if (size == 0) return 0; // Or handle as an error/special case if preferred
    return memcmp(buf1, buf2, size);
}

// --- Test Case 1: Minimal Valid MTX HDMX Table ---
static void test_hdmx_minimal_valid() {
    printf("Running test_hdmx_minimal_valid...\n");
    uint8_t mtx_hdmx_data[] = {
        1, 0, 1, // VersionInfo: major=1, minor=0, format=1
        0x00,      // ControlByte: no resolution records
        12,        // PixelSize = 12
        100,       // MaxWidth = 100
        50         // AdvanceWidth[0] = 50
    };
    struct Stream mtx_stream = constructStream((uint8_t*)mtx_hdmx_data, sizeof(mtx_hdmx_data));
    struct SFNTTable hdmx_table;
    memset(&hdmx_table, 0, sizeof(hdmx_table));
    hdmx_table.offset = 0; 
    hdmx_table.bufSize = sizeof(mtx_hdmx_data);
    hdmx_table.buf = NULL; 
    struct TTFmaxpData maxp_data;
    maxp_data.numGlyphs = 1;

    enum EOTError err = populateHdmx(&hdmx_table, &maxp_data, &mtx_stream);

    assert(err == EOT_SUCCESS);
    assert(hdmx_table.buf != NULL);
    uint8_t expected_std_hdmx[] = {
        0, 0,       // version = 0
        0, 1,       // numRecords = 1
        0, 0, 0, 4, // sizeDeviceRecord = 4
        12,         // pixelSize = 12
        100,        // maxWidth = 100
        50,         // widths[0] = 50
        0           // padding
    };
    assert(hdmx_table.bufSize == sizeof(expected_std_hdmx));
    assert(compare_buffers(hdmx_table.buf, expected_std_hdmx, sizeof(expected_std_hdmx)) == 0);
    free(hdmx_table.buf);
    printf("PASSED\n");
}

// --- Test Case 2: Invalid MTX HDMX Version ---
static void test_hdmx_invalid_version() {
    printf("Running test_hdmx_invalid_version...\n");
    uint8_t mtx_hdmx_data_bad_version[] = {
        2, 0, 1, 0x00, 12, 100, 50
    };
    struct Stream mtx_stream = constructStream((uint8_t*)mtx_hdmx_data_bad_version, sizeof(mtx_hdmx_data_bad_version));
    struct SFNTTable hdmx_table;
    memset(&hdmx_table, 0, sizeof(hdmx_table));
    hdmx_table.offset = 0;
    hdmx_table.bufSize = sizeof(mtx_hdmx_data_bad_version);
    hdmx_table.buf = NULL;
    struct TTFmaxpData maxp_data;
    maxp_data.numGlyphs = 1;
    enum EOTError err = populateHdmx(&hdmx_table, &maxp_data, &mtx_stream);
    assert(err == EOT_HDMX_MTX_CORRUPT);
    assert(hdmx_table.buf == NULL);
    printf("PASSED\n");
}

// --- Test Case 3: HDMX Data Truncation ---
static void test_hdmx_truncation() {
    printf("Running test_hdmx_truncation...\n");
    uint8_t mtx_hdmx_data[] = {
        1, 0, 1,           // VersionInfo
        0x00,              // ControlByte
        0xFD, 0x01, 0x2C,   // PixelSize = 300 
        0xFD, 0x01, 0x5E,   // MaxWidth = 350
        0xFD, 0x00, 0xFA    // AdvanceWidth[0] = 250
    };
    struct Stream mtx_stream = constructStream((uint8_t*)mtx_hdmx_data, sizeof(mtx_hdmx_data));
    struct SFNTTable hdmx_table;
    memset(&hdmx_table, 0, sizeof(hdmx_table));
    hdmx_table.offset = 0;
    hdmx_table.bufSize = sizeof(mtx_hdmx_data);
    hdmx_table.buf = NULL;
    struct TTFmaxpData maxp_data;
    maxp_data.numGlyphs = 1;
    enum EOTError err = populateHdmx(&hdmx_table, &maxp_data, &mtx_stream);
    assert(err == EOT_SUCCESS);
    assert(hdmx_table.buf != NULL);
    uint8_t expected_std_hdmx[] = {
        0,0, 0,1, 0,0,0,4, // Header: ver=0, numRec=1, sizeDevRec=4
        (uint8_t)300,      // pixelSize=300 truncated to 44
        (uint8_t)350,      // maxWidth=350 truncated to 94
        250,               // widths[0]=250
        0                  // padding
    };
    assert(hdmx_table.bufSize == sizeof(expected_std_hdmx));
    assert(compare_buffers(hdmx_table.buf, expected_std_hdmx, sizeof(expected_std_hdmx)) == 0);
    free(hdmx_table.buf);
    printf("PASSED\n");
}

// --- Test Case 4: HDMX with Resolution Records ---
static void test_hdmx_with_resolution_records() {
    printf("Running test_hdmx_with_resolution_records...\n");
    uint8_t mtx_hdmx_data[] = {
        1, 1, 1,       // VersionInfo: major=1, minor=1, format=1
        0xC0,          // ControlByte: has X and Y resolution records
        1, 96,         // X Res Records: count 1 (byte 1), value 96 (byte 96)
        1, 96,         // Y Res Records: count 1 (byte 1), value 96 (byte 96)
        12,            // PixelSize = 12
        100,           // MaxWidth = 100
        50             // AdvanceWidth[0] = 50
    };
    struct Stream mtx_stream = constructStream(mtx_hdmx_data, sizeof(mtx_hdmx_data));
    struct SFNTTable hdmx_table;
    memset(&hdmx_table, 0, sizeof(hdmx_table));
    hdmx_table.offset = 0;
    hdmx_table.bufSize = sizeof(mtx_hdmx_data);
    hdmx_table.buf = NULL;
    struct TTFmaxpData maxp_data;
    maxp_data.numGlyphs = 1;
    enum EOTError err = populateHdmx(&hdmx_table, &maxp_data, &mtx_stream);
    assert(err == EOT_SUCCESS);
    assert(hdmx_table.buf != NULL);
    uint8_t expected_std_hdmx[] = {0,0,0,1, 0,0,0,4, 12,100,50,0};
    assert(hdmx_table.bufSize == sizeof(expected_std_hdmx));
    assert(compare_buffers(hdmx_table.buf, expected_std_hdmx, sizeof(expected_std_hdmx)) == 0);
    free(hdmx_table.buf);
    printf("PASSED\n");
}

// --- Test Case 5: Multiple Device Records ---
static void test_hdmx_multiple_device_records() {
    printf("Running test_hdmx_multiple_device_records...\n");
    uint8_t mtx_hdmx_data[] = {
        1,0,1,    // Version
        0x00,     // Control
        12,100,50,52, // Rec1: PS=12, MW=100, AW[0]=50, AW[1]=52
        14,110,60,62  // Rec2: PS=14, MW=110, AW[0]=60, AW[1]=62
    };
    struct Stream mtx_stream = constructStream(mtx_hdmx_data, sizeof(mtx_hdmx_data));
    struct SFNTTable hdmx_table;
    memset(&hdmx_table, 0, sizeof(hdmx_table));
    hdmx_table.offset = 0;
    hdmx_table.bufSize = sizeof(mtx_hdmx_data);
    hdmx_table.buf = NULL;
    struct TTFmaxpData maxp_data;
    maxp_data.numGlyphs = 2;
    enum EOTError err = populateHdmx(&hdmx_table, &maxp_data, &mtx_stream);
    assert(err == EOT_SUCCESS);
    assert(hdmx_table.buf != NULL);
    // sizeDeviceRecord = (2 + numGlyphs=2 + 3)&~3 = (7)&~3 = 4
    uint8_t expected_std_hdmx[] = {0,0,0,2, 0,0,0,4, 12,100,50,52, 14,110,60,62};
    assert(hdmx_table.bufSize == sizeof(expected_std_hdmx));
    assert(compare_buffers(hdmx_table.buf, expected_std_hdmx, sizeof(expected_std_hdmx)) == 0);
    free(hdmx_table.buf);
    printf("PASSED\n");
}

// --- Test Case 6: Premature Stream End ---
static void test_hdmx_premature_stream_end() {
    printf("Running test_hdmx_premature_stream_end...\n");
    uint8_t mtx_hdmx_data[] = {1,0,1, 0x00, 12,100,50}; // AW[1] missing for numGlyphs=2
    struct Stream mtx_stream = constructStream(mtx_hdmx_data, sizeof(mtx_hdmx_data));
    struct SFNTTable hdmx_table;
    memset(&hdmx_table, 0, sizeof(hdmx_table));
    hdmx_table.offset = 0;
    hdmx_table.bufSize = sizeof(mtx_hdmx_data);
    hdmx_table.buf = NULL;
    struct TTFmaxpData maxp_data;
    maxp_data.numGlyphs = 2;
    enum EOTError err = populateHdmx(&hdmx_table, &maxp_data, &mtx_stream);
    assert(err == EOT_HDMX_MTX_CORRUPT);
    assert(hdmx_table.buf == NULL);
    printf("PASSED\n");
}

// --- Test Case 7: numGlyphs = 0 ---
static void test_hdmx_numglyphs_zero() {
    printf("Running test_hdmx_numglyphs_zero...\n");
    uint8_t mtx_hdmx_data[] = {1,0,1,0x00}; // Only header and control byte
    struct Stream mtx_stream = constructStream(mtx_hdmx_data, sizeof(mtx_hdmx_data));
    struct SFNTTable hdmx_table;
    memset(&hdmx_table, 0, sizeof(hdmx_table));
    hdmx_table.offset = 0;
    hdmx_table.bufSize = sizeof(mtx_hdmx_data);
    hdmx_table.buf = NULL;
    struct TTFmaxpData maxp_data;
    maxp_data.numGlyphs = 0;
    enum EOTError err = populateHdmx(&hdmx_table, &maxp_data, &mtx_stream);
    assert(err == EOT_SUCCESS);
    assert(hdmx_table.buf == NULL);
    assert(hdmx_table.bufSize == 0);
    printf("PASSED\n");
}

// --- Test Case 8: Zero Device Records in MTX Stream ---
static void test_hdmx_zero_device_records_in_stream() {
    printf("Running test_hdmx_zero_device_records_in_stream...\n");
    uint8_t mtx_hdmx_data[] = {1,0,1,0x00}; // Header and control byte only
    struct Stream mtx_stream = constructStream(mtx_hdmx_data, sizeof(mtx_hdmx_data));
    struct SFNTTable hdmx_table;
    memset(&hdmx_table, 0, sizeof(hdmx_table));
    hdmx_table.offset = 0;
    hdmx_table.bufSize = sizeof(mtx_hdmx_data);
    hdmx_table.buf = NULL;
    struct TTFmaxpData maxp_data;
    maxp_data.numGlyphs = 10; // Font has glyphs, but no hdmx records for them
    enum EOTError err = populateHdmx(&hdmx_table, &maxp_data, &mtx_stream);
    assert(err == EOT_SUCCESS);
    assert(hdmx_table.buf != NULL);
    // sizeDeviceRecord = (2 + 10 + 3)&~3 = 15&~3 = 12
    uint8_t expected_std_hdmx[] = {0,0,0,0, 0,0,0,12}; // numRecords = 0
    assert(hdmx_table.bufSize == sizeof(expected_std_hdmx));
    assert(compare_buffers(hdmx_table.buf, expected_std_hdmx, sizeof(expected_std_hdmx)) == 0);
    free(hdmx_table.buf);
    printf("PASSED\n");
}

// --- Test Case 4: HDMX with Resolution Records ---
static void test_hdmx_with_resolution_records() {
    printf("Running test_hdmx_with_resolution_records...\n");

    // MTX Data:
    // MTXTableVersionInfo: {major=1, minor=0, format=1}
    // HdmxControlByte: 0xC0 (has X and Y resolution records: 11000000b)
    // numXResRecords: 1 (encoded by read255UShort, e.g., direct '1')
    // xResolution[0]: 72 (encoded by read255UShort, e.g., direct '72')
    // numYResRecords: 1 (encoded by read255UShort, e.g., direct '1')
    // yResolution[0]: 96 (encoded by read255UShort, e.g., direct '96')
    // DeviceRecord 1 (same as minimal_valid):
    //   PixelSize: 12
    //   MaxWidth: 100
    //   AdvanceWidth[0]: 50
    uint8_t mtx_hdmx_data[] = {
        1, 0, 1,       // VersionInfo
        0xC0,          // ControlByte (has X and Y res records)
        1,             // numXResRecords = 1
        72,            // xRes[0] = 72
        1,             // numYResRecords = 1
        96,            // yRes[0] = 96
        12,            // PixelSize = 12
        100,           // MaxWidth = 100
        50             // AdvanceWidth[0] = 50
    };

    struct Stream mtx_stream = constructStream((uint8_t*)mtx_hdmx_data, sizeof(mtx_hdmx_data));
    struct SFNTTable hdmx_table;
    memset(&hdmx_table, 0, sizeof(hdmx_table));
    hdmx_table.offset = 0;
    hdmx_table.bufSize = sizeof(mtx_hdmx_data);
    hdmx_table.buf = NULL;
    struct TTFmaxpData maxp_data;
    maxp_data.numGlyphs = 1;

    enum EOTError err = populateHdmx(&hdmx_table, &maxp_data, &mtx_stream);

    assert(err == EOT_SUCCESS);
    assert(hdmx_table.buf != NULL);

    // Expected Standard HDMX output (identical to test_hdmx_minimal_valid):
    uint8_t expected_std_hdmx[] = {
        0, 0,       // version = 0
        0, 1,       // numRecords = 1
        0, 0, 0, 4, // sizeDeviceRecord = 4
        12,         // pixelSize = 12
        100,        // maxWidth = 100
        50,         // widths[0] = 50
        0           // padding
    };
    assert(hdmx_table.bufSize == sizeof(expected_std_hdmx));
    assert(compare_buffers(hdmx_table.buf, expected_std_hdmx, sizeof(expected_std_hdmx)) == 0);

    free(hdmx_table.buf);
    printf("PASSED\n");
}

// --- Test Case 5: HDMX with numGlyphs = 0 ---
// (Corresponds to Test Case 7 in UNIT_TESTS_MTX.md, but named sequentially after previous tests)
static void test_hdmx_numglyphs_zero_corrected() { // Renamed to avoid conflict if old one existed
    printf("Running test_hdmx_numglyphs_zero_corrected...\n");

    // MTX Data:
    // MTXTableVersionInfo: {major=1, minor=0, format=1}
    // HdmxControlByte: 0x00
    // No device record data is strictly needed in the MTX stream for this test,
    // as populateHdmx should return early if maxp_data.numGlyphs is 0.
    uint8_t mtx_hdmx_data[] = {
        1, 0, 1, // VersionInfo
        0x00,    // ControlByte
    };
   
    struct Stream mtx_stream = constructStream((uint8_t*)mtx_hdmx_data, sizeof(mtx_hdmx_data));
    struct SFNTTable hdmx_table;
    memset(&hdmx_table, 0, sizeof(hdmx_table));
    hdmx_table.offset = 0;
    hdmx_table.bufSize = sizeof(mtx_hdmx_data); 
    hdmx_table.buf = NULL; 
    struct TTFmaxpData maxp_data;
    maxp_data.numGlyphs = 0; // Key part of this test

    enum EOTError err = populateHdmx(&hdmx_table, &maxp_data, &mtx_stream);

    // populateHdmx for numGlyphs=0 returns EOT_SUCCESS, buf=NULL, bufSize=0
    assert(err == EOT_SUCCESS);
    assert(hdmx_table.buf == NULL); 
    assert(hdmx_table.bufSize == 0);

    printf("PASSED\n");
}


// TODO: Add more HDMX tests as per UNIT_TESTS_MTX.md:
// - Inconsistent MTX Data (e.g. trailing data after all records are supposedly read)

/*
int main() {
    test_hdmx_minimal_valid();
    test_hdmx_invalid_version();
    test_hdmx_truncation();
    test_hdmx_with_resolution_records();
    test_hdmx_multiple_device_records();
    test_hdmx_premature_stream_end();
    test_hdmx_numglyphs_zero_corrected(); // Corrected test name
    test_hdmx_zero_device_records_in_stream();
    printf("All HDMX tests finished.\n");
    return 0;
}
*/
