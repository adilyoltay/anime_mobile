# PR-VALIDATION: Test Report

**Date:** 2025-10-02 12:02:37  
**Branch:** feature/PR-VALIDATION  
**Commit:** bc715c2e

---

## Test Results Summary

### Test Suite 1: Orphan Fix Validation
**Status:** ✅ PASSED

Tests:
- Orphan paint detection
- Synthetic Shape reuse
- Gradient hierarchy preservation
- Gradient with parametric path
- Dash/DashPath support

### Test Suite 2: Round-Trip Validation
**Status:** ❌ FAILED

Tests:
- JSON → RIV → JSON → RIV stability
- Object count preservation
- Orphan fix idempotency
- Import test validation

---

## Overall Status

❌ **SOME TESTS FAILED**

---

## Test Artifacts

- Validation outputs: `output/validation/`
- Round-trip outputs: `output/roundtrip/`
- Full logs: Check individual test directories

---

## Next Steps

❌ Fix failing tests before merge
❌ Review test artifacts for details
