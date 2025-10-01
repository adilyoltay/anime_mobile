# Optional Next PRs - Brief Outlines

**Status**: All optional - pipeline is production-ready  
**Priority**: LOW (enhancements, not blockers)

---

## PR-TrimPath-Compat

**Title**: "TrimPath Runtime Compatibility - Investigate and Fix"

**Goal**: Re-enable TrimPath with proper runtime support

**Implementation**:
1. Extract working TrimPath from known-good RIV:
   ```bash
   # Find a RIV with working TrimPath
   ./universal_extractor working_trim.riv working.json
   grep -A10 '"typeKey": 47' working.json
   ```

2. Compare properties:
   ```
   Working TrimPath:
     start: 0.0 vs 0.5?
     end: 1.0 vs 100.0?
     offset: 0.0
     modeValue: 0 vs 1?
   
   Our TrimPath:
     start: 0.0
     end: 0.0  ← Problem?
     offset: 0.0
     modeValue: 0
   ```

3. Test hypotheses:
   - Try `end: 1.0` (normalized range)
   - Try `end: 100.0` (percentage)
   - Try `modeValue: 1` (different mode)

4. Update defaults in `extractor_postprocess.hpp`:
   ```cpp
   case 47: // TrimPath
       props["start"] = 0.0;
       props["end"] = 1.0;  // Was 0.0
       props["offset"] = 0.0;
       props["modeValue"] = 0;
   ```

5. Add validation rule:
   ```cpp
   // In json_validator.cpp
   if (typeKey == 47) {
       double start = props.value("start", 0.0);
       double end = props.value("end", 1.0);
       if (start > end) {
           errors.push_back("TrimPath start > end");
       }
   }
   ```

6. Re-enable in extractor:
   ```cpp
   // Remove skip in checkParentSanity()
   // if (typeKey == 47) { continue; }  // DELETE THIS
   ```

**Test Plan**:
- 23 objects (with TrimPath): expect SUCCESS
- All thresholds: expect SUCCESS
- Validator: expect PASS

**Exit Criteria**: TrimPath files import without MALFORMED

**Estimated Time**: 2-4 hours

---

## PR-StateMachine-Enable

**Title**: "StateMachine Re-enable - Test and Validate"

**Goal**: Enable StateMachine objects if needed

**Implementation**:
```cpp
// In universal_builder.cpp line 453
constexpr bool OMIT_STATE_MACHINE = false; // Was true
```

**Test Plan**:
1. Find RIV with StateMachine
2. Convert and test import
3. Verify no regressions on bee_baby

**Exit Criteria**: 
- SM files import SUCCESS
- bee_baby still SUCCESS

**Estimated Time**: 30 minutes - 1 hour

**Note**: bee_baby has 0 StateMachines, so may not be testable with current assets

---

## PR-Extractor-Keyed-Fix

**Title**: "Extractor Keyed Data Support - Fix Segfault"

**Goal**: Enable full round-trip (extract keyed data from converted files)

**Implementation**:
1. Debug segfault:
   ```bash
   lldb ./universal_extractor
   run output/bee_baby_KEYED.riv output.json
   bt  # Get stack trace
   ```

2. Likely issues:
   - Missing null checks
   - Invalid cast (KeyFrame types)
   - Infinite loop in keyed tree traversal

3. Fix extraction (example):
   ```cpp
   // In universal_extractor.cpp
   if (auto* keyedObj = dynamic_cast<KeyedObject*>(obj)) {
       objJson["objectId"] = keyedObj->objectId();
       // Extract keyed properties
   }
   ```

**Test Plan**:
- Extract from converted file: expect no segfault
- Validate extracted JSON: expect PASS
- Re-convert extracted JSON: expect SUCCESS

**Exit Criteria**: Full round-trip works without crash

**Estimated Time**: 2-3 hours

---

## PR-Regression-Automation

**Title**: "Automated Regression Testing - CI/CD Integration"

**Goal**: Prevent future regressions with automated testing

**Implementation**:

1. Create test script (`tests/regression_suite.sh`):
   ```bash
   #!/bin/bash
   set -e
   
   THRESHOLDS=(189 190 273)
   
   for count in "${THRESHOLDS[@]}"; do
       echo "=== Testing $count objects ==="
       
       # Validate
       ./json_validator "test_$count.json" || {
           echo "❌ Validation failed"
           exit 1
       }
       
       # Convert
       ./rive_convert_cli "test_$count.json" "test_$count.riv"
       
       # Import
       ./import_test "test_$count.riv" || {
           echo "❌ Import failed"
           exit 1
       }
       
       echo "✅ $count objects passed"
   done
   
   echo "✅ All regression tests passed"
   ```

2. Add large asset test:
   ```bash
   # Test full bee_baby
   ./universal_extractor bee_baby.riv bee_baby.json
   ./json_validator bee_baby.json
   ./rive_convert_cli bee_baby.json bee_baby_rebuilt.riv
   ./import_test bee_baby_rebuilt.riv
   ```

3. CI/CD integration (GitHub Actions example):
   ```yaml
   name: Rive Converter Tests
   
   on: [push, pull_request]
   
   jobs:
     test:
       runs-on: ubuntu-latest
       steps:
         - uses: actions/checkout@v2
         - name: Build
           run: |
             cmake -S . -B build
             cmake --build build
         - name: Run Tests
           run: |
             cd tests
             ./regression_suite.sh
   ```

**Test Plan**:
- Run locally: all tests pass
- Trigger on PR: tests gate merge
- Monitor in CI: catch regressions early

**Exit Criteria**: Automated tests run on every commit

**Estimated Time**: 2-3 hours (script + CI setup)

---

## Summary

All 4 optional PRs are **enhancements**, not blockers:

1. **TrimPath-Compat**: Re-enable rare feature (2-4h)
2. **StateMachine**: Enable if needed (30min-1h)
3. **Extractor-Keyed**: Fix round-trip (2-3h)
4. **Regression-Automation**: Prevent regressions (2-3h)

**Total optional work**: 6-11 hours

**Current pipeline**: ✅ **PRODUCTION READY WITHOUT THESE**

Choose based on:
- TrimPath usage in production
- StateMachine requirements
- Round-trip extraction needs
- CI/CD infrastructure availability
