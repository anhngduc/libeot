# Unit Test Specifications for MTX HDMX and MTX VDMX Parsing

This document outlines the specifications for unit tests designed to verify the parsing and conversion logic for MTX-compressed HDMX and VDMX tables into their standard OpenType formats.

## I. General Test Requirements

### 1. Test Harness
A C-based test program is required.
-   **Compilation & Linking**: It must include relevant libeot headers (`parseCTF.h`, `parseTTF.h` for `TTFmaxpData`, `libeot.h` for error codes, `stream.h`) and link against the compiled libeot library.
-   **Stream Simulation**: Must be able to provide test data as a `struct Stream`. This can be achieved by:
    *   Wrapping byte arrays defined in the C test code using `constructStreamFromBuffer()`.
    *   (Optionally, for more complex scenarios) Reading from small, well-defined binary files.
-   **Assertion Framework**: A simple assertion mechanism (e.g., `assert.h`) should be used to check test outcomes.
-   **Test Execution**: Each test case should be a separate function. A main function should call all test functions and report successes/failures.

### 2. Mock Data Setup
For each test case:
-   **`SFNTTable` Struct**:
    *   An `SFNTTable` instance (e.g., `hdmx_test_table`, `vdmx_test_table`) must be initialized.
    *   `tag`: Set to "hdmx" or "VDMX".
    *   `offset`: Typically `0` if the test stream contains only the data for that specific table.
    *   `bufSize`: Set to the size of the input MTX-compressed byte array.
    *   `buf`: Initialized to `NULL`. The `populateHdmx`/`populateVdmx` function is expected to allocate and populate this with the converted standard OpenType table.
-   **`TTFmaxpData` Struct (for HDMX tests)**:
    *   A `TTFmaxpData` instance (e.g., `mock_maxp_data`) must be populated.
    *   `numGlyphs`: Set to a value appropriate for the specific HDMX test data being used (e.g., if a DeviceRecord in the test data has 10 advance widths, `numGlyphs` should be 10).
-   **Input Stream (`struct Stream`)**:
    *   Initialized from a `uint8_t` array containing the raw bytes of the MTX-compressed test table.

## II. Test Cases for MTX HDMX Parsing (`populateHdmx`)

### A. Valid MTX HDMX Data

1.  **Test Case: Minimal Valid HDMX Table**
    *   **Input MTX Data**:
        *   `MTXTableVersionInfo`: `majorVersion=1`, `minorVersion=0`, `tableFormat=1`.
        *   `HdmxControlByte`: `0x00` (no resolution records).
        *   One `DeviceRecord`:
            *   `pixelSize` (MTX, u16): e.g., `12` (becomes `0x0C` as u8).
            *   `maxWidth` (MTX, u16): e.g., `100` (becomes `0x64` as u8).
            *   `AdvanceWidths` (MTX, u16[]): For `numGlyphs` (e.g., 2 glyphs): `[50, 60]` (becomes `[0x32, 0x3C]` as u8[]).
    *   **`TTFmaxpData`**: `numGlyphs = 2`.
    *   **Expected Outcome**:
        *   `populateHdmx` returns `EOT_SUCCESS`.
        *   `hdmx_test_table.buf` is not `NULL`.
        *   `hdmx_test_table.bufSize` is correct for the standard HDMX table generated.
        *   **Verification of `hdmx_test_table.buf`**:
            *   Standard `HdmxHeader`:
                *   `version`: `0`.
                *   `numRecords`: `1`.
                *   `sizeDeviceRecord`: `(2 + numGlyphs + 3) & ~3`.
            *   Standard `DeviceRecord[0]`:
                *   `pixelSize`: `12`.
                *   `maxWidth`: `100`.
                *   `widths[0]`: `50`.
                *   `widths[1]`: `60`.
                *   Correct padding bytes (0s) at the end of the record.

2.  **Test Case: HDMX with Resolution Records**
    *   **Input MTX Data**:
        *   `MTXTableVersionInfo`: `majorVersion=1`, `minorVersion=1`, `tableFormat=1`.
        *   `HdmxControlByte`: `0xC0` (has X and Y resolution records).
        *   X Resolution Records: e.g., count `1`, value `96`.
        *   Y Resolution Records: e.g., count `1`, value `96`.
        *   One `DeviceRecord` as above.
    *   **`TTFmaxpData`**: `numGlyphs = 2`.
    *   **Expected Outcome**:
        *   `EOT_SUCCESS`.
        *   Resolution records are parsed and skipped from the input stream.
        *   Standard HDMX table content is identical to the "Minimal Valid" case (as resolution records are not stored in the standard output).

3.  **Test Case: HDMX Data Truncation**
    *   **Input MTX Data**:
        *   One `DeviceRecord`:
            *   `pixelSize` (MTX, u16): `300` (exceeds 255).
            *   `maxWidth` (MTX, u16): `400` (exceeds 255).
            *   `AdvanceWidths` (MTX, u16[]): For `numGlyphs = 1`: `[500]` (exceeds 255).
    *   **`TTFmaxpData`**: `numGlyphs = 1`.
    *   **Expected Outcome**:
        *   `EOT_SUCCESS`.
        *   Warnings for truncation should be logged by the function (test harness might not check logs directly, but behavior should be as if warnings occurred).
        *   Standard `DeviceRecord[0]`:
            *   `pixelSize`: `(uint8_t)300` (i.e., `44`).
            *   `maxWidth`: `(uint8_t)400` (i.e., `144`).
            *   `widths[0]`: `(uint8_t)500` (i.e., `244`).

4.  **Test Case: Multiple Device Records**
    *   **Input MTX Data**: Two or more valid `DeviceRecord` entries.
    *   **`TTFmaxpData`**: `numGlyphs` matching the records.
    *   **Expected Outcome**:
        *   `EOT_SUCCESS`.
        *   Standard `HdmxHeader.numRecords` reflects the count of device records.
        *   All device records are correctly converted and present in `hdmx_test_table.buf`.

### B. Malformed MTX HDMX Data / Error Handling

1.  **Test Case: Invalid MTX Version/Format**
    *   **Input MTX Data**:
        *   `MTXTableVersionInfo`: `majorVersion=2` (invalid), or `tableFormat=2` (invalid for HDMX).
    *   **Expected Outcome**: `EOT_HDMX_MTX_CORRUPT`. `hdmx_test_table.buf` remains `NULL`.

2.  **Test Case: Premature Stream End**
    *   **Input MTX Data**: A valid MTX HDMX header, but the byte stream is truncated mid-DeviceRecord or mid-AdvanceWidths.
    *   **Expected Outcome**: `EOT_HDMX_MTX_CORRUPT` (or `EOT_STREAM_UNEXPECTED_END` if underlying stream functions return that, which then should be mapped by `populateHdmx` to `EOT_HDMX_MTX_CORRUPT`). `hdmx_test_table.buf` is `NULL`.

3.  **Test Case: Inconsistent MTX Data (if detectable by `populateHdmx`)**
    *   E.g., if MTX format had its own `numGlyphs` per record and it mismatched `maxpData->numGlyphs`. (Current `populateHdmx` relies on `maxpData->numGlyphs` for the loop).
    *   E.g., trailing data after all records have been supposedly read according to `hdmxTable->bufSize`.
    *   **Expected Outcome**: `EOT_HDMX_MTX_CORRUPT`.

### C. Edge Cases for HDMX

1.  **Test Case: `numGlyphs = 0`**
    *   **`TTFmaxpData`**: `numGlyphs = 0`.
    *   **Input MTX Data**: Minimal header, `HdmxControlByte=0x00`. (No device records expected or possible if `numGlyphs` is 0, as `standardSizeDeviceRecord` would depend on it). The `populateHdmx` function currently returns `EOT_SUCCESS` with `buf=NULL`, `bufSize=0` if `numGlyphs` is 0. This is an acceptable defined behavior.
    *   **Expected Outcome**: `EOT_SUCCESS`, `hdmx_test_table.buf == NULL`, `hdmx_test_table.bufSize == 0`.

2.  **Test Case: Zero Device Records in MTX stream**
    *   **Input MTX Data**: Valid `MTXTableVersionInfo`, `HdmxControlByte`, but stream ends immediately after (i.e. `originalMtxTableSize` implies no device records).
    *   **`TTFmaxpData`**: `numGlyphs > 0`.
    *   **Expected Outcome**:
        *   `EOT_SUCCESS`.
        *   Standard `HdmxHeader.numRecords = 0`.
        *   `hdmx_test_table.bufSize` should be just the size of the standard header (8 bytes).

## III. Test Cases for MTX VDMX Parsing (`populateVdmx`)

### A. Valid MTX VDMX Data

1.  **Test Case: Minimal Valid VDMX Table (Ratio Flags `00b` - No Ratios)**
    *   **Input MTX Data**:
        *   `MTXTableVersionInfo`: `majorVersion=1`, `minorVersion=0`, `tableFormat=2`.
        *   `VdmxControlByte`: `0x00` (no resolution records, ratioFlags = `00b`).
        *   (No further data as no ratios/groups are defined).
    *   **Expected Outcome**:
        *   `populateVdmx` returns `EOT_SUCCESS`.
        *   `vdmx_test_table.buf` is not `NULL`.
        *   **Verification of `vdmx_test_table.buf`**:
            *   Standard `VdmxHeader`: `version=1`, `numRecs=0`, `numRatios=0`.
            *   `vdmx_test_table.bufSize` is `sizeof(uint16_t)*3 = 6`. (No RatioRange array, no offset array).

2.  **Test Case: Valid VDMX Table (Ratio Flags `01b` - 1:1 Ratio)**
    *   **Input MTX Data**:
        *   `MTXTableVersionInfo`: `majorVersion=1`, `minorVersion=0`, `tableFormat=2`.
        *   `VdmxControlByte`: `0x01` (no res records, ratioFlags = `01b`).
        *   One `VdmxGroup`: `recs=1`, `startszMTX=10`, `endszMTX=10`.
            *   One `vTable` entry: `yPelHeight=10`, `yMax=8`, `yMin=-2`.
    *   **Expected Outcome**:
        *   `EOT_SUCCESS`.
        *   Standard `VdmxHeader`: `version=1`, `numRecs=1`, `numRatios=1`.
        *   `RatioRange[0]`: `{bCharSet=1, xRatio=1, yStartRatio=1, yEndRatio=1}`.
        *   `GroupOffset[0]`: Points to the start of the single `VDMXGroup` table that follows.
        *   Standard `VDMXGroup[0]`: `recs=1`, `startsz=10`, `endsz=10`.
            *   `vTable[0]`: `yPelHeight=10`, `yMax=8`, `yMin=-2`.
        *   Correct `vdmx_test_table.bufSize`.

3.  **Test Case: Valid VDMX Table (Ratio Flags `10b` - 1:1 & 2:1 Ratios)**
    *   **Input MTX Data**:
        *   `MTXTableVersionInfo`: `majorVersion=1`, `minorVersion=0`, `tableFormat=2`.
        *   `VdmxControlByte`: `0x02` (no res records, ratioFlags = `10b`).
        *   Two `VdmxGroup`s, one for each ratio.
    *   **Expected Outcome**:
        *   `EOT_SUCCESS`.
        *   `VdmxHeader`: `numRecs=2`, `numRatios=2`.
        *   `RatioRange[0]`: `{1,1,1,1}`, `RatioRange[1]`: `{1,2,1,1}`.
        *   Two group offsets, pointing to their respective `VDMXGroup` tables.
        *   Two correctly converted `VDMXGroup` tables.

4.  **Test Case: Valid VDMX Table (Ratio Flags `11b` - Explicit Ratios)**
    *   **Input MTX Data**:
        *   `MTXTableVersionInfo`: `majorVersion=1`, `minorVersion=0`, `tableFormat=2`.
        *   `VdmxControlByte`: `0x03` (no res records, ratioFlags = `11b`).
        *   `mtxNumRatioRecords` (u16, e.g., `1`).
        *   MTX Ratio Record: `xR=3`, `yR=2`.
        *   One `VdmxGroup` corresponding to this ratio.
    *   **Expected Outcome**:
        *   `EOT_SUCCESS`.
        *   `VdmxHeader`: `numRecs=1`, `numRatios=1`.
        *   `RatioRange[0]`: `{bCharSet=1, xRatio=3, yStartRatio=2, yEndRatio=2}`.
        *   Correctly converted `VDMXGroup`.

5.  **Test Case: VDMX Data Truncation for `startsz`/`endsz`**
    *   **Input MTX Data**: `VdmxGroup` with `startszMTX=300`, `endszMTX=400`.
    *   **Expected Outcome**:
        *   `EOT_SUCCESS`.
        *   Warnings logged for truncation.
        *   Standard `VDMXGroup`: `startsz=(uint8_t)300`, `endsz=(uint8_t)400`.

### B. Malformed MTX VDMX Data / Error Handling

1.  **Test Case: Invalid MTX Version/Format**
    *   **Input MTX Data**: `MTXTableVersionInfo` with `majorVersion=2` or `tableFormat=1`.
    *   **Expected Outcome**: `EOT_VDMX_MTX_CORRUPT`. `vdmx_test_table.buf` is `NULL`.

2.  **Test Case: Premature Stream End**
    *   E.g., Stream ends mid-RatioRecord (for flags `11b`), mid-VdmxGroup header, or mid-vTable entries.
    *   **Expected Outcome**: `EOT_VDMX_MTX_CORRUPT`.

3.  **Test Case: Inconsistent Data**
    *   `ratioFlags=11b`, `mtxNumRatioRecords > 0`, but not enough data in stream for the declared number of VdmxGroups.
    *   MTX stream has trailing data after all declared records and groups are parsed.
    *   **Expected Outcome**: `EOT_VDMX_MTX_CORRUPT`.

4.  **Test Case: Group Offset Exceeds `UINT16_MAX`**
    *   Craft a scenario (many ratio ranges, many vtables) where the calculated offset for a VDMXGroup within `sOut` would exceed `UINT16_MAX`.
    *   **Expected Outcome**: `EOT_VDMX_MTX_CORRUPT` (as standard VDMX uses `uint16_t` for these offsets).

### C. Edge Cases for VDMX

1.  **Test Case: `VdmxControlByte` ratioFlags `11b` with `mtxNumRatioRecords = 0`**
    *   **Input MTX Data**: `MTXTableVersionInfo` (valid for VDMX), `VdmxControlByte` (ratioFlags `11b`), `mtxNumRatioRecords = 0`. No further group data.
    *   **Expected Outcome**:
        *   `EOT_SUCCESS`.
        *   Standard `VdmxHeader`: `version=1`, `numRecs=0`, `numRatios=0`.
        *   `vdmx_test_table.bufSize` should be `sizeof(uint16_t)*3 = 6`.

2.  **Test Case: `VdmxGroup` with `recs = 0`**
    *   **Input MTX Data**: Valid setup with one ratio and one group, but that group has `recs=0`.
    *   **Expected Outcome**:
        *   `EOT_SUCCESS`.
        *   Standard `VDMXGroup` should have `recs=0`, and no vTable entries following it.
        *   The size of this group in `sOut` should be just the header size (`sizeof(uint16_t) + 2 * sizeof(uint8_t)`).

## IV. Pseudocode for Test Function Structure

```c
// Example Test Function Structure (HDMX)
// Replace with specific details for each test case.

void test_hdmx_case_name() {
    // 1. SETUP
    // Define MTX HDMX byte array for the specific test case
    uint8_t mtx_hdmx_data[] = { /* ... test-specific bytes ... */ };
    struct Stream mtx_stream = constructStreamFromBuffer(mtx_hdmx_data, sizeof(mtx_hdmx_data));

    // Initialize SFNTTable for the hdmx table
    struct SFNTTable hdmx_test_table;
    memset(&hdmx_test_table, 0, sizeof(struct SFNTTable)); // Zero out initially
    // strcpy(hdmx_test_table.tag, "hdmx"); // Not strictly needed for populateHdmx
    hdmx_test_table.offset = 0; 
    hdmx_test_table.bufSize = sizeof(mtx_hdmx_data); // Original MTX size
    hdmx_test_table.buf = NULL; 

    // Initialize TTFmaxpData
    struct TTFmaxpData mock_maxp_data;
    mock_maxp_data.numGlyphs = /* value appropriate for mtx_hdmx_data */;

    enum EOTError expected_error = /* EOT_SUCCESS or specific error code */;
    
    // 2. EXECUTION
    enum EOTError actual_error = populateHdmx(&hdmx_test_table, &mock_maxp_data, &mtx_stream);

    // 3. ASSERTION
    assert(actual_error == expected_error);

    if (expected_error == EOT_SUCCESS) {
        assert(hdmx_test_table.buf != NULL);
        assert(hdmx_test_table.bufSize > 0); // Or a more specific expected size

        // Deep Verification of hdmx_test_table.buf:
        // - Create a stream from hdmx_test_table.buf:
        //   struct Stream std_hdmx_stream = constructStreamFromBuffer(hdmx_test_table.buf, hdmx_test_table.bufSize);
        // - Read and verify standard HdmxHeader fields (version, numRecords, sizeDeviceRecord).
        // - Loop numRecords times:
        //   - Read and verify standard DeviceRecord fields (pixelSize, maxWidth).
        //   - Loop numGlyphs times: Read and verify advance widths.
        //   - Verify padding if any.
        // - Ensure stream is fully consumed if all data is checked.
    } else {
        assert(hdmx_test_table.buf == NULL); // Buffer should not be allocated on error
    }

    // 4. CLEANUP
    if (hdmx_test_table.buf) {
        free(hdmx_test_table.buf);
    }
    // freeStream(&mtx_stream); // If constructStreamFromBuffer allocates internal resources
}

// Similar structure for test_vdmx_case_name(), adjusting mock data and verification logic.
```

## V. Conclusion
These test specifications provide a comprehensive (though not exhaustive) set of scenarios to validate the MTX to standard OpenType HDMX/VDMX conversion logic. Actual byte arrays for input data need to be carefully crafted based on MTX and standard OpenType specifications.
