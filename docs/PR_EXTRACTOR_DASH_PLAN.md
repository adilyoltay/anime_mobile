# PR Plan: Extractor Dash/DashPath Support

**Tarih:** 2 Ekim 2025, 12:06  
**Status:** 🔴 READY FOR IMPLEMENTATION  
**Priority:** LOW (Opsiyonel - Round-trip testing için)  
**Total ETA:** 2-3 hours  
**Dependencies:** None (PR-ORPHAN-FIX already in main)

---

## 🎯 Executive Summary

Extractor şu anda Dash/DashPath objelerini extract edemiyor, bu yüzden round-trip testleri segfault ile başarısız oluyor. Bu PR, extractor'a minimal Dash/DashPath desteği ekleyecek.

**ÖNEMLİ:** Bu converter'ın sorunu DEĞİL! Converter zaten Dash/DashPath'i tam destekliyor. Bu sadece round-trip testing için gerekli.

---

## 🔬 Root Cause Analysis

### Current State (VERIFIED)

**Problem:**
```bash
# Cycle 1: JSON → RIV (converter)
./rive_convert_cli test_dash.json output.riv
→ ✅ SUCCESS (Dash/DashPath objects created)

# Cycle 2: RIV → JSON (extractor)
./universal_extractor output.riv output.json
→ ❌ Segmentation fault: 11
```

**Root Cause:**
```cpp
// universal_extractor.cpp:71-110 - getTypeName()
case 47: return "TrimPath";     // ✅ Has support
case 533: return "Feather";     // ✅ Has support
// case 507: MISSING!            // ❌ Dash
// case 506: MISSING!            // ❌ DashPath
```

**Impact:**
- Extractor encounters Dash (507) or DashPath (506)
- getTypeName() returns "Unknown"
- No extraction code for these types
- Segfault when trying to access properties

---

## 📋 Implementation Plan

### Phase 1: Add Type Names (5 min) ✅

**File:** `converter/universal_extractor.cpp`  
**Location:** Line 71-110 (getTypeName switch)

```cpp
// Add after line 108 (case 533: Feather)
case 507: return "Dash";
case 506: return "DashPath";
case 48: return "DrawTarget";  // Already in main via PR-DRAWTARGET
case 49: return "DrawRules";   // Already in main via PR-DRAWTARGET
```

**Why:** Prevents "Unknown" type warnings and helps debugging.

---

### Phase 2: Add Includes (5 min) ✅

**File:** `converter/universal_extractor.cpp`  
**Location:** After line 53 (existing paint includes)

```cpp
// Add after #include "rive/shapes/paint/feather.hpp"
#include "rive/shapes/paint/dash.hpp"
#include "rive/shapes/paint/dash_path.hpp"
#include "rive/draw_target.hpp"  // Already in main via PR-DRAWTARGET
#include "rive/draw_rules.hpp"   // Already in main via PR-DRAWTARGET
```

**Why:** Required to use dynamic_cast for type detection.

---

### Phase 3: Add Property Extraction (1 hour)

**File:** `converter/universal_extractor.cpp`  
**Location:** After Fill/Stroke extraction (around line 300-350)

#### 3.1: DashPath Extraction

```cpp
// Add after Stroke extraction
if (auto* dashPath = dynamic_cast<DashPath*>(comp)) {
    objInfo.properties["offset"] = dashPath->offset();
    objInfo.properties["offsetIsPercentage"] = dashPath->offsetIsPercentage();
    
    std::cout << "  [DashPath] localId=" << localId
              << " offset=" << dashPath->offset()
              << " offsetIsPercentage=" << dashPath->offsetIsPercentage()
              << std::endl;
}
```

**SDK Reference:**
```cpp
// include/rive/generated/shapes/paint/dash_path_base.hpp
float offset() const { return m_Offset; }
bool offsetIsPercentage() const { return m_OffsetIsPercentage; }
```

#### 3.2: Dash Extraction

```cpp
// Add after DashPath extraction
if (auto* dash = dynamic_cast<Dash*>(comp)) {
    objInfo.properties["length"] = dash->length();
    objInfo.properties["lengthIsPercentage"] = dash->lengthIsPercentage();
    
    std::cout << "  [Dash] localId=" << localId
              << " length=" << dash->length()
              << " lengthIsPercentage=" << dash->lengthIsPercentage()
              << std::endl;
}
```

**SDK Reference:**
```cpp
// include/rive/generated/shapes/paint/dash_base.hpp
float length() const { return m_Length; }
bool lengthIsPercentage() const { return m_LengthIsPercentage; }
```

---

### Phase 4: Test & Validate (30 min)

#### Test 1: Simple Dash Extraction
```bash
# Create test RIV with Dash/DashPath
./rive_convert_cli converter/test_dash.json output/test_dash.riv

# Extract it
./universal_extractor output/test_dash.riv output/test_dash_extracted.json

# Verify Dash/DashPath in JSON
jq '.artboards[0].objects | map(select(.typeKey == 507 or .typeKey == 506))' \
   output/test_dash_extracted.json

# Expected output:
# [
#   {
#     "typeKey": 506,
#     "typeName": "DashPath",
#     "properties": {
#       "offset": 0.0,
#       "offsetIsPercentage": false
#     }
#   },
#   {
#     "typeKey": 507,
#     "typeName": "Dash",
#     "properties": {
#       "length": 10.0,
#       "lengthIsPercentage": false
#     }
#   }
# ]
```

#### Test 2: Round-Trip Validation
```bash
# Full cycle
./rive_convert_cli test_dash.json rt1.riv
./universal_extractor rt1.riv rt2.json
./rive_convert_cli rt2.json rt3.riv

# Compare Dash counts
python3 converter/analyze_riv.py rt1.riv | grep "type_507\|type_506"
python3 converter/analyze_riv.py rt3.riv | grep "type_507\|type_506"

# Verify properties preserved
jq '.artboards[0].objects[] | select(.typeKey == 507) | .properties.length' rt2.json
# Expected: 10.0
```

---

## 📝 Success Criteria

### Must Have
- ✅ Extractor doesn't segfault on Dash/DashPath
- ✅ Dash properties extracted correctly (length, lengthIsPercentage)
- ✅ DashPath properties extracted correctly (offset, offsetIsPercentage)
- ✅ Round-trip test passes (JSON → RIV → JSON → RIV)

### Nice to Have
- ✅ Clean console output for Dash/DashPath
- ✅ Type names appear correctly in extracted JSON

---

## 🧪 Testing Strategy

### Unit Tests
```bash
# Test files available in converter/:
- test_dash.json (Shape with Dash/DashPath)
- test_gradient_with_path.json (includes Dash if updated)
```

### Integration Tests
```bash
# Run full validation suite after fix:
./scripts/validate_roundtrip.sh

# Expected:
# All 6 tests PASS (previously all failed)
```

---

## ⚠️ Known Limitations

### What This PR Does
- ✅ Adds Dash/DashPath extraction support
- ✅ Fixes extractor segfault
- ✅ Enables round-trip testing

### What This PR Does NOT Do
- ❌ Does NOT modify converter (already works)
- ❌ Does NOT add new Dash functionality
- ❌ Does NOT affect production rendering

---

## 🔄 Related Work

### Completed PRs
- ✅ **PR-DRAWTARGET**: Added DrawTarget/DrawRules extraction (main)
- ✅ **PR-ORPHAN-FIX**: Orphan paint auto-fix (main)
- ✅ **PR-VALIDATION**: Test suite (feature branch)

### This PR Enables
- ✅ Full round-trip validation suite
- ✅ Dash/DashPath preservation in conversions
- ✅ Complete property coverage

---

## 📊 Implementation Checklist

### Code Changes
- [ ] Add Dash/DashPath includes
- [ ] Add typeKey 507/506 to getTypeName()
- [ ] Add DashPath extraction code
- [ ] Add Dash extraction code

### Testing
- [ ] Test simple Dash extraction
- [ ] Test round-trip stability
- [ ] Verify property preservation
- [ ] Run full validation suite

### Documentation
- [ ] Update extractor documentation
- [ ] Document Dash/DashPath support
- [ ] Add round-trip test results

---

## 🚀 Timeline

**Total Time:** 2-3 hours

| Phase | Duration | Status |
|-------|----------|--------|
| Phase 1: Type Names | 5 min | 🔴 TODO |
| Phase 2: Includes | 5 min | 🔴 TODO |
| Phase 3: Extraction | 1 hour | 🔴 TODO |
| Phase 4: Testing | 30 min | 🔴 TODO |
| **Total** | **2-3 hours** | 🔴 **PENDING** |

---

## 💡 Implementation Notes

### Why This is Low Priority
1. Converter already works perfectly (proven by import_test)
2. Only affects round-trip testing (not production)
3. Extractor is separate from converter
4. Orphan fix is production-ready without this

### Why This is Still Useful
1. Enables full round-trip validation
2. Helps debugging conversions
3. Provides complete property coverage
4. Enables automated testing

### Design Decisions

**Q: Why not use generic property extraction?**  
A: Extractor uses type-specific dynamic_cast for safety and clarity.

**Q: Should we extract gap/space properties?**  
A: No, only public properties. Dash doesn't expose gap directly.

**Q: What about DashPath parent validation?**  
A: Extractor preserves hierarchy as-is; validation is converter's job.

---

## 📁 Files to Modify

```
converter/universal_extractor.cpp
  - Line 54: Add includes
  - Line 108: Add type names
  - Line 300+: Add extraction code
```

---

## 🎯 Expected Outcome

### Before
```bash
./scripts/validate_roundtrip.sh
→ ❌ ALL TESTS FAILED (6/6 - extractor segfault)
```

### After
```bash
./scripts/validate_roundtrip.sh
→ ✅ ALL TESTS PASSED (6/6 - clean extraction)
```

---

**Implementation Ready:** 2 Ekim 2025  
**Total Effort:** 2-3 hours  
**Priority:** LOW (Optional)  
**Confidence:** 99%+ (trivial addition)
