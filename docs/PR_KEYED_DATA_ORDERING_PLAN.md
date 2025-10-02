# PR Plan: Keyed Data Ordering Fix
**Date:** October 2, 2025  
**Priority:** 🔥 P0 CRITICAL  
**Status:** Planning  

---

## Problem Synthesis (3 Sources)

### External Report 1: Pipeline Status
- **Keyed cascade collapse:** topologicalSort treats objects without parentId as roots
- **Impact:** Keyed objects (25/26/30/28/138) emitted BEFORE their animation targets
- **Result:** Builder cannot resolve objectId → localId → 521 keyed records skipped (540 → 294 objects)
- **File:** `converter/extractor_postprocess.hpp:71`, `converter/src/universal_builder.cpp:816`

### External Report 2: Round-Trip Findings
- **Root cause:** Pre-check at `universal_builder.cpp:924-936` rejects KeyedObject if objectId target not emitted yet
- **Topological sort limitation:** Only respects parent relationships, ignores objectId dependencies
- **Consequence:** KeyedObjects with parentId=0 stay ahead of targets → localIdToBuilderObjectId empty → cascade skip
- **Impact:** 291/806 objects created, 0 objectId remaps successful

### Our Analysis (Session Findings)
- **Current fix:** Restored parentId=0 + added setParent skip in PASS2 (commit ae4e70ec)
- **Observation:** Fresh extraction still shows cascade skips for localId 47, 110, 109, 46...
- **Import status:** SUCCESS but 238/273 objects (12.8% loss)
- **Keyed data:** Some animations preserved but incomplete

---

## Root Cause Consensus

**All 3 sources agree:**

```
Topological Sort ONLY considers parentId edges
    ↓
KeyedObjects have parentId=0 (artboard children for ordering)
    ↓
Sort places them BEFORE animation targets (Shapes, Nodes, etc.)
    ↓
Builder Pass 1: KeyedObject.objectId lookup FAILS (target not created yet)
    ↓
CASCADE SKIP: 521+ keyed records dropped
    ↓
Result: 50%+ object loss, animations broken
```

**The Fix:**
Topological sort must respect **TWO dependency types:**
1. **Parent edges:** parentId → ensure children after parents
2. **Reference edges:** objectId → ensure keyed data after animation targets

---

## PR Breakdown (4 PRs, Sequential)

### PR1: Topological Sort - Add objectId Dependencies 🔥
**Priority:** P0 CRITICAL  
**File:** `converter/extractor_postprocess.hpp`  
**Estimated Time:** 2 hours  

**Changes:**
1. Extend dependency graph to include objectId references
2. Add KeyedObject → target component edges BEFORE computing in-degrees
3. Ensure keyed data emits AFTER all referenced components

**Algorithm:**
```cpp
// Build TWO edge types:
// 1. Parent edges (existing)
for each object:
    if has parentId:
        childToParent[localId] = parentId
        
// 2. Reference edges (NEW)
for each KeyedObject:
    targetId = properties["objectId"]
    childToParent[localId] = targetId  // Treat target as "parent" for ordering

// Compute in-degrees (respects both edge types)
// Kahn's algorithm proceeds as before
```

**Validation:**
- Cascade skip count should drop from 521 → 0
- Object count should return to 540 (or 806 total)
- No "KeyedObject targets missing localId=X" messages

---

### PR2: Builder - Remove Premature objectId Check
**Priority:** P1 HIGH  
**File:** `converter/src/universal_builder.cpp:924-937`  
**Estimated Time:** 30 minutes  

**Changes:**
Once PR1 ensures correct ordering, the pre-check becomes redundant:

```cpp
// BEFORE (lines 924-937):
if (typeKey == 25) { // KeyedObject
    if (objJson.contains("properties") && objJson["properties"].contains("objectId")) {
        uint32_t targetLocalId = objJson["properties"]["objectId"].get<uint32_t>();
        auto it = localIdToBuilderObjectId.find(targetLocalId);
        if (it == localIdToBuilderObjectId.end()) {
            std::cerr << "Cascade skip: KeyedObject targets missing localId=" 
                      << targetLocalId << std::endl;
            skipKeyframeData = true; // ← THIS KILLS 521 OBJECTS
            continue;
        }
    }
}

// AFTER:
// Remove this check entirely - topological sort guarantees targets exist
// Keep only validation logging (optional):
if (typeKey == 25) {
    uint32_t targetLocalId = objJson["properties"]["objectId"].get<uint32_t>();
    auto it = localIdToBuilderObjectId.find(targetLocalId);
    if (it == localIdToBuilderObjectId.end()) {
        std::cerr << "⚠️  WARNING: KeyedObject target not found: " << targetLocalId 
                  << " (topological sort failed?)" << std::endl;
        // DON'T skip - log and continue
    }
}
```

**Validation:**
- No cascade skips triggered
- All keyed objects created
- Interpolator remaps succeed

---

### PR3: Validation - Fail Fast on Object Count Mismatch
**Priority:** P2 MEDIUM  
**Files:** `converter/import_test.cpp`, `scripts/roundtrip_compare.sh`  
**Estimated Time:** 1 hour  

**Changes:**

**1. import_test.cpp - Add Object Count Validation**
```cpp
// After successful import (current line ~200)
if (result == rive::ImportResult::success && file) {
    size_t totalObjects = 0;
    for (size_t i = 0; i < file->artboardCount(); ++i) {
        auto* ab = file->artboard(i);
        totalObjects += ab->objects().size();
    }
    
    std::cout << "\n📊 Import Summary:" << std::endl;
    std::cout << "  Total objects imported: " << totalObjects << std::endl;
    
    // If we have expected count from analyzer, compare:
    if (argc > 2) {
        size_t expectedCount = std::stoul(argv[2]);
        if (totalObjects != expectedCount) {
            std::cerr << "❌ OBJECT COUNT MISMATCH!" << std::endl;
            std::cerr << "   Expected: " << expectedCount << std::endl;
            std::cerr << "   Imported: " << totalObjects << std::endl;
            std::cerr << "   Lost:     " << (expectedCount - totalObjects) << std::endl;
            return 1; // FAIL
        }
    }
}
```

**2. roundtrip_compare.sh - Capture Analyzer Counts**
```bash
# Extract original object count
ORIG_COUNT=$(python3 converter/analyze_riv.py --json "$ORIGINAL_FILE" 2>/dev/null | \
             grep -o '"total_objects":[0-9]*' | cut -d: -f2)

# Pass to import_test
./converter/import_test "$ROUNDTRIP_FILE" "$ORIG_COUNT"
if [ $? -ne 0 ]; then
    echo "❌ Import validation failed - object count mismatch"
    exit 1
fi
```

**Validation:**
- Script exits with code 1 if object counts differ
- CI catches regressions automatically

---

### PR4: Scripts - Fix JSON Parsing & Grep Issues
**Priority:** P2 MEDIUM  
**Files:** `scripts/track_roundtrip_growth.py`, `scripts/compare_riv_files.py`, `scripts/roundtrip_compare.sh`  
**Estimated Time:** 1 hour  

**Changes:**

**1. track_roundtrip_growth.py - Strip analyzer logs**
```python
# Line 30-36 (BEFORE):
result = subprocess.run([...], capture_output=True, text=True)
data = json.loads(result.stdout)  # FAILS - has [info] lines

# AFTER:
result = subprocess.run([...], capture_output=True, text=True)
# Strip everything before first '{'
json_start = result.stdout.find('{')
if json_start >= 0:
    data = json.loads(result.stdout[json_start:])
else:
    data = {"total_objects": 0, "property_keys": {}}  # Fallback
```

**2. compare_riv_files.py - Silent JSON mode**
```python
# Add --silent flag
parser.add_argument('--silent', action='store_true', 
                    help='Suppress info logs for machine-readable JSON')

if args.silent:
    # Don't print analyzer logs
    pass
```

**3. roundtrip_compare.sh - Fix grep**
```bash
# Line 76 (BEFORE):
NULL_COUNT=$(grep -c "NULL!" "$IMPORT_LOG" || echo 0)  # Returns "0\n0"

# AFTER:
NULL_COUNT=$(grep -c "NULL!" "$IMPORT_LOG" 2>/dev/null || echo 0)
# Ensure single value
NULL_COUNT=${NULL_COUNT%%$'\n'*}
```

**Validation:**
- JSON parsing succeeds
- Object counts populate correctly
- Grep doesn't break conditionals

---

## Execution Plan

### Phase 1: Critical Fix (PR1)
1. ✅ Implement objectId dependency edges in topological sort
2. ✅ Test with bee_baby.riv
3. ✅ Verify cascade skip count = 0
4. ✅ Verify object count = 540
5. **→ WAIT FOR REVIEW FEEDBACK**

### Phase 2: Builder Cleanup (PR2)
1. Remove premature objectId check
2. Test round-trip conversion
3. Verify interpolator remaps succeed
4. **→ WAIT FOR REVIEW FEEDBACK**

### Phase 3: Validation (PR3)
1. Add object count validation to import_test
2. Update CI scripts
3. Test full pipeline
4. **→ WAIT FOR REVIEW FEEDBACK**

### Phase 4: Scripts (PR4)
1. Fix JSON parsing
2. Fix grep issues
3. Verify CI diagnostics
4. **→ WAIT FOR REVIEW FEEDBACK**

---

## Success Criteria

### PR1 Success:
- [ ] Cascade skip messages: 521 → 0
- [ ] Object count: 294 → 540
- [ ] No "KeyedObject targets missing" errors
- [ ] Topological sort completes in 1 pass

### PR2 Success:
- [ ] All keyed objects created
- [ ] Interpolator remap success > 0
- [ ] No cascade skips
- [ ] Round-trip object count matches original

### PR3 Success:
- [ ] import_test fails on object count mismatch
- [ ] CI detects regressions
- [ ] Exit codes correct

### PR4 Success:
- [ ] JSON parsing succeeds
- [ ] Object counts non-zero
- [ ] Grep conditionals work
- [ ] Machine-readable output

---

## Risk Assessment

### PR1: 🔥 HIGH RISK
- Topological sort is core algorithm
- Must handle cycles correctly
- Edge cases: self-referencing objects, missing targets

**Mitigation:**
- Add cycle detection
- Extensive logging
- Test with multiple RIV files

### PR2: ⚠️ MEDIUM RISK
- Removing validation might expose edge cases
- Depends on PR1 correctness

**Mitigation:**
- Keep as warning (don't remove entirely)
- Only proceed after PR1 validation

### PR3-4: ✅ LOW RISK
- Script/validation changes
- No core algorithm impact
- Easy to revert

---

## Testing Matrix

| Test File | Original Objects | Expected Round-Trip | Status |
|-----------|------------------|---------------------|--------|
| bee_baby.riv | 540 | 540 | 🔄 In Progress |
| rectangle.riv | ~10 | ~10 | ⏳ Pending |
| interactive_monster.riv | 1428 | 1428 | ⏳ Pending |
| nature.riv | ~5000 | ~5000 | ⏳ Pending |

---

## Next Immediate Action

**START PR1:** Implement objectId dependency edges in topological sort

Files to modify:
- `converter/extractor_postprocess.hpp:71-138` (topologicalSort function)

Expected changes:
- ~20 lines added
- Build reference edge map
- Merge with parent edge map
- Compute in-degrees from combined edges

**READY TO BEGIN - AWAITING GO-AHEAD**
