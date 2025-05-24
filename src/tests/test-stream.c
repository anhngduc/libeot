#include <stdio.h>
#include <time.h> // For clock()
#include <stdlib.h> // For malloc and free
#include <string.h> // For memset (used in some tests)
#include <stdbool.h> // For bool type used in readNBits_original_for_test

#include "../util/stream.h"

// Original readNBits function for testing comparison
/* FIXME: This could be made A LOT faster. I am too lazy to figure out how. */
static enum StreamResult readNBits_original_for_test(struct Stream *s, uint32_t *out, unsigned n)
{
  const uint8_t masks[] = {0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01};
  if (n > 32) {
    return EOT_VALUE_OUT_OF_BOUNDS;
  }
  *out = 0;
  for (unsigned i = 0; i < n; ++i) {
    if (s->pos >= s->size) {
      return EOT_NOT_ENOUGH_DATA;
    }
    bool bitSet = (s->buf[s->pos] & masks[s->bitPos]) > 0;
    *out |= (bitSet ? 1 : 0) << (n - i - 1); // Shift current bit to its final place
    ++s->bitPos;
    if (s->bitPos == 8) {
      s->bitPos = 0;
      ++s->pos;
    }
  }
  return EOT_STREAM_OK;
}

// Assertion Macro Helper
// Expands to a printf and return for failed assertions
// Note: test_name_str must be a string literal
#define ASSERT_COND(condition, test_name_str, details_fmt, ...) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "FAIL: %s (%s:%d)\n      Condition: %s\n      Details: " details_fmt "\n\n", \
                    test_name_str, __func__, __LINE__, #condition, ##__VA_ARGS__); \
            return 1; /* Indicates test failure */ \
        } \
    } while (0)

#define ASSERT_EQ_STREAM_RESULT(actual, expected, test_name_str, detail_suffix) \
    ASSERT_COND((actual) == (expected), test_name_str, "StreamResult - %s. Expected: %d, Got: %d", \
                detail_suffix, (int)(expected), (int)(actual))

#define ASSERT_EQ_U32(actual, expected, test_name_str, detail_suffix) \
    ASSERT_COND((actual) == (expected), test_name_str, "uint32_t - %s. Expected: %u (0x%X), Got: %u (0x%X)", \
                detail_suffix, (uint32_t)(expected), (uint32_t)(expected), (uint32_t)(actual), (uint32_t)(actual))

#define ASSERT_EQ_U8(actual, expected, test_name_str, detail_suffix) \
    ASSERT_COND((actual) == (expected), test_name_str, "uint8_t - %s. Expected: %u (0x%X), Got: %u (0x%X)", \
                detail_suffix, (uint8_t)(expected), (uint8_t)(expected), (uint8_t)(actual), (uint8_t)(actual))

#define ASSERT_EQ_SIZE_T(actual, expected, test_name_str, detail_suffix) \
    ASSERT_COND((actual) == (expected), test_name_str, "size_t - %s. Expected: %zu, Got: %zu", \
                detail_suffix, (size_t)(expected), (size_t)(actual))


// Test data
static uint8_t test_buf_simple[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x12, 0x34};
static uint8_t test_buf_all_ff[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static uint8_t test_buf_pattern[] = {0xA5, 0x5A, 0xC3, 0x3C, 0xF0, 0x0F, 0x1E, 0xE1};
// 0xA5 = 10100101
// 0x5A = 01011010
// 0xC3 = 11000011
// 0x3C = 00111100

// Forward declaration for the new test suite
static int test_readNBits_additional_vectors_suite();

// --- Test Suites for BEReadU* ---
int test_BEReadU8_suite() {
    uint8_t buffer[] = {0x12, 0x34};
    struct Stream s;
    uint8_t out_u8;
    enum StreamResult res;

    // Basic read
    s = constructStream(buffer, sizeof(buffer));
    res = BEReadU8(&s, &out_u8);
    ASSERT_EQ_STREAM_RESULT(res, EOT_STREAM_OK, "BEReadU8", "Basic read result");
    ASSERT_EQ_U8(out_u8, 0x12, "BEReadU8", "Basic read value");
    ASSERT_EQ_SIZE_T(s.pos, 1, "BEReadU8", "Basic read pos");
    ASSERT_EQ_SIZE_T(s.bitPos, 0, "BEReadU8", "Basic read bitPos");

    // Read second byte
    res = BEReadU8(&s, &out_u8);
    ASSERT_EQ_STREAM_RESULT(res, EOT_STREAM_OK, "BEReadU8", "Second read result");
    ASSERT_EQ_U8(out_u8, 0x34, "BEReadU8", "Second read value");
    ASSERT_EQ_SIZE_T(s.pos, 2, "BEReadU8", "Second read pos");

    // Error: Not enough data
    res = BEReadU8(&s, &out_u8);
    ASSERT_EQ_STREAM_RESULT(res, EOT_NOT_ENOUGH_DATA, "BEReadU8", "Not enough data");

    // Error: Off byte boundary
    s = constructStream(buffer, sizeof(buffer));
    s.bitPos = 1;
    res = BEReadU8(&s, &out_u8);
    ASSERT_EQ_STREAM_RESULT(res, EOT_OFF_BYTE_BOUNDARY, "BEReadU8", "Off byte boundary");
    
    printf("test_BEReadU8_suite: PASSED\n");
    return 0;
}

int test_BEReadU16_suite() {
    uint8_t buffer[] = {0x12, 0x34, 0x56}; // Enough for one U16, then partial
    struct Stream s;
    uint16_t out_u16;
    enum StreamResult res;

    // Basic read
    s = constructStream(buffer, sizeof(buffer));
    res = BEReadU16(&s, &out_u16);
    ASSERT_EQ_STREAM_RESULT(res, EOT_STREAM_OK, "BEReadU16", "Basic read result");
    ASSERT_EQ_U32(out_u16, 0x1234, "BEReadU16", "Basic read value"); // ASSERT_EQ_U32 used for 16-bit too
    ASSERT_EQ_SIZE_T(s.pos, 2, "BEReadU16", "Basic read pos");
    ASSERT_EQ_SIZE_T(s.bitPos, 0, "BEReadU16", "Basic read bitPos");

    // Error: Not enough data (only 1 byte left)
    res = BEReadU16(&s, &out_u16);
    ASSERT_EQ_STREAM_RESULT(res, EOT_NOT_ENOUGH_DATA, "BEReadU16", "Not enough data for second read");

    // Error: Not enough data (empty stream)
    s = constructStream(buffer, 0);
    res = BEReadU16(&s, &out_u16);
    ASSERT_EQ_STREAM_RESULT(res, EOT_NOT_ENOUGH_DATA, "BEReadU16", "Not enough data (empty)");
    
    // Error: Off byte boundary
    s = constructStream(buffer, sizeof(buffer));
    s.bitPos = 1;
    res = BEReadU16(&s, &out_u16);
    ASSERT_EQ_STREAM_RESULT(res, EOT_OFF_BYTE_BOUNDARY, "BEReadU16", "Off byte boundary");

    printf("test_BEReadU16_suite: PASSED\n");
    return 0;
}

int test_BEReadU24_suite() {
    uint8_t buffer[] = {0x12, 0x34, 0x56, 0x78}; // Enough for one U24, then partial
    struct Stream s;
    uint32_t out_u24;
    enum StreamResult res;

    // Basic read
    s = constructStream(buffer, sizeof(buffer));
    res = BEReadU24(&s, &out_u24);
    ASSERT_EQ_STREAM_RESULT(res, EOT_STREAM_OK, "BEReadU24", "Basic read result");
    ASSERT_EQ_U32(out_u24, 0x123456, "BEReadU24", "Basic read value");
    ASSERT_EQ_SIZE_T(s.pos, 3, "BEReadU24", "Basic read pos");
    ASSERT_EQ_SIZE_T(s.bitPos, 0, "BEReadU24", "Basic read bitPos");

    // Error: Not enough data (only 1 byte left)
    res = BEReadU24(&s, &out_u24);
    ASSERT_EQ_STREAM_RESULT(res, EOT_NOT_ENOUGH_DATA, "BEReadU24", "Not enough data for second read");
    
    // Error: Off byte boundary
    s = constructStream(buffer, sizeof(buffer));
    s.bitPos = 1;
    res = BEReadU24(&s, &out_u24);
    ASSERT_EQ_STREAM_RESULT(res, EOT_OFF_BYTE_BOUNDARY, "BEReadU24", "Off byte boundary");

    printf("test_BEReadU24_suite: PASSED\n");
    return 0;
}

int test_BEReadU32_suite() {
    uint8_t buffer[] = {0x12, 0x34, 0x56, 0x78, 0x9A}; // Enough for one U32, then partial
    struct Stream s;
    uint32_t out_u32;
    enum StreamResult res;

    // Basic read
    s = constructStream(buffer, sizeof(buffer));
    res = BEReadU32(&s, &out_u32);
    ASSERT_EQ_STREAM_RESULT(res, EOT_STREAM_OK, "BEReadU32", "Basic read result");
    ASSERT_EQ_U32(out_u32, 0x12345678, "BEReadU32", "Basic read value");
    ASSERT_EQ_SIZE_T(s.pos, 4, "BEReadU32", "Basic read pos");
    ASSERT_EQ_SIZE_T(s.bitPos, 0, "BEReadU32", "Basic read bitPos");

    // Error: Not enough data (only 1 byte left)
    res = BEReadU32(&s, &out_u32);
    ASSERT_EQ_STREAM_RESULT(res, EOT_NOT_ENOUGH_DATA, "BEReadU32", "Not enough data for second read");

    // Error: Off byte boundary
    s = constructStream(buffer, sizeof(buffer));
    s.bitPos = 1;
    res = BEReadU32(&s, &out_u32);
    ASSERT_EQ_STREAM_RESULT(res, EOT_OFF_BYTE_BOUNDARY, "BEReadU32", "Off byte boundary");

    printf("test_BEReadU32_suite: PASSED\n");
    return 0;
}

// --- Test Suites for readNBits ---

// Helper function for comparative testing of readNBits
static int run_and_compare_readNBits_case(
    const char* suite_name, const char* case_name,
    uint8_t* buffer, unsigned buf_size,
    unsigned initial_pos, unsigned initial_bitPos,
    unsigned n_bits_to_read,
    enum StreamResult expected_res_opt, // Expected result for optimized (can be used as primary expected)
    uint32_t expected_val_opt // Expected value for optimized if EOT_STREAM_OK
) {
    struct Stream s_optimized, s_original;
    uint32_t out_optimized = 0, out_original = 0; // Initialize to 0
    enum StreamResult res_optimized, res_original;
    char test_desc[256];

    // Setup for optimized version
    s_optimized = constructStream(buffer, buf_size);
    s_optimized.pos = initial_pos;
    s_optimized.bitPos = initial_bitPos;

    // Setup for original version
    s_original = constructStream(buffer, buf_size);
    s_original.pos = initial_pos;
    s_original.bitPos = initial_bitPos;

    // Execute both versions
    res_optimized = readNBits(&s_optimized, &out_optimized, n_bits_to_read);
    res_original = readNBits_original_for_test(&s_original, &out_original, n_bits_to_read);

    // --- Assertions ---

    // 1. Compare results of optimized vs original
    snprintf(test_desc, sizeof(test_desc), "%s: %s - Result Opt vs Orig", suite_name, case_name);
    ASSERT_EQ_STREAM_RESULT(res_optimized, res_original, test_desc, "Return code Opt vs Orig");

    // 2. Compare optimized result against expected (this also implicitly means original should match this expected if previous assert passed)
    snprintf(test_desc, sizeof(test_desc), "%s: %s - Result Optimized", suite_name, case_name);
    ASSERT_EQ_STREAM_RESULT(res_optimized, expected_res_opt, test_desc, "Return code Optimized vs Expected");
    
    // 3. If both are EOT_STREAM_OK, compare output values and final stream states
    if (res_optimized == EOT_STREAM_OK && res_original == EOT_STREAM_OK) {
        snprintf(test_desc, sizeof(test_desc), "%s: %s - Output Opt vs Orig", suite_name, case_name);
        ASSERT_EQ_U32(out_optimized, out_original, test_desc, "Output value Opt vs Orig");

        snprintf(test_desc, sizeof(test_desc), "%s: %s - Output Optimized", suite_name, case_name);
        ASSERT_EQ_U32(out_optimized, expected_val_opt, test_desc, "Output value Optimized vs Expected");

        snprintf(test_desc, sizeof(test_desc), "%s: %s - Final Pos Opt vs Orig", suite_name, case_name);
        ASSERT_EQ_SIZE_T(s_optimized.pos, s_original.pos, test_desc, "Final stream pos Opt vs Orig");
        
        snprintf(test_desc, sizeof(test_desc), "%s: %s - Final BitPos Opt vs Orig", suite_name, case_name);
        ASSERT_EQ_SIZE_T(s_optimized.bitPos, s_original.bitPos, test_desc, "Final stream bitPos Opt vs Orig");
    }
    // If results are errors but match, state comparison is less critical but could be added if desired.
    // For now, matching error codes is the primary goal for error paths.

    return 0; // Indicates success for this specific case
}


int test_readNBits_param_validation_suite() {
    uint8_t buffer[] = {0xFF};
    uint32_t out_opt, out_orig; // Not strictly needed for n>32 here due to early exit
    enum StreamResult res_opt, res_orig;

    // n = 0 (Comparative)
    if (run_and_compare_readNBits_case("ParamValidation", "n=0", buffer, sizeof(buffer), 0, 0, 0, EOT_STREAM_OK, 0) != 0) return 1;
    
    // n > 32 (Original check for optimized, then check original behaves the same)
    struct Stream s_opt = constructStream(buffer, sizeof(buffer));
    res_opt = readNBits(&s_opt, &out_opt, 33);
    ASSERT_EQ_STREAM_RESULT(res_opt, EOT_VALUE_OUT_OF_BOUNDS, "ParamValidation", "n=33 optimized result");
    
    struct Stream s_orig = constructStream(buffer, sizeof(buffer));
    res_orig = readNBits_original_for_test(&s_orig, &out_orig, 33);
    ASSERT_EQ_STREAM_RESULT(res_orig, EOT_VALUE_OUT_OF_BOUNDS, "ParamValidation", "n=33 original result");
    ASSERT_EQ_STREAM_RESULT(res_opt, res_orig, "ParamValidation", "n=33 Opt vs Orig result");


    printf("test_readNBits_param_validation_suite: PASSED\n");
    return 0;
}

int test_readNBits_aligned_suite() {
    // Initial sequence
    if (run_and_compare_readNBits_case("Aligned", "1 bit from start", test_buf_simple, sizeof(test_buf_simple), 0, 0, 1, EOT_STREAM_OK, 1) != 0) return 1;
    // To test sequence correctly, we'd need to pass the stream state, or reset for each case.
    // The helper is designed for isolated cases. Re-evaluating sequence tests.
    // For now, individual calls to helper for each state.
    if (run_and_compare_readNBits_case("Aligned", "next 2 bits (initial_bitPos=1)", test_buf_simple, sizeof(test_buf_simple), 0, 1, 2, EOT_STREAM_OK, 1) != 0) return 1;
    if (run_and_compare_readNBits_case("Aligned", "next 3 bits (initial_bitPos=3)", test_buf_simple, sizeof(test_buf_simple), 0, 3, 3, EOT_STREAM_OK, 2) != 0) return 1;

    // Full byte/multi-byte reads from start
    if (run_and_compare_readNBits_case("Aligned", "8 bits from start", test_buf_simple, sizeof(test_buf_simple), 0, 0, 8, EOT_STREAM_OK, 0xAA) != 0) return 1;
    if (run_and_compare_readNBits_case("Aligned", "16 bits from start", test_buf_simple, sizeof(test_buf_simple), 0, 0, 16, EOT_STREAM_OK, 0xAABB) != 0) return 1;
    if (run_and_compare_readNBits_case("Aligned", "24 bits from start", test_buf_simple, sizeof(test_buf_simple), 0, 0, 24, EOT_STREAM_OK, 0xAABBCC) != 0) return 1;
    if (run_and_compare_readNBits_case("Aligned", "32 bits from start", test_buf_simple, sizeof(test_buf_simple), 0, 0, 32, EOT_STREAM_OK, 0xAABBCCDD) != 0) return 1;
    
    printf("test_readNBits_aligned_suite: PASSED\n");
    return 0;
}

int test_readNBits_unaligned_suite() {
    if (run_and_compare_readNBits_case("Unaligned", "bitPos=3, read 8 bits (all FF)", test_buf_all_ff, sizeof(test_buf_all_ff), 0, 3, 8, EOT_STREAM_OK, 0xFF) != 0) return 1;
    if (run_and_compare_readNBits_case("Unaligned", "bitPos=7, read 3 bits (0xAA,0xBB)", test_buf_simple, sizeof(test_buf_simple), 0, 7, 3, EOT_STREAM_OK, 0x02) != 0) return 1;
    if (run_and_compare_readNBits_case("Unaligned", "bitPos=1, read 16 bits (0xAA,0xBB,0xCC)", test_buf_simple, sizeof(test_buf_simple), 0, 1, 16, EOT_STREAM_OK, 0x5577) != 0) return 1;
    
    printf("test_readNBits_unaligned_suite: PASSED\n");
    return 0;
}

int test_readNBits_spanning_bytes_suite() {
    // Test 1: Read 4 bits from start of 0xA5 (10100101) -> expect 0x0A (1010)
    if (run_and_compare_readNBits_case("Spanning", "Span T1: 4b from start (0xA5)", test_buf_pattern, sizeof(test_buf_pattern), 0, 0, 4, EOT_STREAM_OK, 0x0A) != 0) return 1;
    // Test 2: From bitPos=4, read 8 bits. Spans 0xA5 and 0x5A. Expected: 0x55
    if (run_and_compare_readNBits_case("Spanning", "Span T2: 8b from bitPos=4 (0xA5,0x5A)", test_buf_pattern, sizeof(test_buf_pattern), 0, 4, 8, EOT_STREAM_OK, 0x55) != 0) return 1;
    // Test 3: From bitPos=4 (of 0x5A), read 10 bits. Spans 0x5A, 0xC3. Expected: 0x2B0
    // initial_pos needs to be 1 for this case based on previous state.
    if (run_and_compare_readNBits_case("Spanning", "Span T3: 10b from pos=1,bitPos=4 (0x5A,0xC3)", test_buf_pattern, sizeof(test_buf_pattern), 1, 4, 10, EOT_STREAM_OK, 0x2B0) != 0) return 1;
    
    printf("test_readNBits_spanning_bytes_suite: PASSED\n");
    return 0;
}

int test_readNBits_error_conditions_suite() {
    uint8_t buffer[] = {0xFF, 0xAA}; 
    uint32_t dummy_out = 0; // Value not checked for error cases, but needed for function call

    // EOT_NOT_ENOUGH_DATA
    if (run_and_compare_readNBits_case("Errors", "Empty stream, read 1 bit", buffer, 0, 0, 0, 1, EOT_NOT_ENOUGH_DATA, dummy_out) != 0) return 1;
    if (run_and_compare_readNBits_case("Errors", "1 byte, bitPos=0, read 9 bits", buffer, 1, 0, 0, 9, EOT_NOT_ENOUGH_DATA, dummy_out) != 0) return 1;
    if (run_and_compare_readNBits_case("Errors", "1 byte, bitPos=4, read 5 bits", buffer, 1, 0, 4, 5, EOT_NOT_ENOUGH_DATA, dummy_out) != 0) return 1;
    
    // Read exactly to the end of the stream (comparative part)
    if (run_and_compare_readNBits_case("Errors", "Read exact end (8 bits from 1 byte)", buffer, 1, 0, 0, 8, EOT_STREAM_OK, 0xFF) != 0) return 1;
    // Then try to read 1 more bit (comparative part for the error)
    if (run_and_compare_readNBits_case("Errors", "Read past end (1 bit after consuming 1 byte)", buffer, 1, 1, 0, 1, EOT_NOT_ENOUGH_DATA, dummy_out) != 0) return 1;

    if (run_and_compare_readNBits_case("Errors", "2 bytes, bitPos=0, read 17 bits", buffer, 2, 0, 0, 17, EOT_NOT_ENOUGH_DATA, dummy_out) != 0) return 1;

    printf("test_readNBits_error_conditions_suite: PASSED\n");
    return 0;
}

int test_readNBits_max_bits_suite() {
    // Read 32 bits, aligned
    if (run_and_compare_readNBits_case("MaxBits", "32 bits aligned", test_buf_simple, 4, 0, 0, 32, EOT_STREAM_OK, 0xAABBCCDD) != 0) return 1;

    // Read 32 bits, unaligned (start bitPos = 4)
    uint8_t unaligned_buf[] = {0xF0, 0xAA, 0xBB, 0xCC, 0xD0};
    if (run_and_compare_readNBits_case("MaxBits", "32 bits unaligned (start bitPos 4)", unaligned_buf, sizeof(unaligned_buf), 0, 4, 32, EOT_STREAM_OK, 0x0AABBCCD) != 0) return 1;

    printf("test_readNBits_max_bits_suite: PASSED\n");
    return 0;
}


int test_readNBits_sequence_suite() {
    // This suite is harder to adapt to the helper without modifying stream state externally
    // or making the helper more complex. For now, will test sequence with direct calls.
    
    struct Stream s_opt, s_orig;
    uint32_t out_opt, out_orig;
    enum StreamResult res_opt, res_orig;

    // Initial setup
    s_opt = constructStream(test_buf_pattern, sizeof(test_buf_pattern));
    s_orig = constructStream(test_buf_pattern, sizeof(test_buf_pattern));

    // 1. Read 3 bits from 0xA5 (10100101) -> expect 0x05 (101)
    res_opt = readNBits(&s_opt, &out_opt, 3);
    res_orig = readNBits_original_for_test(&s_orig, &out_orig, 3);
    ASSERT_EQ_STREAM_RESULT(res_opt, EOT_STREAM_OK, "Seq", "Seq1 res_opt");
    ASSERT_EQ_STREAM_RESULT(res_orig, res_opt, "Seq", "Seq1 res_orig vs res_opt");
    ASSERT_EQ_U32(out_opt, 0x05, "Seq", "Seq1 out_opt");
    ASSERT_EQ_U32(out_orig, out_opt, "Seq", "Seq1 out_orig vs out_opt");
    ASSERT_EQ_SIZE_T(s_opt.bitPos, 3, "Seq", "Seq1 s_opt.bitPos");
    ASSERT_EQ_SIZE_T(s_orig.bitPos, s_opt.bitPos, "Seq", "Seq1 s_orig.bitPos vs s_opt.bitPos");
    ASSERT_EQ_SIZE_T(s_opt.pos, 0, "Seq", "Seq1 s_opt.pos");
    ASSERT_EQ_SIZE_T(s_orig.pos, s_opt.pos, "Seq", "Seq1 s_orig.pos vs s_opt.pos");

    // 2. Read 7 bits. Expected: 0x15
    res_opt = readNBits(&s_opt, &out_opt, 7);
    res_orig = readNBits_original_for_test(&s_orig, &out_orig, 7);
    ASSERT_EQ_STREAM_RESULT(res_opt, EOT_STREAM_OK, "Seq", "Seq2 res_opt");
    ASSERT_EQ_STREAM_RESULT(res_orig, res_opt, "Seq", "Seq2 res_orig vs res_opt");
    ASSERT_EQ_U32(out_opt, 0x15, "Seq", "Seq2 out_opt");
    ASSERT_EQ_U32(out_orig, out_opt, "Seq", "Seq2 out_orig vs out_opt");
    ASSERT_EQ_SIZE_T(s_opt.bitPos, 2, "Seq", "Seq2 s_opt.bitPos"); // 3+7=10. 10%8 = 2
    ASSERT_EQ_SIZE_T(s_orig.bitPos, s_opt.bitPos, "Seq", "Seq2 s_orig.bitPos vs s_opt.bitPos");
    ASSERT_EQ_SIZE_T(s_opt.pos, 1, "Seq", "Seq2 s_opt.pos");
    ASSERT_EQ_SIZE_T(s_orig.pos, s_opt.pos, "Seq", "Seq2 s_orig.pos vs s_opt.pos");

    // 3. Read 12 bits. Expected: 0x6B0
    res_opt = readNBits(&s_opt, &out_opt, 12);
    res_orig = readNBits_original_for_test(&s_orig, &out_orig, 12);
    ASSERT_EQ_STREAM_RESULT(res_opt, EOT_STREAM_OK, "Seq", "Seq3 res_opt");
    ASSERT_EQ_STREAM_RESULT(res_orig, res_opt, "Seq", "Seq3 res_orig vs res_opt");
    ASSERT_EQ_U32(out_opt, 0x6B0, "Seq", "Seq3 out_opt");
    ASSERT_EQ_U32(out_orig, out_opt, "Seq", "Seq3 out_orig vs out_opt");
    ASSERT_EQ_SIZE_T(s_opt.bitPos, 6, "Seq", "Seq3 s_opt.bitPos"); // 2+12=14. 14%8 = 6
    ASSERT_EQ_SIZE_T(s_orig.bitPos, s_opt.bitPos, "Seq", "Seq3 s_orig.bitPos vs s_opt.bitPos");
    ASSERT_EQ_SIZE_T(s_opt.pos, 2, "Seq", "Seq3 s_opt.pos");
    ASSERT_EQ_SIZE_T(s_orig.pos, s_opt.pos, "Seq", "Seq3 s_orig.pos vs s_opt.pos");

    printf("test_readNBits_sequence_suite: PASSED\n");
    return 0;
}


int run_comprehensive_tests() {
    printf("Running comprehensive tests...\n");
    int failed = 0;

    failed |= test_BEReadU8_suite();
    failed |= test_BEReadU16_suite();
    failed |= test_BEReadU24_suite();
    failed |= test_BEReadU32_suite();

    failed |= test_readNBits_param_validation_suite();
    failed |= test_readNBits_aligned_suite();
    failed |= test_readNBits_unaligned_suite();
    failed |= test_readNBits_spanning_bytes_suite();
    failed |= test_readNBits_error_conditions_suite();
    failed |= test_readNBits_max_bits_suite();
    failed |= test_readNBits_sequence_suite();
    failed |= test_readNBits_additional_vectors_suite(); // Added new suite

    if (failed) {
        fprintf(stderr, "--- SOME COMPREHENSIVE TESTS FAILED ---\n");
    } else {
        printf("--- ALL COMPREHENSIVE TESTS PASSED ---\n");
    }
    return failed;
}


// Define a buffer size for benchmarks
#define BENCHMARK_BUFFER_SIZE (1024 * 1024 * 4) // 4MB
#define BENCHMARK_ITERATIONS 1000000

// Global buffer for benchmarks to avoid re-allocation
uint8_t* benchmark_buffer = NULL;

void setup_benchmark_buffer() {
    if (benchmark_buffer == NULL) {
        benchmark_buffer = (uint8_t*)malloc(BENCHMARK_BUFFER_SIZE);
        if (benchmark_buffer == NULL) {
            perror("Failed to allocate benchmark buffer");
            exit(1); // Exit if allocation fails
        }
        // Initialize buffer with some data
        for (size_t i = 0; i < BENCHMARK_BUFFER_SIZE; ++i) {
            benchmark_buffer[i] = (uint8_t)(i % 256);
        }
    }
}

void benchmark_BEReadU8() {
    // Ensure buffer is setup before use by benchmarks
    if (benchmark_buffer == NULL) setup_benchmark_buffer(); 
    struct Stream s = constructStream(benchmark_buffer, BENCHMARK_BUFFER_SIZE);
    uint8_t val;
    long iterations = BENCHMARK_ITERATIONS * 10; // BEReadU8 is fast, more iterations

    clock_t start_time = clock();
    for (long i = 0; i < iterations; ++i) {
        s.pos = 0; // Reset stream position
        BEReadU8(&s, &val);
    }
    clock_t end_time = clock();
    double cpu_time_used = ((double)(end_time - start_time)) / CLOCKS_PER_SEC;
    printf("BEReadU8 benchmark: %f seconds for %ld iterations\n", cpu_time_used, iterations);
}

void benchmark_BEReadU16() {
    // Ensure buffer is setup
    if (benchmark_buffer == NULL) setup_benchmark_buffer();
    struct Stream s = constructStream(benchmark_buffer, BENCHMARK_BUFFER_SIZE);
    uint16_t val;
    long iterations = BENCHMARK_ITERATIONS;

    clock_t start_time = clock();
    for (long i = 0; i < iterations; ++i) {
        s.pos = 0; // Reset stream position
        BEReadU16(&s, &val);
    }
    clock_t end_time = clock();
    double cpu_time_used = ((double)(end_time - start_time)) / CLOCKS_PER_SEC;
    printf("BEReadU16 benchmark: %f seconds for %ld iterations\n", cpu_time_used, iterations);
}

void benchmark_BEReadU24() {
    // Ensure buffer is setup
    if (benchmark_buffer == NULL) setup_benchmark_buffer();
    struct Stream s = constructStream(benchmark_buffer, BENCHMARK_BUFFER_SIZE);
    uint32_t val; // BEReadU24 reads into a uint32_t
    long iterations = BENCHMARK_ITERATIONS;

    clock_t start_time = clock();
    for (long i = 0; i < iterations; ++i) {
        s.pos = 0; // Reset stream position
        BEReadU24(&s, &val);
    }
    clock_t end_time = clock();
    double cpu_time_used = ((double)(end_time - start_time)) / CLOCKS_PER_SEC;
    printf("BEReadU24 benchmark: %f seconds for %ld iterations\n", cpu_time_used, iterations);
}

void benchmark_BEReadU32() {
    // Ensure buffer is setup
    if (benchmark_buffer == NULL) setup_benchmark_buffer();
    struct Stream s = constructStream(benchmark_buffer, BENCHMARK_BUFFER_SIZE);
    uint32_t val;
    long iterations = BENCHMARK_ITERATIONS;

    clock_t start_time = clock();
    for (long i = 0; i < iterations; ++i) {
        s.pos = 0; // Reset stream position
        BEReadU32(&s, &val);
    }
    clock_t end_time = clock();
    double cpu_time_used = ((double)(end_time - start_time)) / CLOCKS_PER_SEC;
    printf("BEReadU32 benchmark: %f seconds for %ld iterations\n", cpu_time_used, iterations);
}

void benchmark_readNBits() {
    // Ensure buffer is setup
    if (benchmark_buffer == NULL) setup_benchmark_buffer();
    struct Stream s = constructStream(benchmark_buffer, BENCHMARK_BUFFER_SIZE);
    uint32_t val;
    long iterations = BENCHMARK_ITERATIONS / 2; // NBit reads can be slower
    int bits_to_read[] = {1, 2, 3, 4, 5, 6, 7, 8, 12, 16, 24, 32}; // Added more cases
    int num_bit_tests = sizeof(bits_to_read) / sizeof(bits_to_read[0]);

    for (int j = 0; j < num_bit_tests; ++j) {
        int n_bits = bits_to_read[j];
        // Ensure buffer is large enough for N-bit reads, especially for 32-bit reads
        // which might need up to 8 bytes if bitPos is not 0.
        // s.pos reset handles this for each iteration.
        
        clock_t start_time = clock();
        for (long i = 0; i < iterations; ++i) {
            s.pos = 0;    // Reset byte position
            s.bitPos = 0; // Reset bit position
            readNBits(&s, &val, n_bits);
        }
        clock_t end_time = clock();
        double cpu_time_used = ((double)(end_time - start_time)) / CLOCKS_PER_SEC;
        printf("readNBits (%02d bits) benchmark: %f seconds for %ld iterations\n", n_bits, cpu_time_used, iterations);
    }
}

int run_original_tests() {
    printf("Test stream start\n");
    uint8_t buff[] = {1, 2, 3, 4, 5 , 6, 7, 8};
    unsigned size = 8;
    struct Stream sIn = constructStream(buff, size);
    uint8_t res = 0;
    int i = 0;
    for (i=0; i < size; i++){
        BEReadU8(&sIn,&res);
        printf("res: %d\n", res);
        if (res != buff[i]) {
            fprintf(stderr, "Original test failed: BEReadU8 mismatch.\n");
            return 1;
        }
    }
    printf("Original tests passed.\n");
    return 0;
}

int main() {
    int comprehensive_test_result = 0;
    int original_test_result = 0;

    original_test_result = run_original_tests();
    if (original_test_result != 0) {
        fprintf(stderr, "Original tests failed! Halting before comprehensive tests.\n");
        // No need to free benchmark_buffer, as it's not set up if original_tests are first
        return 1; 
    }
    printf("Original tests passed.\n\n");

    comprehensive_test_result = run_comprehensive_tests();
    if (comprehensive_test_result != 0) {
        fprintf(stderr, "Comprehensive tests failed! Halting before benchmarks.\n");
        // No need to free benchmark_buffer, as it's not set up if comprehensive_tests are first
        return 1;
    }
    printf("Comprehensive tests passed.\n\n");

    printf("Starting benchmarks...\n");
    // setup_benchmark_buffer() is now called defensively by each benchmark function 
    // or can be called once here if preferred, after tests.
    // For safety, defensive calls in benchmark functions are fine.
    // If setup_benchmark_buffer() was called by tests, it's also fine.
    // Let's ensure it's called once before all benchmarks for clarity.
    setup_benchmark_buffer(); 


    benchmark_BEReadU8();
    benchmark_BEReadU16();
    benchmark_BEReadU24();
    benchmark_BEReadU32();
    benchmark_readNBits();

    printf("Benchmarks finished.\n");
    
    if (benchmark_buffer != NULL) {
        free(benchmark_buffer); 
        benchmark_buffer = NULL;
    }

    return 0; // Success if all tests passed and benchmarks ran
}

// --- Test Suite for Additional readNBits Test Vectors ---
static int test_readNBits_additional_vectors_suite() {
    static const uint8_t buf0[] = {0x00, 0x00, 0x00, 0x00};
    static const uint8_t buf1[] = {0xFF, 0xFF, 0xFF, 0xFF};
    static const uint8_t bufA5[] = {0xA5, 0x5A, 0xA5, 0x5A}; // 10100101 01011010 ...
    static const uint8_t buf1234[] = {0x12, 0x34, 0x56, 0x78};
    static const uint8_t bufSingle[] = {0xF0}; // 11110000
    static const uint8_t bufTwo[] = {0xAA, 0xBB}; // 10101010 10111011
    uint32_t dummy_val = 0; // For error cases where value is ignored

    // 1. buf0, sz 4, pos 0, bitPos 0, n 1, OK, val 0
    if (run_and_compare_readNBits_case("AdditionalVectors", "TV1: 1b from 0x00 (pos0,bit0)", (uint8_t*)buf0, sizeof(buf0), 0, 0, 1, EOT_STREAM_OK, 0) != 0) return 1;
    // 2. buf1, sz 4, pos 0, bitPos 0, n 1, OK, val 1
    if (run_and_compare_readNBits_case("AdditionalVectors", "TV2: 1b from 0xFF (pos0,bit0)", (uint8_t*)buf1, sizeof(buf1), 0, 0, 1, EOT_STREAM_OK, 1) != 0) return 1;
    // 3. buf0, sz 4, pos 0, bitPos 0, n 8, OK, val 0x00
    if (run_and_compare_readNBits_case("AdditionalVectors", "TV3: 8b from 0x00 (pos0,bit0)", (uint8_t*)buf0, sizeof(buf0), 0, 0, 8, EOT_STREAM_OK, 0x00) != 0) return 1;
    // 4. buf1, sz 4, pos 0, bitPos 0, n 8, OK, val 0xFF
    if (run_and_compare_readNBits_case("AdditionalVectors", "TV4: 8b from 0xFF (pos0,bit0)", (uint8_t*)buf1, sizeof(buf1), 0, 0, 8, EOT_STREAM_OK, 0xFF) != 0) return 1;
    // 5. bufA5, sz 4, pos 0, bitPos 0, n 3, OK, val 0x05 (101 from 10100101)
    if (run_and_compare_readNBits_case("AdditionalVectors", "TV5: 3b from 0xA5 (pos0,bit0)", (uint8_t*)bufA5, sizeof(bufA5), 0, 0, 3, EOT_STREAM_OK, 0x05) != 0) return 1;
    // 6. bufA5, sz 4, pos 0, bitPos 3, n 5, OK, val 0x05 (00101 from 101(00101))
    if (run_and_compare_readNBits_case("AdditionalVectors", "TV6: 5b from 0xA5 (pos0,bit3)", (uint8_t*)bufA5, sizeof(bufA5), 0, 3, 5, EOT_STREAM_OK, 0x05) != 0) return 1;
    // 7. bufA5, sz 4, pos 0, bitPos 0, n 12, OK, val 0xA55 (10100101 0101 from 0xA5,0x5A)
    if (run_and_compare_readNBits_case("AdditionalVectors", "TV7: 12b from 0xA55A (pos0,bit0)", (uint8_t*)bufA5, sizeof(bufA5), 0, 0, 12, EOT_STREAM_OK, 0xA55) != 0) return 1;
    // 8. buf1234, sz 4, pos 0, bitPos 0, n 16, OK, val 0x1234
    if (run_and_compare_readNBits_case("AdditionalVectors", "TV8: 16b from 0x1234.. (pos0,bit0)", (uint8_t*)buf1234, sizeof(buf1234), 0, 0, 16, EOT_STREAM_OK, 0x1234) != 0) return 1;
    // 9. buf1234, sz 4, pos 1, bitPos 0, n 16, OK, val 0x3456
    if (run_and_compare_readNBits_case("AdditionalVectors", "TV9: 16b from 0x1234.. (pos1,bit0)", (uint8_t*)buf1234, sizeof(buf1234), 1, 0, 16, EOT_STREAM_OK, 0x3456) != 0) return 1;
    // 10. buf1234, sz 4, pos 0, bitPos 1, n 7, OK, val 0x12 ((0010010) from 0x12=00010010, starting at bit 1)
    // 0x12 = 00010010. bitPos=1 -> (0)001001(0). Read 7 bits -> 0010010 = 0x12
    if (run_and_compare_readNBits_case("AdditionalVectors", "TV10: 7b from 0x12 (pos0,bit1)", (uint8_t*)buf1234, sizeof(buf1234), 0, 1, 7, EOT_STREAM_OK, 0x12) != 0) return 1;
    // 11. buf1234, sz 4, pos 0, bitPos 0, n 32, OK, val 0x12345678
    if (run_and_compare_readNBits_case("AdditionalVectors", "TV11: 32b from 0x1234.. (pos0,bit0)", (uint8_t*)buf1234, sizeof(buf1234), 0, 0, 32, EOT_STREAM_OK, 0x12345678) != 0) return 1;
    // 12. bufSingle (0xF0), sz 1, pos 0, bitPos 0, n 4, OK, val 0x0F (1111 from 11110000)
    if (run_and_compare_readNBits_case("AdditionalVectors", "TV12: 4b from 0xF0 (pos0,bit0)", (uint8_t*)bufSingle, sizeof(bufSingle), 0, 0, 4, EOT_STREAM_OK, 0x0F) != 0) return 1;
    // 13. bufSingle (0xF0), sz 1, pos 0, bitPos 4, n 4, OK, val 0x00 (0000 from 11110000)
    if (run_and_compare_readNBits_case("AdditionalVectors", "TV13: 4b from 0xF0 (pos0,bit4)", (uint8_t*)bufSingle, sizeof(bufSingle), 0, 4, 4, EOT_STREAM_OK, 0x00) != 0) return 1;
    // 14. bufSingle (0xF0), sz 1, pos 0, bitPos 0, n 8, OK, val 0xF0
    if (run_and_compare_readNBits_case("AdditionalVectors", "TV14: 8b from 0xF0 (pos0,bit0)", (uint8_t*)bufSingle, sizeof(bufSingle), 0, 0, 8, EOT_STREAM_OK, 0xF0) != 0) return 1;
    // 15. buf0, sz 0, pos 0, bitPos 0, n 1, EOT_NOT_ENOUGH_DATA
    if (run_and_compare_readNBits_case("AdditionalVectors", "TV15: 1b from empty buf", (uint8_t*)buf0, 0, 0, 0, 1, EOT_NOT_ENOUGH_DATA, dummy_val) != 0) return 1;
    // 16. buf0, sz 0, pos 0, bitPos 0, n 8, EOT_NOT_ENOUGH_DATA
    if (run_and_compare_readNBits_case("AdditionalVectors", "TV16: 8b from empty buf", (uint8_t*)buf0, 0, 0, 0, 8, EOT_NOT_ENOUGH_DATA, dummy_val) != 0) return 1;
    // 17. bufSingle (0xF0), sz 1, pos 0, bitPos 0, n 9, EOT_NOT_ENOUGH_DATA
    if (run_and_compare_readNBits_case("AdditionalVectors", "TV17: 9b from 1-byte buf (pos0,bit0)", (uint8_t*)bufSingle, sizeof(bufSingle), 0, 0, 9, EOT_NOT_ENOUGH_DATA, dummy_val) != 0) return 1;
    // 18. bufSingle (0xF0), sz 1, pos 0, bitPos 4, n 5, EOT_NOT_ENOUGH_DATA
    if (run_and_compare_readNBits_case("AdditionalVectors", "TV18: 5b from 1-byte buf (pos0,bit4)", (uint8_t*)bufSingle, sizeof(bufSingle), 0, 4, 5, EOT_NOT_ENOUGH_DATA, dummy_val) != 0) return 1;
    // 19. bufSingle (0xF0), sz 1, pos 0, bitPos 2, n 6, OK, val 0x30 (110000 from 11(110000))
    if (run_and_compare_readNBits_case("AdditionalVectors", "TV19: 6b from 0xF0 (pos0,bit2)", (uint8_t*)bufSingle, sizeof(bufSingle), 0, 2, 6, EOT_STREAM_OK, 0x30) != 0) return 1;
    // 20. bufTwo (0xAA,0xBB), sz 2, pos 0, bitPos 5, n 11, OK, val 0x2BB
    // 0xAA = 10101(010) -> 010
    // 0xBB = (10111011) -> 10111011
    // result = (010)(10111011) = 01010111011 = 0x2BB
    if (run_and_compare_readNBits_case("AdditionalVectors", "TV20: 11b from 0xAABB (pos0,bit5)", (uint8_t*)bufTwo, sizeof(bufTwo), 0, 5, 11, EOT_STREAM_OK, 0x2BB) != 0) return 1;
    // 21. bufA5 (0xA5,0x5A,0xA5,0x5A), sz 4, pos 0, bitPos 0, n 24, OK, val 0xA55AA5
    if (run_and_compare_readNBits_case("AdditionalVectors", "TV21: 24b from 0xA55AA5.. (pos0,bit0)", (uint8_t*)bufA5, sizeof(bufA5), 0, 0, 24, EOT_STREAM_OK, 0xA55AA5) != 0) return 1;
    
    // 22. Test Vector 22 (Sequence)
    { 
        struct Stream s_opt = constructStream((uint8_t*)bufA5, sizeof(bufA5));
        struct Stream s_orig = constructStream((uint8_t*)bufA5, sizeof(bufA5));
        uint32_t out_opt, out_orig;
        enum StreamResult res_opt, res_orig;

        // Step 1: read 1 bit from 0xA5 (10100101), expect 1
        res_opt = readNBits(&s_opt, &out_opt, 1);
        res_orig = readNBits_original_for_test(&s_orig, &out_orig, 1);
        ASSERT_EQ_STREAM_RESULT(res_opt, res_orig, "AdditionalVectors: TV22.1", "Result Opt vs Orig");
        ASSERT_EQ_STREAM_RESULT(res_opt, EOT_STREAM_OK, "AdditionalVectors: TV22.1", "Result Optimized vs Expected");
        ASSERT_EQ_U32(out_opt, out_orig, "AdditionalVectors: TV22.1", "Output Opt vs Orig");
        ASSERT_EQ_U32(out_opt, 1, "AdditionalVectors: TV22.1", "Output Optimized vs Expected");
        ASSERT_EQ_SIZE_T(s_opt.pos, s_orig.pos, "AdditionalVectors: TV22.1", "Pos Opt vs Orig");
        ASSERT_EQ_SIZE_T(s_opt.bitPos, s_orig.bitPos, "AdditionalVectors: TV22.1", "BitPos Opt vs Orig");

        // Step 2: read 1 bit, expect 0
        res_opt = readNBits(&s_opt, &out_opt, 1);
        res_orig = readNBits_original_for_test(&s_orig, &out_orig, 1);
        ASSERT_EQ_STREAM_RESULT(res_opt, res_orig, "AdditionalVectors: TV22.2", "Result Opt vs Orig");
        ASSERT_EQ_STREAM_RESULT(res_opt, EOT_STREAM_OK, "AdditionalVectors: TV22.2", "Result Optimized vs Expected");
        ASSERT_EQ_U32(out_opt, out_orig, "AdditionalVectors: TV22.2", "Output Opt vs Orig");
        ASSERT_EQ_U32(out_opt, 0, "AdditionalVectors: TV22.2", "Output Optimized vs Expected");
        ASSERT_EQ_SIZE_T(s_opt.pos, s_orig.pos, "AdditionalVectors: TV22.2", "Pos Opt vs Orig");
        ASSERT_EQ_SIZE_T(s_opt.bitPos, s_orig.bitPos, "AdditionalVectors: TV22.2", "BitPos Opt vs Orig");

        // Step 3: read 1 bit, expect 1
        res_opt = readNBits(&s_opt, &out_opt, 1);
        res_orig = readNBits_original_for_test(&s_orig, &out_orig, 1);
        ASSERT_EQ_STREAM_RESULT(res_opt, res_orig, "AdditionalVectors: TV22.3", "Result Opt vs Orig");
        ASSERT_EQ_STREAM_RESULT(res_opt, EOT_STREAM_OK, "AdditionalVectors: TV22.3", "Result Optimized vs Expected");
        ASSERT_EQ_U32(out_opt, out_orig, "AdditionalVectors: TV22.3", "Output Opt vs Orig");
        ASSERT_EQ_U32(out_opt, 1, "AdditionalVectors: TV22.3", "Output Optimized vs Expected");
        ASSERT_EQ_SIZE_T(s_opt.pos, s_orig.pos, "AdditionalVectors: TV22.3", "Pos Opt vs Orig");
        ASSERT_EQ_SIZE_T(s_opt.bitPos, s_orig.bitPos, "AdditionalVectors: TV22.3", "BitPos Opt vs Orig");
    }

    // 23. bufTwo (0xAA,0xBB), sz 2, pos 1, bitPos 7, n 1, OK, val 1
    // 0xBB = 1011101(1). Read 1 bit -> 1
    if (run_and_compare_readNBits_case("AdditionalVectors", "TV23: 1b from 0xBB (pos1,bit7)", (uint8_t*)bufTwo, sizeof(bufTwo), 1, 7, 1, EOT_STREAM_OK, 1) != 0) return 1;
    // 24. bufTwo (0xAA,0xBB), sz 2, pos 1, bitPos 7, n 2, EOT_NOT_ENOUGH_DATA
    if (run_and_compare_readNBits_case("AdditionalVectors", "TV24: 2b from 0xBB (pos1,bit7)", (uint8_t*)bufTwo, sizeof(bufTwo), 1, 7, 2, EOT_NOT_ENOUGH_DATA, dummy_val) != 0) return 1;
    // 25. bufA5, sz 4, pos 0, bitPos 0, n 0, OK, val 0
    if (run_and_compare_readNBits_case("AdditionalVectors", "TV25: 0b (pos0,bit0)", (uint8_t*)bufA5, sizeof(bufA5), 0, 0, 0, EOT_STREAM_OK, 0) != 0) return 1;

    printf("test_readNBits_additional_vectors_suite: PASSED\n");
    return 0;
}