# RIV Validation Infrastructure

Complete validation and testing suite for Rive RIV file format.

## Overview

This infrastructure provides comprehensive validation for RIV binary format through a multi-layered testing approach:

1. **JSON Validation** - Input schema validation
2. **Format Compliance** - Binary structure validation
3. **Stream Integrity** - ToC/stream consistency
4. **Growth Tracking** - Size regression detection
5. **Import Testing** - Runtime compatibility
6. **CI/CD Integration** - Automated testing

---

## 🧪 Validation Tools

### 1. JSON Validator (`json_validator`)

**Purpose:** Validates JSON input before conversion

**Features:**
- Schema validation
- Parent reference checking
- Cycle detection
- Required property validation
- Forward reference detection

**Usage:**
```bash
./build_converter/converter/json_validator input.json
```

**Exit Codes:**
- `0` - Valid
- `1` - Validation failed
- `2` - Error (file not found, parse error)

---

### 2. RIV Structure Validator (`validate_roundtrip_riv.py`)

**Purpose:** Validates binary RIV file structure

**Features:**
- Magic header verification
- Version checking
- Chunk boundary validation
- ToC parsing
- Bitmap type code verification
- Object stream parsing
- Visual chunk distribution

**Usage:**
```bash
python3 scripts/validate_roundtrip_riv.py output.riv
```

**Output:**
```
======================================================================
RIV Structure Analysis
======================================================================

File Information:
  Magic: RIVE
  Size: 99 bytes
  Version: 7.0

Chunk Structure:
  HEADER     @  4  (   3 bytes,  3.0%) █
  TOC        @  7  (  15 bytes, 15.2%) ███████
  BITMAP     @ 22  (  12 bytes, 12.1%) ██████
  OBJECTS    @ 34  (  64 bytes, 64.6%) ████████████████████████████████
```

---

### 3. Stream Integrity Validator (`validate_stream_integrity.py`)

**Purpose:** Deep validation of RIV object stream

**Features:**
- ToC vs Stream consistency
- Property key usage validation
- Type bitmap verification
- Terminator counting
- VarUint truncation detection
- Property encoding validation

**Usage:**
```bash
python3 scripts/validate_stream_integrity.py output.riv
```

**Checks:**
- ✅ All keys in ToC are used in stream
- ✅ All keys in stream are in ToC
- ✅ Terminator count matches object count
- ✅ No truncated varuints
- ✅ Type codes match encoding

---

### 4. Binary Comparison Tool (`compare_riv_files.py`)

**Purpose:** Compare two RIV files byte-by-byte

**Features:**
- Side-by-side hex dump
- Color-coded differences
- Chunk-level comparison
- Property count tracking
- Type sequence validation
- JSON export

**Usage:**
```bash
# Visual comparison
python3 scripts/compare_riv_files.py file1.riv file2.riv

# JSON output
python3 scripts/compare_riv_files.py file1.riv file2.riv --json
```

**Output:**
```
Offset 0008 - 0018
────────────────────────────────────────────────────────────────────────────────
0008 file1    05 0d 0e 14 15 17 2c 81 01 cc 01 d4 01 00 a0 00
     file2    04 07 08 2c 37 38 39 3b c4 01 cc 01 d4 01 00 a4
```

---

### 5. Growth Tracker (`track_roundtrip_growth.py`)

**Purpose:** Track size growth in round-trip conversions

**Features:**
- File size comparison
- Object count tracking
- Header key analysis
- Threshold-based validation
- Growth percentage calculation

**Thresholds:**
- **PASS:** < 5% growth
- **WARN:** 5-10% growth
- **FAIL:** > 10% growth

**Usage:**
```bash
# Visual output
python3 scripts/track_roundtrip_growth.py original.riv roundtrip.riv

# JSON output for CI
python3 scripts/track_roundtrip_growth.py original.riv roundtrip.riv --json
```

**Exit Codes:**
- `0` - Pass (growth < 5%)
- `1` - Warning (growth 5-10%)
- `2` - Failure (growth > 10%)

---

### 6. Serializer Diagnostics (`serializer_diagnostics.hpp`)

**Purpose:** Runtime diagnostics for serialization process

**Features:**
- Chunk boundary tracking
- Offset logging
- Alignment checking
- Size variance detection
- Hierarchical output

**Usage:**
```bash
RIVE_SERIALIZE_VERBOSE=1 ./rive_convert_cli input.json output.riv
```

**Output:**
```
  ℹ️  Starting RIV serialization (serialize_core_document)
  🔢 Objects: 6
📦 HEADER @ offset 0
✅ HEADER complete: 7 bytes
📦 TOC @ offset 7 (expect ~22 bytes)
✅ TOC complete: 15 bytes (diff: -7)
  ⚠️ Bitmap NOT aligned to 4 bytes (offset 22, remainder 2)
```

---

## 🔄 Workflows

### Round-trip CI (`round_trip_ci.sh`)

**6-Step Validation Pipeline:**

```
[1/6] JSON validation          ✅
[2/6] RIV conversion            ✅
[3/6] Binary structure          ✅
[4/6] Stream integrity          ✅
[5/6] Chunk analysis            ✅
[6/6] Import test               ✅
```

**Usage:**
```bash
bash scripts/round_trip_ci.sh
```

**Test Files:**
- `test_189_no_trim.json` (189 objects)
- `test_190_no_trim.json` (190 objects - previous threshold)
- `test_273_no_trim.json` (273 objects)
- `bee_baby_NO_TRIMPATH.json` (1142 objects - full test)

---

### Round-trip Comparison (`roundtrip_compare.sh`)

**4-Step Workflow:**

```
[1/4] Extract RIV → JSON
[2/4] Convert JSON → RIV
[3/4] Compare files
[4/4] Track growth metrics
```

**Usage:**
```bash
bash scripts/roundtrip_compare.sh original.riv
```

**Artifacts Generated:**
- `{basename}_extracted.json` - Extracted JSON
- `{basename}_roundtrip.riv` - Round-trip RIV
- `{basename}_comparison.json` - Binary diff report
- `{basename}_growth.json` - Growth metrics

---

## 🤖 CI/CD Integration

### GitHub Actions

**Workflow:** `.github/workflows/roundtrip.yml`

**Triggers:**
- Push to `main` or `develop` branches
- Pull requests to `main` or `develop`
- Changes to converter, scripts, or workflows

**Jobs:**

1. **test-converter** (macOS)
   - Build all converter tools
   - Generate test files
   - Run full validation pipeline
   - Upload artifacts on failure
   - Generate test summary

2. **test-validator-standalone** (Ubuntu)
   - Build JSON validator
   - Run unit tests
   - Test error detection

**Artifacts:**
- Test outputs (RIV, JSON files)
- Analysis reports
- Retention: 7 days

---

## 📊 Metrics & Reporting

### Test Summary (GitHub Actions)

```markdown
## 🧪 Round-trip Test Results

### Summary
- ✅ Passed: 4
- ❌ Failed: 0

### Validation Steps
Each test file goes through:
1. ✅ JSON validation
2. ✅ RIV conversion
3. ✅ Binary structure validation
4. ✅ Stream integrity check
5. ✅ Chunk analysis
6. ✅ Import test

🎉 **All tests passed!**
```

### Growth Report

```
File Size:
  Original:   99 bytes
  Round-trip: 99 bytes
  Difference: +0 bytes (+0.00%)
  Status:     ✅ PASS

Growth Thresholds:
  Warning:  > 5%
  Fail:     > 10%
```

---

## 🛠️ Development Workflow

### Adding New Tests

1. Add test JSON to `output/tests/`
2. Update `round_trip_ci.sh` if needed
3. Run locally: `bash scripts/round_trip_ci.sh`
4. Commit and push - CI runs automatically

### Debugging Failures

1. **Check artifacts** - Download from GitHub Actions
2. **Run locally** with verbose:
   ```bash
   RIVE_SERIALIZE_VERBOSE=1 ./rive_convert_cli input.json output.riv
   ```
3. **Validate stream**:
   ```bash
   python3 scripts/validate_stream_integrity.py output.riv
   ```
4. **Compare files**:
   ```bash
   python3 scripts/compare_riv_files.py file1.riv file2.riv
   ```

---

## 📁 File Structure

```
scripts/
├── round_trip_ci.sh              # Main CI test runner
├── roundtrip_compare.sh          # Full comparison workflow
├── validate_roundtrip_riv.py     # Binary structure validator
├── validate_stream_integrity.py  # Stream integrity checker
├── compare_riv_files.py          # Binary diff tool
├── track_roundtrip_growth.py     # Growth tracker
└── analyze_roundtrip_growth.py   # JSON growth analyzer

converter/
├── include/
│   └── serializer_diagnostics.hpp  # Diagnostics framework
└── src/
    └── serializer.cpp              # With diagnostics integration

.github/workflows/
└── roundtrip.yml                 # CI/CD workflow
```

---

## ✅ Success Criteria

A test passes when:
- ✅ JSON schema is valid
- ✅ RIV file is generated
- ✅ Binary structure is correct
- ✅ Stream integrity verified
- ✅ Import test succeeds
- ✅ Growth < 5% (warning at 5-10%, fail > 10%)

---

## 🚨 Common Issues

### Issue: "Bitmap not 4-byte aligned"

**Cause:** RuntimeHeader::read() expects bitmap immediately after ToC terminator  
**Solution:** No padding between ToC and bitmap (per format spec)  
**Status:** ⚠️  Warning (not error) - this is expected behavior

### Issue: "Property key used in stream but not in ToC"

**Cause:** Property written to stream without adding to header  
**Solution:** Add key to `headerSet` before serialization  
**File:** `converter/src/serializer.cpp`

### Issue: "Unexpected EOF reading varuint"

**Cause:** Truncated LEB128 sequence in file  
**Solution:** File corruption or incomplete write  
**Action:** Regenerate file

### Issue: "Growth threshold exceeded"

**Cause:** File size increased >10% in round-trip  
**Solution:** Investigate chunk growth with `compare_riv_files.py`  
**Common:** Animation data expansion (expected, see AGENTS.md)

---

## 📚 References

- **Format Spec:** `converter/src/riv_structure.md`
- **Implementation Guide:** `docs/NEXT_SESSION_HIERARCHICAL.md`
- **Project Memory:** `AGENTS.md`
- **Runtime Header:** `include/rive/runtime_header.hpp`

---

## 🎯 Future Enhancements

- [ ] Property preservation validation
- [ ] Visual diff reports (HTML)
- [ ] Performance benchmarking
- [ ] Compression ratio tracking
- [ ] Memory usage profiling
- [ ] Cross-platform testing (Windows/Linux)

---

**Last Updated:** 2025-10-02  
**Status:** ✅ Production Ready
