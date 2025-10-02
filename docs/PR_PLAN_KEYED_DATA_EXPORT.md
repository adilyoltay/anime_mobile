# PR Plan: Keyed Data Export for Full Round-Trip Support

**PR ID:** PR-KEYED-DATA-EXPORT  
**Priority:** P1 (High)  
**Complexity:** High  
**Estimated Effort:** 3-5 days  
**Dependencies:** None (interpolatorId fix already complete)  

---

## Executive Summary

**Problem:** Multiple round-trips lose keyed data (KeyedObject, KeyedProperty, KeyFrame) because the extractor only exports components from `artboard->objects()`, which excludes runtime-created components.

**Impact:**
- ❌ Round-trip #2: 475 keyed data → 0 (100% loss)
- ❌ Animations lost after re-export
- ❌ Cannot edit and re-export RIV files

**Goal:** Export ALL components (file-loaded + runtime-created) to enable full round-trip stability.

---

## Problem Analysis

### Current Behavior

```
Original RIV → JSON (C1):
  ✅ artboard->objects() returns 285 components
  ✅ coreIdToLocalId has 285 entries
  ✅ KeyedObject.objectId remaps correctly

JSON → RIV (RT1):
  ✅ Builder creates ~600 objects
  ✅ Components + keyed data both created
  ⚠️  Runtime-created components NOT in artboard->objects()

RT1 RIV → JSON (C3):
  ❌ artboard->objects() returns only 8 components
  ❌ coreIdToLocalId has only 8 entries
  ❌ KeyedObject.objectId references missing (227, 229, etc.)

JSON → RIV (RT2):
  ❌ Builder skips KeyedObjects (orphaned references)
  ❌ Result: 0 keyed data
```

### Root Cause

**Rive Runtime Limitation:**
```cpp
// Current extractor approach
for (auto obj : artboard->objects()) {
    // Only returns FILE-LOADED components
    // Runtime-created components NOT included
}
```

**Why?**
- `artboard->objects()` is a file-deserialization structure
- Components created by builder at runtime are NOT added to this list
- But keyed data DOES reference these runtime-created components
- Result: Orphaned references in JSON

---

## Solution Strategy

### Approach 1: Component Graph Traversal (RECOMMENDED)

**Idea:** Build complete component graph by traversing from artboard root

**Implementation:**
```cpp
// Step 1: Collect ALL components via graph traversal
std::unordered_set<Component*> allComponents;
std::queue<Component*> queue;
queue.push(artboard);

while (!queue.empty()) {
    Component* comp = queue.front();
    queue.pop();
    
    if (allComponents.count(comp)) continue;
    allComponents.insert(comp);
    
    // Add children (for Container types)
    if (auto* container = dynamic_cast<ContainerComponent*>(comp)) {
        for (auto* child : container->children()) {
            queue.push(child);
        }
    }
}

// Step 2: Export ALL components
for (auto* comp : allComponents) {
    uint32_t runtimeId = artboard->idOf(comp);
    uint32_t localId = nextLocalId++;
    coreIdToLocalId[runtimeId] = localId;
    // ... export component
}

// Step 3: Export keyed data (objectId remapping now works)
for (auto* anim : artboard->animations()) {
    for (auto* ko : anim->keyedObjects()) {
        // Remap objectId using complete coreIdToLocalId map
        uint32_t runtimeId = ko->objectId();
        koJson["properties"]["objectId"] = coreIdToLocalId[runtimeId];
    }
}
```

**Pros:**
- ✅ Captures ALL components (file + runtime)
- ✅ No dependency on objects() ordering
- ✅ Natural parent-child relationships preserved

**Cons:**
- ⚠️  Need to implement graph traversal
- ⚠️  May include system components (need filtering)

### Approach 2: Iterate All Core Objects (ALTERNATIVE)

**Idea:** Use artboard's internal object map

```cpp
// Rive SDK may provide:
// - artboard->objectCount()
// - artboard->objectAt(index)
// - artboard->allObjects()

// If available:
for (size_t i = 0; i < artboard->objectCount(); ++i) {
    auto* obj = artboard->objectAt(i);
    if (auto* comp = obj->as<Component>()) {
        uint32_t runtimeId = artboard->idOf(comp);
        coreIdToLocalId[runtimeId] = nextLocalId++;
        // ... export
    }
}
```

**Pros:**
- ✅ Simple iteration
- ✅ Complete coverage

**Cons:**
- ❌ API may not exist in Rive SDK
- ❌ May include internal/system objects

### Approach 3: Keyed Data Hints (FALLBACK)

**Idea:** Collect component IDs from keyed data, then export those

```cpp
// Step 1: Pre-scan keyed data for referenced component IDs
std::unordered_set<uint32_t> referencedIds;
for (auto* anim : artboard->animations()) {
    for (auto* ko : anim->keyedObjects()) {
        referencedIds.insert(ko->objectId());
    }
}

// Step 2: Export artboard->objects() first
// ...

// Step 3: For missing IDs, try to resolve via artboard lookup
for (uint32_t runtimeId : referencedIds) {
    if (coreIdToLocalId.count(runtimeId)) continue;
    
    // Attempt to find component by ID
    auto* obj = artboard->resolve(runtimeId);
    if (obj && obj->is<Component>()) {
        // Export this component retroactively
        uint32_t localId = nextLocalId++;
        coreIdToLocalId[runtimeId] = localId;
        // ... export component
    }
}
```

**Pros:**
- ✅ Minimal change to existing code
- ✅ Only exports referenced components

**Cons:**
- ❌ Fragile (depends on resolve() API)
- ❌ May miss components not referenced by keyed data
- ❌ Export order issues (components after keyed data)

---

## Recommended Implementation Plan

### Phase 1: Investigation (Day 1)

**Goal:** Understand Rive SDK's component access methods

**Tasks:**
1. ✅ Review `include/rive/artboard.hpp` for component iteration APIs
2. ✅ Check `include/rive/component.hpp` for hierarchy traversal
3. ✅ Test different approaches with debug logging
4. ✅ Document findings in `KEYED_DATA_INVESTIGATION.md`

**Deliverables:**
- API analysis document
- Proof-of-concept code snippet
- Decision on Approach 1 vs 2 vs 3

### Phase 2: Implementation (Days 2-3)

**Goal:** Implement chosen approach in extractor

**Tasks:**

**2.1 Component Collection (2-3 hours)**
```cpp
File: converter/universal_extractor.cpp

// Add helper function
std::vector<Component*> collectAllComponents(Artboard* artboard) {
    std::vector<Component*> result;
    std::unordered_set<Component*> visited;
    std::queue<Component*> queue;
    
    // Start from artboard root
    queue.push(artboard);
    
    while (!queue.empty()) {
        Component* comp = queue.front();
        queue.pop();
        
        if (visited.count(comp)) continue;
        visited.insert(comp);
        result.push_back(comp);
        
        // Traverse children
        for (size_t i = 0; i < comp->childCount(); ++i) {
            queue.push(comp->childAt(i));
        }
    }
    
    return result;
}
```

**2.2 Export Logic Update (3-4 hours)**
```cpp
// Replace current artboard->objects() loop
auto allComponents = collectAllComponents(artboard);

std::cout << "  Collecting components: " 
          << allComponents.size() << " total" << std::endl;

// Build complete coreIdToLocalId mapping
for (auto* comp : allComponents) {
    uint32_t runtimeCoreId = artboard->idOf(comp);
    
    if (comp->is<Artboard>()) {
        coreIdToLocalId[runtimeCoreId] = 0;
    } else {
        uint32_t localId = nextLocalId++;
        coreIdToLocalId[runtimeCoreId] = localId;
        
        // Export component (existing logic)
        json compJson = exportComponent(comp, localId);
        artboardJson["objects"].push_back(compJson);
    }
}

std::cout << "  Component ID mapping: " 
          << coreIdToLocalId.size() << " entries" << std::endl;
```

**2.3 Validation (2 hours)**
```cpp
// Add diagnostic logging
int mappingHits = 0;
int mappingMisses = 0;

for (auto* anim : artboard->animations()) {
    for (auto* ko : anim->keyedObjects()) {
        uint32_t runtimeId = ko->objectId();
        if (coreIdToLocalId.count(runtimeId)) {
            mappingHits++;
        } else {
            mappingMisses++;
            std::cerr << "⚠️  KeyedObject.objectId " << runtimeId 
                      << " not in coreIdToLocalId" << std::endl;
        }
    }
}

std::cout << "  KeyedObject.objectId mapping: "
          << mappingHits << " hits, " 
          << mappingMisses << " misses" << std::endl;
```

**Deliverables:**
- Updated extractor with component graph traversal
- Complete coreIdToLocalId mapping
- Diagnostic logging for validation

### Phase 3: Testing (Day 4)

**Goal:** Verify keyed data preservation across multiple round-trips

**Test Cases:**

**3.1 Single Round-Trip (Baseline)**
```bash
# Should already work (no regression)
./universal_extractor original.riv c1.json
./rive_convert_cli c1.json rt1.riv
./import_test rt1.riv
# Expected: SUCCESS
```

**3.2 Double Round-Trip (Target)**
```bash
# RT1 → JSON → RT2
./universal_extractor rt1.riv c3.json
./rive_convert_cli c3.json rt2.riv
./import_test rt2.riv

# Verify keyed data preserved
python3 check_keyed_data.py c1.json c3.json
# Expected: Same keyed data count
```

**3.3 Triple Round-Trip (Stability)**
```bash
# RT2 → JSON → RT3
./universal_extractor rt2.riv c5.json
./rive_convert_cli c5.json rt3.riv

# Verify convergence
cmp rt2.riv rt3.riv
# Expected: Identical files
```

**3.4 Test Files**
- ✅ `bee_baby.riv` (487 keyed data objects)
- ✅ `demo-casino-slots.riv` (1,529 keyed data objects)
- ✅ `rectangle.riv` (minimal, no keyed data)

**Success Criteria:**
| Metric | Current | Target |
|--------|---------|--------|
| **C3 Components** | 8 | 285+ |
| **C3 Keyed Data** | 475 | 475 |
| **C5 Keyed Data** | 0 | 475 |
| **RT2 = RT3** | Different | Identical |

### Phase 4: Documentation (Day 5)

**Goal:** Document solution and update project docs

**Tasks:**

**4.1 Technical Documentation**
```
File: docs/KEYED_DATA_EXPORT_SOLUTION.md

- Problem summary
- Solution approach
- Implementation details
- Test results
- Known limitations
```

**4.2 Update Project Docs**
```
File: AGENTS.md
- Add PR-KEYED-DATA-EXPORT section
- Update round-trip status
- Mark as COMPLETE

File: FINAL_ROUNDTRIP_VALIDATION.md
- Update with new test results
- Remove "separate issue" notes
```

**4.3 Update Scripts**
```bash
# Ensure CI includes multi-round-trip tests
scripts/round_trip_ci.sh
  - Add 3-cycle test
  - Verify keyed data stability
```

---

## Implementation Details

### File Changes

**Primary File:** `converter/universal_extractor.cpp`

**Lines to Modify:**
- ~170-190: Component collection (replace `artboard->objects()` loop)
- ~380-395: KeyedObject export (validation logging)
- Add: `collectAllComponents()` helper function (new)

**Estimated LOC:** +50 lines, modify 20 lines

### API Research Needed

**Check Rive SDK for:**
```cpp
// Component hierarchy APIs
class Component {
    size_t childCount() const;
    Component* childAt(size_t index) const;
    Component* parent() const;
};

// Or Container-specific
class ContainerComponent {
    std::vector<Component*>& children();
};

// Or Artboard-level
class Artboard {
    size_t objectCount() const;        // May exist
    Core* objectAt(size_t index);      // May exist
    Core* resolve(uint32_t id);        // May exist
};
```

**Fallback Plan:**
If APIs don't exist, use Approach 3 (keyed data hints) as temporary solution.

---

## Risk Assessment

### High Risk

**1. SDK API Availability**
- **Risk:** Required traversal APIs may not exist
- **Mitigation:** Test multiple approaches, have fallback ready
- **Probability:** Low (hierarchy traversal is common)

**2. Component Type Filtering**
- **Risk:** May export system/internal components
- **Mitigation:** Add type filtering (exclude internal types)
- **Impact:** Medium (extra objects in JSON, but not breaking)

### Medium Risk

**3. Performance Impact**
- **Risk:** Graph traversal slower than objects() iteration
- **Mitigation:** Use hash set for visited tracking
- **Impact:** Low (export is one-time operation)

**4. Export Order**
- **Risk:** Components exported in wrong order (children before parents)
- **Mitigation:** Post-processing topological sort (already exists)
- **Impact:** Low (extractor_postprocess handles this)

### Low Risk

**5. Regression**
- **Risk:** Break existing single round-trip
- **Mitigation:** Extensive testing, keep old code path as backup
- **Impact:** Low (can revert easily)

---

## Testing Strategy

### Unit Tests (New)

```cpp
File: converter/test_component_collection.cpp

TEST(ComponentCollection, CollectsAllComponents) {
    // Create artboard with hierarchy
    auto artboard = makeArtboard();
    auto shape1 = makeShape();
    auto shape2 = makeShape();
    artboard->addChild(shape1);
    shape1->addChild(shape2);
    
    // Collect all
    auto components = collectAllComponents(artboard);
    
    // Verify count
    EXPECT_EQ(components.size(), 3); // artboard + 2 shapes
}

TEST(ComponentCollection, BuildsCompleteMapping) {
    auto artboard = makeArtboardWithComponents(10);
    
    // Export and build mapping
    std::unordered_map<uint32_t, uint32_t> mapping;
    exportWithMapping(artboard, mapping);
    
    // Verify all components mapped
    EXPECT_EQ(mapping.size(), 10);
}
```

### Integration Tests

```bash
File: scripts/test_keyed_data_preservation.sh

#!/bin/bash
# Test keyed data preservation across 3 round-trips

echo "Testing keyed data preservation..."

# Original extraction
./universal_extractor bee_baby.riv c1.json
C1_KEYED=$(jq '[.artboards[0].objects[] | select(.typeKey == 25 or .typeKey == 26 or .typeKey == 30)] | length' c1.json)

# Round-trip 1
./rive_convert_cli c1.json rt1.riv
./universal_extractor rt1.riv c3.json
C3_KEYED=$(jq '[.artboards[0].objects[] | select(.typeKey == 25 or .typeKey == 26 or .typeKey == 30)] | length' c3.json)

# Round-trip 2
./rive_convert_cli c3.json rt2.riv
./universal_extractor rt2.riv c5.json
C5_KEYED=$(jq '[.artboards[0].objects[] | select(.typeKey == 25 or .typeKey == 26 or .typeKey == 30)] | length' c5.json)

# Verify
if [ "$C1_KEYED" -eq "$C3_KEYED" ] && [ "$C3_KEYED" -eq "$C5_KEYED" ]; then
    echo "✅ Keyed data preserved: $C1_KEYED objects"
    exit 0
else
    echo "❌ Keyed data lost: $C1_KEYED → $C3_KEYED → $C5_KEYED"
    exit 1
fi
```

---

## Success Metrics

### Primary Metrics

| Metric | Current | Target | Priority |
|--------|---------|--------|----------|
| **Component Export (RT1)** | 8 | 285+ | P0 |
| **Keyed Data (C5)** | 0 | 475+ | P0 |
| **ObjectId Remap Success** | ~2% | 100% | P0 |
| **Round-Trip Convergence** | Never | 3 cycles | P0 |

### Secondary Metrics

| Metric | Target | Priority |
|--------|--------|----------|
| **Export Time** | <5s for casino-slots | P1 |
| **JSON Size** | Similar to C1 | P2 |
| **No Regressions** | All existing tests pass | P0 |

---

## Rollout Plan

### Phase 1: Development Branch
```bash
git checkout -b pr-keyed-data-export
# Implement changes
git commit -m "feat: Add complete component export for keyed data preservation"
```

### Phase 2: Testing
```bash
# Run full test suite
bash scripts/round_trip_ci.sh

# Test on all production files
for file in converter/exampleriv/*.riv; do
    bash scripts/test_keyed_data_preservation.sh "$file"
done
```

### Phase 3: Code Review
- Request review from team
- Address feedback
- Update documentation

### Phase 4: Merge
```bash
git checkout main
git merge pr-keyed-data-export
git push origin main
```

---

## Future Enhancements

### Post-PR Improvements

**1. Component Filtering (P2)**
- Filter out internal/system components
- Only export user-visible components
- Reduces JSON size

**2. Keyed Data Validation (P2)**
- Validate all objectId references resolve
- Warn on orphaned references
- Better error messages

**3. Performance Optimization (P3)**
- Cache component hierarchy
- Parallel export of independent components
- Streaming export for large files

---

## Dependencies

### External Dependencies
- None (uses existing Rive SDK)

### Internal Dependencies
- ✅ interpolatorId fix (complete)
- ✅ Extractor postprocessing (exists)
- ✅ Builder ID remapping (exists)

---

## Timeline

| Phase | Duration | Start | End |
|-------|----------|-------|-----|
| **Investigation** | 1 day | Day 1 | Day 1 |
| **Implementation** | 2 days | Day 2 | Day 3 |
| **Testing** | 1 day | Day 4 | Day 4 |
| **Documentation** | 1 day | Day 5 | Day 5 |
| **Total** | **5 days** | - | - |

---

## Acceptance Criteria

### Must Have (P0)
- ✅ RT1 export includes 200+ components (vs current 8)
- ✅ C5 keyed data matches C1 keyed data (100% preservation)
- ✅ Triple round-trip converges (RT2 = RT3)
- ✅ Zero orphaned KeyedObject references
- ✅ All existing tests pass (no regression)

### Should Have (P1)
- ✅ Diagnostic logging for component count
- ✅ Validation of objectId mapping success rate
- ✅ Documentation updated
- ✅ Integration tests added

### Nice to Have (P2)
- ⏳ Component type filtering
- ⏳ Performance benchmarks
- ⏳ Unit tests for component collection

---

## Related Work

### Completed
- ✅ PR-INTERPOLATORID: Interpolator stability (commit 49a88507)
- ✅ PR-ORPHAN-SM: StateMachine export (commit earlier)

### In Progress
- This PR: Keyed data preservation

### Future
- PR-ANIMATION-PACKER: Optimize animation data format
- PR-COMPONENT-FILTERING: Filter internal components

---

## References

### Documentation
- `FINAL_ROUNDTRIP_VALIDATION.md` - Current test results
- `INTERPOLATORID_BUG_FIX.md` - Similar ID remapping pattern
- `AGENTS.md` - Project status

### Code
- `converter/universal_extractor.cpp:170-190` - Component export loop
- `converter/universal_extractor.cpp:380-395` - KeyedObject export
- `converter/src/universal_builder.cpp:1071-1083` - Forward reference guard

---

## Conclusion

This PR will complete the round-trip pipeline by ensuring ALL components are exported, enabling full keyed data preservation across multiple conversion cycles.

**Priority:** HIGH  
**Impact:** HIGH (enables editing workflow)  
**Risk:** MEDIUM (requires API research)  
**Effort:** 5 days  

**Status:** READY TO START  
**Next Action:** Begin Phase 1 (Investigation)
