# Exact Round-Trip Mode Test Report

**Date**: 2025-10-03  
**Commit**: `a5b88178`  
**Feature**: Byte-perfect RIV round-trip reconstruction

---

## Test Overview

The exact round-trip mode enables byte-for-byte reconstruction of RIV files through the JSON intermediate format. This report validates the implementation with two test files:

1. **test.riv** - Minimal geometry file (856 KB)
2. **bee_baby.riv** - Production file with animations and state machines (9.7 KB)

---

## Test Results Summary

| Metric | test.riv | bee_baby.riv |
|--------|----------|--------------|
| **File Size** | 876,800 bytes | 9,700 bytes |
| **Artboard Objects** | 4 | 273 |
| **State Machines** | 1 (1 layer) | 1 (5 layers) |
| **Extract** | ✅ PASS | ✅ PASS |
| **Convert (--exact)** | ✅ PASS | ✅ PASS |
| **Binary Match** | ✅ IDENTICAL | ✅ IDENTICAL |
| **Import Test** | ✅ SUCCESS | ✅ SUCCESS |
| **NULL Objects** | 0 | 0 |
| **3-Cycle Stability** | ✅ STABLE | ✅ STABLE |

---

## Test 1: test.riv (Minimal Geometry)

### Commands Executed

```bash
# Step 1: Extract to exact JSON
./build_converter/converter/universal_extractor \
  converter/exampleriv/test.riv \
  output/tests/test_exact.json

# Step 2: Convert back to RIV
./build_converter/converter/rive_convert_cli --exact \
  output/tests/test_exact.json \
  output/tests/test_exact_roundtrip.riv

# Step 3: Binary comparison
cmp converter/exampleriv/test.riv output/tests/test_exact_roundtrip.riv

# Step 4: Import test
./build_converter/converter/import_test output/tests/test_exact_roundtrip.riv
```

### Results

- **File Size**: 876,800 bytes (original) → 876,800 bytes (roundtrip)
- **Binary Diff**: 0 bytes (identical)
- **Import Status**: SUCCESS
- **Objects**: 4 (1 Node, 1 Fill, 1 FillColor, 1 KeyedObject)
- **State Machines**: 1 with 1 layer (4 states)

### 3-Cycle Stability Test

```bash
./scripts/simple_stability_test.sh converter/exampleriv/test.riv
```

| Cycle | File Size | Objects | SHA-256 Hash |
|-------|-----------|---------|--------------|
| C2 | 876,800 bytes | 4 | `137cf9dd...61c652` |
| C4 | 876,800 bytes | 4 | `137cf9dd...61c652` |
| C6 | 876,800 bytes | 4 | `137cf9dd...61c652` |

**Verdict**: ✅ **FULLY STABLE** - Identical SHA-256 across all cycles

---

## Test 2: bee_baby.riv (Production File)

### Commands Executed

```bash
# Step 1: Extract to exact JSON
./build_converter/converter/universal_extractor \
  converter/exampleriv/bee_baby.riv \
  output/tests/bee_exact.json

# Step 2: Convert back to RIV
./build_converter/converter/rive_convert_cli --exact \
  output/tests/bee_exact.json \
  output/tests/bee_exact_roundtrip.riv

# Step 3: Binary comparison
cmp converter/exampleriv/bee_baby.riv output/tests/bee_exact_roundtrip.riv

# Step 4: Full round-trip test
./scripts/roundtrip_compare.sh converter/exampleriv/bee_baby.riv
```

### Results

- **File Size**: 9,700 bytes (original) → 9,700 bytes (roundtrip)
- **Binary Diff**: 0 bytes (identical)
- **Import Status**: SUCCESS
- **Artboards**: 2
  - Artboard #0: 500x500, 273 objects
  - Artboard #1: 0x0, 4 objects
- **State Machines**: 1 with 5 layers
  - Idle (4 states)
  - Bee (5 states)
  - Leaves Jump (5 states)
  - MouseClick (5 states)
  - MouseTrack (3 states)
- **NULL Objects**: 0

### 3-Cycle Stability Test

```bash
./scripts/simple_stability_test.sh converter/exampleriv/bee_baby.riv
```

| Cycle | File Size | Objects | SHA-256 Hash |
|-------|-----------|---------|--------------|
| C2 | 9,700 bytes | 273 | `19c377e8...8448d9a` |
| C4 | 9,700 bytes | 273 | `19c377e8...8448d9a` |
| C6 | 9,700 bytes | 273 | `19c377e8...8448d9a` |

**Verdict**: ✅ **FULLY STABLE** - Identical SHA-256 across all cycles

### Full Pipeline Test

```bash
./scripts/roundtrip_compare.sh converter/exampleriv/bee_baby.riv
```

**Results**:
- ✅ [1/5] Extract: SUCCESS
- ✅ [2/5] Convert with `--exact` flag: SUCCESS
- ✅ [3/5] Import test: PASS (0 NULL objects)
- ✅ [4/5] Binary comparison: Byte-for-byte identical
- ✅ [5/5] Growth analysis: 0.0% growth

**Final Verdict**: Round-trip successful - files match!

---

## CLI Flag Validation

### Test: Auto-Detect Mode (Without --exact flag)

```bash
./build_converter/converter/rive_convert_cli \
  output/tests/bee_exact.json \
  output/tests/bee_auto_detect.riv
```

**Output**:
```
⚠️  Warning: Exact mode JSON detected. Consider using --exact flag for clarity.
🌟 Detected UNIVERSAL exact stream - performing raw serialization
✅ Wrote RIV file: output/tests/bee_auto_detect.riv (9700 bytes)
```

**Result**: ✅ Warning shown correctly, file identical to original

### Test: --exact Flag with Non-Exact JSON

```bash
echo '{"artboards": [{"objects": [{"typeKey": 1}]}]}' > output/tests/non_exact.json
./build_converter/converter/rive_convert_cli --exact \
  output/tests/non_exact.json \
  output/tests/should_fail.riv
```

**Output**:
```
❌ Error: --exact flag requires JSON with __riv_exact__ = true
```

**Result**: ✅ Validation works as expected

---

## Key Features Validated

### 1. Extractor (`analyze_riv.py` + `universal_extractor.cpp`)
- ✅ Property types read from generated headers
- ✅ `componentIndex` added for diagnostics
- ✅ `objectTerminator` field preserves raw terminator bytes
- ✅ `tail` field captures post-stream catalog chunks

### 2. Serializer (`serializer.cpp`)
- ✅ ToC/bitmap validation against JSON
- ✅ 64-bit varuint serialization fixed
- ✅ Category-based value serialization:
  - uint → varuint
  - double → float64
  - color → color32
  - string → string
  - bytes → raw bytes
- ✅ Exact terminator/tail byte replication

### 3. CLI Interface (`main.cpp`)
- ✅ `--exact` flag for explicit mode
- ✅ Auto-detection with warning
- ✅ Validation when flag misused
- ✅ Backward compatibility

### 4. Test Scripts
- ✅ `roundtrip_compare.sh`: Auto-detects exact mode
- ✅ `simple_stability_test.sh`: 3-cycle validation
- ✅ Both scripts handle exact mode correctly

---

## Conclusion

The exact round-trip mode implementation is **production-ready** with:

- ✅ **Byte-perfect reconstruction** - 0 byte difference in all tests
- ✅ **Full stability** - Identical SHA-256 across multiple round-trip cycles
- ✅ **Catalog preservation** - Animations and state machines preserved
- ✅ **Runtime compatibility** - All files import successfully with 0 NULL objects
- ✅ **Robust CLI** - Flag validation and auto-detection working correctly

Both minimal geometry files and complex production files with animations pass all validation tests.

---

## Files Modified

1. `converter/src/main.cpp` - CLI `--exact` flag
2. `converter/src/serializer.cpp` - Exact serializer implementation
3. `converter/universal_extractor.cpp` - Exact JSON extraction
4. `converter/include/serializer.hpp` - Header updates
5. `converter/src/riv_structure.md` - Documentation
6. `scripts/roundtrip_compare.sh` - Auto-detect exact mode
7. `scripts/simple_stability_test.sh` - 3-cycle stability support
8. `AGENTS.md` - Workflow documentation

**Commit**: `a5b88178` - "feat: Add byte-perfect exact round-trip mode for RIV files"  
**Status**: Committed and pushed to main branch
