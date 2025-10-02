# PR-KEYED-DATA-EXPORT - Quick Reference

**Status:** 🔴 NOT STARTED  
**Priority:** P1 (High)  
**Estimated Effort:** 5 days  
**Depends On:** None (interpolatorId fix complete)  

---

## The Problem

```
RT1 RIV → JSON (C3):
  ❌ Only 8 components exported (should be 285+)
  ❌ 475 keyed data objects reference missing components
  
RT2 Creation:
  ❌ Builder skips orphaned KeyedObjects
  ❌ Result: 0 keyed data (100% loss)
```

---

## The Solution

**Replace:** `artboard->objects()` loop (returns 8 components)  
**With:** Component graph traversal (returns ALL components)

```cpp
// NEW: Collect ALL components via graph traversal
std::vector<Component*> collectAllComponents(Artboard* artboard) {
    std::vector<Component*> result;
    std::unordered_set<Component*> visited;
    std::queue<Component*> queue;
    queue.push(artboard);
    
    while (!queue.empty()) {
        auto* comp = queue.front();
        queue.pop();
        if (visited.count(comp)) continue;
        visited.insert(comp);
        result.push_back(comp);
        
        // Add children
        for (size_t i = 0; i < comp->childCount(); ++i) {
            queue.push(comp->childAt(i));
        }
    }
    return result;
}

// Use in extractor
auto allComponents = collectAllComponents(artboard);
for (auto* comp : allComponents) {
    uint32_t runtimeId = artboard->idOf(comp);
    uint32_t localId = nextLocalId++;
    coreIdToLocalId[runtimeId] = localId;
    // ... export component
}
```

---

## Implementation Checklist

### Phase 1: Investigation (1 day)
- [ ] Review Rive SDK component APIs
- [ ] Test `childCount()` / `childAt()` availability
- [ ] Proof-of-concept traversal code
- [ ] Document findings

### Phase 2: Implementation (2 days)
- [ ] Add `collectAllComponents()` helper
- [ ] Replace `artboard->objects()` loop
- [ ] Build complete `coreIdToLocalId` mapping
- [ ] Add diagnostic logging
- [ ] Test with bee_baby.riv

### Phase 3: Testing (1 day)
- [ ] Single round-trip (no regression)
- [ ] Double round-trip (C3 components = C1 components)
- [ ] Triple round-trip (RT2 = RT3)
- [ ] Test on casino-slots.riv
- [ ] Verify keyed data preservation

### Phase 4: Documentation (1 day)
- [ ] Write KEYED_DATA_EXPORT_SOLUTION.md
- [ ] Update AGENTS.md
- [ ] Update FINAL_ROUNDTRIP_VALIDATION.md
- [ ] Create integration test script

---

## Success Metrics

| Metric | Before | After | Status |
|--------|--------|-------|--------|
| C3 Components | 8 | 285+ | ⏳ |
| C5 Keyed Data | 0 | 475+ | ⏳ |
| objectId Remap | ~2% | 100% | ⏳ |
| RT Convergence | Never | 3 cycles | ⏳ |

---

## File Changes

**Primary:**
- `converter/universal_extractor.cpp` (+50 lines, modify ~20)

**Secondary:**
- `docs/KEYED_DATA_EXPORT_SOLUTION.md` (new)
- `scripts/test_keyed_data_preservation.sh` (new)
- `AGENTS.md` (update)

---

## Test Commands

```bash
# Investigation
./build_converter/converter/universal_extractor bee_baby.riv test.json 2>&1 | grep "components"

# After fix - verify component count
jq '[.artboards[0].objects[] | select(.localId != null)] | length' test.json
# Expected: 285+ (vs current 8)

# Full round-trip test
bash scripts/test_keyed_data_preservation.sh bee_baby.riv
# Expected: ✅ Keyed data preserved
```

---

## Risk Mitigation

| Risk | Mitigation |
|------|-----------|
| API not available | Use fallback Approach 3 (keyed data hints) |
| Export order wrong | Rely on existing topological sort |
| Performance issue | Hash set for visited tracking |
| Regression | Keep old code path as backup |

---

## Related Issues

- ✅ **PR-INTERPOLATORID** (complete) - Interpolator stability
- 🔄 **PR-KEYED-DATA-EXPORT** (this) - Component export
- ⏳ **PR-ANIMATION-PACKER** (future) - Format optimization

---

## Next Steps

1. **Start Investigation:** Review Rive SDK component hierarchy APIs
2. **Create Branch:** `git checkout -b pr-keyed-data-export`
3. **Implement POC:** Test `collectAllComponents()` approach
4. **Report Findings:** Document in investigation notes

---

**Full Plan:** See `PR_PLAN_KEYED_DATA_EXPORT.md`  
**Questions:** Check Phase 1 investigation tasks
