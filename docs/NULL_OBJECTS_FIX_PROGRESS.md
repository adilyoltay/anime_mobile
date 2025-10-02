# NULL Objects Fix - Progress Report

**Date:** 2025-10-02  
**Project:** Rive Runtime - Round-trip Converter  
**Critical Issue:** 230/271 (85%) NULL objects in round-trip RIV files

---

## 🎯 Executive Summary

Round-trip conversion creates RIV files with NULL objects that crash Rive Play. Root cause: `File::readRuntimeObject()` returns `nullptr` when runtime cannot deserialize objects - likely due to keyed animation data ordering or missing type registrations.

**Current Status:** 2/6 PRs completed (33%)

---

## 📊 PR Timeline & Status

### ✅ PR-NULL-GUARD (COMPLETED)
**Branch:** `fix/null-objects-hard-fail`  
**Status:** ✅ Merged & Approved  
**Priority:** P0 - Critical  
**Duration:** 30 min  

**Objective:** NULL objects = HARD FAILURE (prevent malformed RIV from passing CI)

**Changes:**
- `converter/import_test.cpp`: Track nullCount, exit(1) if >0
- `scripts/roundtrip_compare.sh`: Changed warning → hard failure

**Results:**
```
✅ Original bee_baby.riv → Exit 0 (no NULLs)
❌ Roundtrip bee_baby.riv → Exit 1 (230 NULLs detected)
```

**Impact:**
- ✅ CI now rejects malformed RIV files
- ✅ Rive Play crashes prevented
- ✅ No regressions detected

**Review:** Approved - no functional issues

---

### ✅ PR-NULL-GUARD-MULTIARTBOARD (COMPLETED)
**Branch:** `feat/null-objects-typekey-trace`  
**Status:** ✅ Built, tested & committed  
**Priority:** P1 - High (review feedback)  
**Duration:** 20 min  

**Objective:** Check NULL objects across ALL artboards (not just default)

**Problem Found:**
```cpp
// Redundant NULL check only on default artboard (lines 123-148)
// while multi-artboard loop already counted all NULLs (lines 50-76)
```

**Solution:**
```cpp
// Removed duplicate NULL check in default artboard section
// All NULL detection unified in multi-artboard iteration loop
int totalNullCount = 0;
for (size_t i = 0; i < file->artboardCount(); ++i) {
    // Accumulate NULLs from ALL artboards
}
if (totalNullCount > 0) { return 1; }  // ✅ Single source of truth
```

**Results:**
```
✅ bee_baby.riv (2 artboards): Exit 0
❌ bee_baby_roundtrip.riv (230 NULLs): Exit 1 (correct fail)
```

**Commit:** `39fa92a8` - "fix: Remove redundant default-artboard NULL check"

---

### ✅ PR-KEYED-TRACE (COMPLETED)
**Branch:** `feat/null-objects-typekey-trace`  
**Status:** ✅ Built, traced, findings recorded  
**Priority:** P0 - Critical (diagnostic)  
**Duration:** ~45 min  

**Objective:** Trace which typeKeys/properties lead to NULL objects at import time

**Changes Made:**
- `src/file.cpp`: Expanded `readRuntimeObject()` verbose logs for:
  - object creation result, reader errors, deserialize() failures
  - unknown property keys, field-id fallbacks, final nullptr return
- `src/artboard.cpp`: Added verbose logs in `Artboard::validateObjects()` to report deleted (invalid) components with `typeKey` and index.

**Run Commands:**
```
RIVE_IMPORT_VERBOSE=1 ./converter/import_test ../output/roundtrip/bee_baby_roundtrip.riv
```

**Key Findings:**
- Header/ToC fallbacks are common and expected for:
  - key 3 (component id), key 44 (mainArtboardId), key 138 (SM name)
- NULLs originate mostly during `Artboard::validateObjects()` — invalid components deleted:
  - SolidColor (typeKey 18), Shape (3), CubicMirroredVertex (35), StraightVertex (5), Fill (20), PointsPath (16), Ellipse (4), Stroke (24), ClippingShape (42), Rectangle (7)
  - Many already-null slots were observed (logged as typeKey=0), indicating prior invalidation cascades
- No evidence that unregistered types (CoreRegistry nullptr) are the primary driver.

**Conclusion:** Initial NULLs cluster around shape/paint/path hierarchy rather than keyed animation objects. Focus next on hierarchy/parenting/order guarantees in the builder/serializer before changing keyed order.

**Artifacts:** `/tmp/verbose_import4.log` (local run)

---

### 🔄 PR-KEYED-ORDER (IN PROGRESS)
**Branch:** `feat/null-objects-typekey-trace`  
**Status:** 🔄 Implementation in progress  
**Priority:** P1 - High  
**Duration:** 2-3 hours (estimated)  

**Objective:** Fix structural hierarchy serialization order (parent-first guarantee)

**Strategy:** Based on PR-KEYED-TRACE findings, the primary issue is structural hierarchy (Shape→Path→Vertices, Shape→Paints) rather than keyed animation data. Implementing parent-first topological sorting for all objects.

**Changes Made:**
1. ✅ Parent-first topological sort (multi-pass algorithm, sorts 806 objects in 1 pass)
2. ✅ Synthetic Shape injection for Fill/Stroke with non-Shape parents
3. ✅ Extended orphan paint fix to include Artboard-parented paints

**Results:**
- Before: 233 NULL objects (extractor output)
- After: 230 NULL objects (3 fixed, 98.7% still failing)
- **Root cause:** Input JSON has incorrect parent relationships (Fill→Artboard instead of Fill→Shape→Artboard)
- Converter mitigations are insufficient for wholesale parent corrections

**Conclusion:** The majority of NULLs stem from **extractor bugs** (incorrect parentId assignments). Converter-side fixes can only address a small subset. Recommend fixing extractor's parent assignment logic.

**UPDATE - FIXED:** Extractor was missing localId assignment for KeyedObject/KeyedProperty/KeyFrame, causing builder to skip them (localId=null → missing in mapping). After fix: **0 NULL objects, round-trip SUCCESS!**

**Analysis Required:**
1. Review `universal_builder.cpp` keyed types order:
   - LinearAnimation (31)
   - KeyedObject (25)
   - KeyedProperty (26)
   - Interpolators (28, 138, etc)
   - KeyFrames (30, 37, 50, 84, 142, 450)

2. Compare with runtime expectations:
   - Check `include/rive/animation/` for hierarchy
   - Verify parent → child ordering

3. Implement topological sort:
   - Ensure KeyedObject before KeyedProperty
   - Ensure KeyedProperty before KeyFrames
   - Ensure Interpolators referenced before use

**Success Criteria:**
- ✅ NULL count decreases (target: 0)
- ✅ Import test passes
- ✅ Object stream in correct order

**Blocked By:** PR-KEYED-TRACE (need to know which types fail)

---

### 🔍 PR-CI-VALIDATION (PLANNED)
**Branch:** TBD  
**Status:** 📝 Planned  
**Priority:** P1 - High  
**Duration:** 1 hour (estimated)  

**Objective:** Add comprehensive validation to CI pipeline

**Planned Changes:**

**1. `scripts/round_trip_ci.sh` - New Step 4.5:**
```bash
echo "  [4.5/6] Advanced validation..."

# ToC vs Stream integrity
python3 scripts/validate_stream_integrity.py "$riv_file" || FAIL

# Chunk alignment & structure
python3 converter/analyze_riv.py "$riv_file" --strict || FAIL

# Binary diff (non-blocking)
python3 scripts/compare_riv_files.py original.riv roundtrip.riv --json \
  > diff.json || true  # Exit 1 OK, 2+ fail
```

**2. Validation Checks:**
- ✅ ToC divergence detection
- ✅ Truncated varuint detection
- ✅ Padding/alignment verification
- ✅ Property key consistency
- ✅ Type bitmap verification

**Success Criteria:**
- ✅ Malformed RIV files fail before import_test
- ✅ Detailed error reports for debugging
- ✅ No silent failures

**Blocked By:** None (can run parallel with PR-KEYED-ORDER)

---

### 📏 PR-GROWTH-REVALIDATE (PLANNED)
**Branch:** TBD  
**Status:** 📝 Planned (blocked by fixes)  
**Priority:** P2 - Medium  
**Duration:** 30 min (estimated)  

**Objective:** Re-measure growth after NULL fixes

**Current Metrics (BROKEN):**
```
Original:   9,700 bytes
Round-trip: 11,586 bytes
Growth:     +19.44% ❌ FAIL (>10% threshold)
Objects:    540 → 800 (+260)
NULL:       230/271 (85%) ❌ MALFORMED
```

**Expected After Fixes:**
```
Original:   9,700 bytes
Round-trip: ~10,200 bytes (estimate)
Growth:     ~5% ✅ PASS (or warning)
Objects:    540 → ~600 (keyed data expansion)
NULL:       0/271 (0%) ✅ CLEAN
```

**Tasks:**
- [ ] Run full round-trip on bee_baby.riv
- [ ] Measure size, object count, growth %
- [ ] Update growth thresholds if needed
- [ ] Document expected keyed data expansion

**Success Criteria:**
- ✅ Growth <10% (ideally <5%)
- ✅ Zero NULL objects
- ✅ Rive Play loads successfully

**Blocked By:** PR-KEYED-ORDER (need clean RIV files first)

---

### 📚 PR-DOCS-UPDATE (PLANNED)
**Branch:** TBD  
**Status:** 📝 Planned  
**Priority:** P3 - Low  
**Duration:** 1 hour (estimated)  

**Objective:** Document findings and solutions

**Documents to Create/Update:**

**1. `docs/NULL_OBJECTS_ROOT_CAUSE.md`**
- Problem definition
- Root cause analysis
- TypeKey trace results
- Fix implementation
- Lessons learned

**2. `AGENTS.md` Updates**
- Section: "## 15. NULL Objects Detection (Oct 2024)"
- Keyed data ordering best practices
- Multi-artboard testing requirements
- CI validation checklist

**3. `docs/VALIDATION_INFRASTRUCTURE.md`**
- Add NULL detection section
- Update CI pipeline description
- Add troubleshooting guide

**Success Criteria:**
- ✅ Complete documentation of issue
- ✅ Reproducible fix instructions
- ✅ Updated agent memory

**Blocked By:** All previous PRs (document after completion)

---

## 📈 Progress Metrics

### Overall Status
- **Total PRs:** 6
- **Completed:** 2 (33%)
- **In Progress:** 1 (17%)
- **Planned:** 3 (50%)

### Time Investment
- **Spent:** ~50 min
- **Remaining:** ~6-8 hours (estimated)
- **Total:** ~7-9 hours

### Code Changes
- **Files Modified:** 3
- **Lines Added:** ~60
- **Lines Removed:** ~15
- **Net Change:** +45 lines

---

## 🎯 Critical Path

```
PR-NULL-GUARD (✅ DONE)
    ↓
PR-NULL-GUARD-MULTIARTBOARD (🔄 IN PROGRESS)
    ↓
PR-KEYED-TRACE (⏸️ PAUSED)
    ↓
PR-KEYED-ORDER (📝 PLANNED) ← Most Critical
    ↓
PR-CI-VALIDATION (📝 PLANNED) ← Can parallel
    ↓
PR-GROWTH-REVALIDATE (📝 PLANNED)
    ↓
PR-DOCS-UPDATE (📝 PLANNED)
```

---

## 🚨 Current Blockers

### Active
- **P1 Review Feedback:** Multi-artboard NULL detection
  - **Status:** Code complete, needs build/test
  - **ETA:** 10 min

### Upcoming
- **TypeKey Analysis:** Need trace data to identify failing types
  - **Blocked By:** PR-KEYED-TRACE completion
  - **Impact:** Cannot fix ordering without knowing which types fail

---

## 🎯 Next Actions (Priority Order)

1. **IMMEDIATE (Today)**
   - [ ] Build PR-NULL-GUARD-MULTIARTBOARD
   - [ ] Test with multi-artboard RIV
   - [ ] Commit & push amendment
   - [ ] Resume PR-KEYED-TRACE
   - [ ] Build with verbose logging
   - [ ] Run trace on bee_baby_roundtrip.riv
   - [ ] Analyze typeKey patterns

2. **SHORT TERM (This Week)**
   - [ ] Implement PR-KEYED-ORDER based on trace
   - [ ] Implement PR-CI-VALIDATION (parallel)
   - [ ] Verify NULL count → 0
   - [ ] Test Rive Play compatibility

3. **MEDIUM TERM (Next Week)**
   - [ ] Re-measure growth metrics
   - [ ] Document all findings
   - [ ] Update AGENTS.md
   - [ ] Create comprehensive guide

---

## 📊 Success Criteria (Final)

### Technical
- ✅ Zero NULL objects in round-trip RIV files
- ✅ Import test passes for all artboards
- ✅ Rive Play loads without crash
- ✅ Round-trip growth <5% (warning at 5-10%)
- ✅ CI catches malformed RIV files
- ✅ Full validation coverage

### Process
- ✅ All PRs reviewed and merged
- ✅ Comprehensive test coverage
- ✅ Documentation complete
- ✅ No regressions introduced
- ✅ CI pipeline hardened

---

## 📝 Notes & Observations

### Key Findings
1. **NULL objects are real bugs** - Not placeholders, genuine deserialization failures
2. **Multi-artboard risk** - Original implementation only checked default artboard
3. **Keyed data suspect** - +260 objects = keyed animation expansion
4. **CI gap** - Malformed files passed as "SUCCESS" before fix

### Lessons Learned
1. Always check ALL artboards, not just default
2. Exit codes matter - soft warnings let bugs through
3. Verbose logging essential for debugging runtime issues
4. Validation must be comprehensive (ToC, stream, types)

### Open Questions
1. Why does `CoreRegistry::makeCoreInstance()` return nullptr for some types?
2. Is it missing registrations or ordering issue?
3. Are there other edge cases beyond multi-artboard?
4. What's the acceptable keyed data expansion percentage?

---

**Report Generated:** 2025-10-02 17:58:00  
**Last Updated:** After P1 review feedback  
**Next Update:** After PR-NULL-GUARD-MULTIARTBOARD completion
