# Round-Trip Instability: Root Cause Analysis

**Date:** 2025-10-02  
**Test File:** bee_baby.riv  
**Status:** 🚨 CRITICAL ISSUE FOUND

---

## Problem Summary

Round-trip conversion is **NOT stable** - object count grows with each cycle:

| Cycle | Total Objects | CubicInterpolator (28) | Interpolator (138) |
|-------|---------------|------------------------|---------------------|
| C1 (Original) | 1,142 | 315 | 55 |
| C3 (1 round-trip) | 1,253 | 420 | 69 |
| C5 (2 round-trips) | 1,291 | 454 | 74 |

**Growth:** +149 objects (+13%) after 2 cycles, primarily interpolators (+158 total)

---

## Root Cause

### Missing Property: interpolatorId (key 69)

**SDK Reference:** `include/rive/generated/animation/interpolating_keyframe_base.hpp`

```cpp
class InterpolatingKeyFrameBase : public KeyFrame {
    static const uint16_t interpolationTypePropertyKey = 68;
    static const uint16_t interpolatorIdPropertyKey = 69;  // ← MISSING in export!
    
protected:
    uint32_t m_InterpolationType = 0;
    uint32_t m_InterpolatorId = -1;  // ← References interpolator object
};
```

### Current Behavior

**Extractor (`universal_extractor.cpp`):**
- ✅ Exports interpolator objects (CubicEaseInterpolator, CubicValueInterpolator)
- ❌ Does NOT export `interpolatorId` in KeyFrame objects
- Result: KeyFrames lose reference to their interpolators

**Builder (`universal_builder.cpp`):**
- ✅ Creates interpolator objects from JSON
- ❌ Does NOT set `interpolatorId` on KeyFrames
- ❌ Creates NEW interpolators for each KeyFrame without an ID
- Result: Duplicate interpolators created every cycle

### Why It Grows

1. **Cycle 1:** Original RIV → JSON
   - 370 interpolators exported as standalone objects
   - KeyFrames have NO `interpolatorId` property

2. **Cycle 2:** JSON → RIV
   - Builder creates 370 interpolators from JSON
   - Builder sees KeyFrames without `interpolatorId`
   - Builder creates 119 NEW interpolators (default behavior)
   - **Total: 489 interpolators** (+32%)

3. **Cycle 3:** RIV → JSON → RIV
   - Process repeats
   - **Total: 528 interpolators** (+43% from original)

---

## Current Export (BROKEN)

**KeyFrame JSON (Missing interpolatorId):**
```json
{
  "localId": 100,
  "parentId": 99,
  "typeKey": 30,
  "typeName": "KeyFrameDouble",
  "properties": {
    "frame": 0,
    "seconds": 0.0,
    "value": 1.0
    // ❌ interpolatorId MISSING!
  }
}
```

---

## Required Fix

### 1. Extractor: Export interpolatorId

**File:** `converter/universal_extractor.cpp`

**Location:** KeyFrame export section (needs to be added)

```cpp
// In KeyFrame export
if (auto* interpKF = dynamic_cast<const InterpolatingKeyFrame*>(kf)) {
    uint32_t interpolatorId = interpKF->interpolatorId();
    if (interpolatorId != static_cast<uint32_t>(-1)) {
        // Remap to localId
        auto it = coreIdToLocalId.find(interpolatorId);
        if (it != coreIdToLocalId.end()) {
            objJson["properties"]["interpolatorId"] = it->second;
        }
    }
}
```

### 2. Builder: Import and Remap interpolatorId

**File:** `converter/src/universal_builder.cpp`

**Add to TypeMap:**
```cpp
{69, CoreUintType},  // interpolatorId
```

**Add to Property Wiring:** (PASS 3 - after object creation)
```cpp
// Remap interpolatorId references
if (typeKey == 30 || typeKey == 37 || ...) {  // KeyFrame types
    auto it = props.find("interpolatorId");
    if (it != props.end()) {
        uint32_t localId = std::get<uint32_t>(it->second);
        // Remap to component-local index
        auto remapIt = localComponentIndex.find(localId);
        if (remapIt != localComponentIndex.end()) {
            if (auto* interpKF = dynamic_cast<InterpolatingKeyFrame*>(obj)) {
                interpKF->interpolatorId(remapIt->second);
            }
        }
    }
}
```

---

## Impact Assessment

### Current State
- ❌ Round-trip NOT stable
- ❌ File size grows ~10% per cycle
- ❌ Object count grows ~10% per cycle
- ❌ Cannot achieve byte-identical output

### After Fix
- ✅ Round-trip STABLE
- ✅ Object count preserved
- ✅ File size stable
- ✅ Birebir aynılık sağlanabilir

---

## Test Verification

### Before Fix
```bash
C1: 1,142 objects → C3: 1,253 objects → C5: 1,291 objects
Interpolators: 370 → 489 → 528 (+42.7%)
```

### After Fix (Expected)
```bash
C1: 1,142 objects → C3: 1,142 objects → C5: 1,142 objects
Interpolators: 370 → 370 → 370 (STABLE)
```

---

## Priority

🔴 **HIGH PRIORITY** - Blocks production round-trip workflow

---

## Related Issues

- **PR-ROUNDTRIP-GROWTH-ANALYSIS:** File size 2x growth (partially due to this)
- **Animation Packer:** Alternative solution for animation data optimization

---

## Next Steps

1. ✅ **Immediate:** Implement interpolatorId export/import fix
2. ⏳ **Short-term:** Test on all production files (bee_baby, casino, etc.)
3. ⏳ **Long-term:** Consider animation packer for further optimization

---

**Root Cause:** Missing property key 69 (interpolatorId) in KeyFrame export/import  
**Impact:** Unstable round-trip, growing file size  
**Fix:** Add property 69 to extractor and builder with proper remapping
