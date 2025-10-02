# PR-KEYED-DATA-EXPORT - Phase 1: Investigation Report

**Date:** 2025-10-02  
**Branch:** pr-keyed-data-export  
**Task:** API investigation for complete component export  

---

## Executive Summary

✅ **Rive SDK provides sufficient APIs for complete component collection**  
✅ **Approach 1 (Graph Traversal) is VIABLE and RECOMMENDED**  
✅ **Ready to proceed to Phase 2 (Implementation)**  

---

## API Findings

### 1. Artboard APIs

**File:** `include/rive/artboard.hpp`

#### objects() - Current Usage
```cpp
// Line 233
const std::vector<Core*>& objects() const { return m_Objects; }

// Line 69 (private)
std::vector<Core*> m_Objects;
```

**Observations:**
- ✅ Returns `m_Objects` vector (file-deserialized objects)
- ❌ Does NOT include runtime-created components
- ❌ Returns only 8 components in RT1 (should be 285+)

**Current Problem:**
```
Original RIV: m_Objects has 285 components ✅
RT1 RIV:      m_Objects has 8 components ❌ (runtime components missing)
```

#### resolve() - ID Lookup
```cpp
// Line 151
Core* resolve(uint32_t id) const override;
```

**Observations:**
- ✅ Can resolve objects by runtime component ID
- ✅ Works for both file-loaded and runtime-created objects
- ✅ Could be used for Approach 3 (keyed data hints)

### 2. Component Hierarchy APIs

**File:** `include/rive/component.hpp`, `include/rive/container_component.hpp`

#### parent() - Parent Access
```cpp
// component.hpp:35
inline ContainerComponent* parent() const { return m_Parent; }
```

**Observations:**
- ✅ Every component knows its parent
- ✅ Can traverse UP the hierarchy
- ✅ Useful for validation

#### children() - Child Access
```cpp
// container_component.hpp:20
const std::vector<Component*>& children() const { return m_children; }
```

**Observations:**
- ✅ ContainerComponent provides children() vector
- ✅ Can traverse DOWN the hierarchy
- ✅ Perfect for BFS/DFS traversal
- ⚠️  Only ContainerComponent has this (not all Components)

**Key Classes:**
- `Component` - Base class (has parent())
- `ContainerComponent` - Has children()
- Components like Shape, Group extend ContainerComponent

---

## Approach Analysis

### ✅ Approach 1: Component Graph Traversal (RECOMMENDED)

**Strategy:** Start from artboard, traverse all children recursively

**Implementation:**
```cpp
std::vector<Component*> collectAllComponents(Artboard* artboard) {
    std::vector<Component*> result;
    std::unordered_set<Component*> visited;
    std::queue<Component*> queue;
    
    // Start from artboard (root)
    queue.push(artboard);
    
    while (!queue.empty()) {
        Component* comp = queue.front();
        queue.pop();
        
        if (visited.count(comp)) continue;
        visited.insert(comp);
        result.push_back(comp);
        
        // Traverse children (if ContainerComponent)
        if (auto* container = comp->as<ContainerComponent>()) {
            for (auto* child : container->children()) {
                queue.push(child);
            }
        }
    }
    
    return result;
}
```

**Pros:**
- ✅ Captures ALL components (file + runtime)
- ✅ Natural hierarchy traversal
- ✅ Uses existing SDK APIs (parent(), children())
- ✅ No dependency on objects() ordering
- ✅ Simple BFS algorithm

**Cons:**
- ⚠️  Need to handle ContainerComponent vs Component
- ⚠️  May include internal/system components (can filter)

**API Availability:**
- ✅ `parent()` - Available in Component
- ✅ `children()` - Available in ContainerComponent
- ✅ `as<T>()` - Template cast available

**Decision:** ✅ **USE THIS APPROACH**

---

### Approach 2: Iterate m_Objects Directly

**Strategy:** Access private `m_Objects` directly

**Problem:**
- ❌ `m_Objects` is private
- ❌ Would require friend class or reflection
- ❌ Still has the same limitation (only file objects)

**Decision:** ❌ **NOT VIABLE**

---

### Approach 3: Keyed Data Hints + resolve()

**Strategy:** Collect referenced IDs from keyed data, resolve them

**Implementation:**
```cpp
// Pre-scan keyed data
std::unordered_set<uint32_t> referencedIds;
for (auto* anim : artboard->animations()) {
    for (auto* ko : anim->keyedObjects()) {
        referencedIds.insert(ko->objectId());
    }
}

// Resolve each ID
for (uint32_t runtimeId : referencedIds) {
    auto* obj = artboard->resolve(runtimeId);
    if (obj && obj->is<Component>()) {
        // Export component
    }
}
```

**Pros:**
- ✅ Only exports referenced components
- ✅ Uses existing resolve() API

**Cons:**
- ❌ May miss components not referenced by keyed data
- ❌ Export order issues (components after keyed data)
- ❌ More complex than Approach 1

**Decision:** ⏳ **FALLBACK if Approach 1 fails**

---

## Proof of Concept

### Test Code

```cpp
// File: converter/test_component_collection.cpp

#include "rive/artboard.hpp"
#include "rive/container_component.hpp"
#include <iostream>
#include <queue>
#include <unordered_set>

std::vector<Component*> collectAllComponents(Artboard* artboard) {
    std::vector<Component*> result;
    std::unordered_set<Component*> visited;
    std::queue<Component*> queue;
    
    queue.push(artboard);
    
    while (!queue.empty()) {
        Component* comp = queue.front();
        queue.pop();
        
        if (visited.count(comp)) continue;
        visited.insert(comp);
        result.push_back(comp);
        
        // Check if component is a container
        if (auto* container = comp->as<ContainerComponent>()) {
            for (auto* child : container->children()) {
                queue.push(child);
            }
        }
    }
    
    return result;
}

// Test
int main() {
    // Load test file
    auto file = loadFile("converter/exampleriv/bee_baby.riv");
    auto* artboard = file->artboard();
    
    // Compare approaches
    auto fileObjects = artboard->objects().size();
    auto allComponents = collectAllComponents(artboard);
    
    std::cout << "File objects():     " << fileObjects << std::endl;
    std::cout << "All components:     " << allComponents.size() << std::endl;
    std::cout << "Difference:         " << (allComponents.size() - fileObjects) << std::endl;
    
    return 0;
}
```

### Expected Output

**Original RIV:**
```
File objects():     285
All components:     285
Difference:         0
```

**RT1 RIV:**
```
File objects():     8
All components:     285+
Difference:         277+  ← CAPTURES MISSING COMPONENTS!
```

---

## Implementation Plan

### Phase 2: Implementation (2 days)

#### Step 1: Add collectAllComponents() Helper (2 hours)

**File:** `converter/universal_extractor.cpp`

**Location:** Add before main() function

```cpp
// Helper: Collect ALL components via graph traversal
std::vector<Component*> collectAllComponents(Artboard* artboard) {
    std::vector<Component*> result;
    std::unordered_set<Component*> visited;
    std::queue<Component*> queue;
    
    queue.push(artboard);
    
    while (!queue.empty()) {
        Component* comp = queue.front();
        queue.pop();
        
        if (visited.count(comp)) continue;
        visited.insert(comp);
        result.push_back(comp);
        
        // Traverse children (ContainerComponent only)
        if (auto* container = comp->as<ContainerComponent>()) {
            for (auto* child : container->children()) {
                queue.push(child);
            }
        }
    }
    
    return result;
}
```

#### Step 2: Replace objects() Loop (2 hours)

**Current Code (line ~170):**
```cpp
// CURRENT: Only file-loaded components
for (auto obj : artboard->objects()) {
    if (auto* comp = obj->as<Component>()) {
        // Export component
    }
}
```

**New Code:**
```cpp
// NEW: All components via graph traversal
auto allComponents = collectAllComponents(artboard);

std::cout << "  Collecting components: " 
          << allComponents.size() << " total" << std::endl;

for (auto* comp : allComponents) {
    uint32_t runtimeCoreId = artboard->idOf(comp);
    
    if (comp->is<Artboard>()) {
        coreIdToLocalId[runtimeCoreId] = 0;
    } else {
        uint32_t localId = nextLocalId++;
        coreIdToLocalId[runtimeCoreId] = localId;
        
        // Export component (existing logic)
        json compJson = exportComponent(comp, localId, runtimeCoreId);
        artboardJson["objects"].push_back(compJson);
    }
}

std::cout << "  Component ID mapping: " 
          << coreIdToLocalId.size() << " entries" << std::endl;
```

#### Step 3: Validation Logging (1 hour)

```cpp
// After keyed data export
int mappingHits = 0;
int mappingMisses = 0;

for (auto* anim : artboard->animations()) {
    for (size_t k = 0; k < anim->keyedObjectCount(); ++k) {
        auto* ko = anim->keyedObject(k);
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
          << mappingMisses << " misses (should be 0)" << std::endl;
```

---

## Testing Strategy

### Quick Test

```bash
# Before fix: Export RT1
./build_converter/converter/universal_extractor \
    output/bug_fix_test/converted.riv test_before.json 2>&1 | grep "components"

# Expected:
# "Collecting components: 8 total"  ← PROBLEM

# After fix: Export RT1
./build_converter/converter/universal_extractor \
    output/bug_fix_test/converted.riv test_after.json 2>&1 | grep "components"

# Expected:
# "Collecting components: 285 total"  ← FIXED!

# Verify keyed data mapping
jq '[.artboards[0].objects[] | select(.typeKey == 25)] | length' test_after.json
# Expected: 39 KeyedObjects (vs 39 in original)
```

### Full Round-Trip Test

```bash
# Phase 1: Original → RT1
./universal_extractor bee_baby.riv c1.json
./rive_convert_cli c1.json rt1.riv

# Phase 2: RT1 → C3 (should have full components now)
./universal_extractor rt1.riv c3.json 2>&1 | grep "components"
# Expected: "Collecting components: 285+ total"

# Verify component count
jq '[.artboards[0].objects[] | select(.localId != null)] | length' c3.json
# Expected: 285+ (vs current 8)

# Phase 3: C3 → RT2 (keyed data should be preserved)
./rive_convert_cli c3.json rt2.riv

# Verify import
./import_test rt2.riv
# Expected: SUCCESS (vs current: 0 keyed data)

# Phase 4: RT2 → C5 (verify stability)
./universal_extractor rt2.riv c5.json

# Verify keyed data preservation
jq '[.artboards[0].objects[] | select(.typeKey == 25 or .typeKey == 26 or .typeKey == 30)] | length' c5.json
# Expected: 475+ (vs current 0)
```

---

## Success Criteria

| Metric | Before | After | Status |
|--------|--------|-------|--------|
| **C3 Components** | 8 | 285+ | ⏳ Pending |
| **C3 coreIdToLocalId** | 8 entries | 285+ entries | ⏳ Pending |
| **C5 Keyed Data** | 0 | 475+ | ⏳ Pending |
| **objectId Remap Success** | ~2% | 100% | ⏳ Pending |
| **Round-Trip Convergence** | Never | 3 cycles | ⏳ Pending |

---

## Risks & Mitigation

### Risk 1: ContainerComponent Cast
**Risk:** Not all components are ContainerComponent  
**Mitigation:** Use `as<ContainerComponent>()` and null check

### Risk 2: Circular References
**Risk:** Parent-child cycles  
**Mitigation:** Use `visited` set to prevent re-processing

### Risk 3: System Components
**Risk:** May export internal/system components  
**Mitigation:** Add type filtering if needed (post-implementation)

---

## Dependencies

### Required Headers
```cpp
#include <queue>           // For BFS queue
#include <unordered_set>   // For visited tracking
#include "rive/container_component.hpp"  // For children() access
```

### SDK APIs Used
- ✅ `Component::parent()` - Available
- ✅ `ContainerComponent::children()` - Available
- ✅ `Component::as<T>()` - Available
- ✅ `Artboard::idOf()` - Available (existing usage)

---

## Next Steps

### Immediate Actions
1. ✅ API investigation complete
2. ⏳ Implement `collectAllComponents()` helper
3. ⏳ Replace `objects()` loop in extractor
4. ⏳ Add validation logging
5. ⏳ Test with bee_baby.riv

### Timeline
- **Phase 1 (Investigation):** ✅ COMPLETE (1 day)
- **Phase 2 (Implementation):** ⏳ NEXT (2 days)
- **Phase 3 (Testing):** ⏳ Planned (1 day)
- **Phase 4 (Documentation):** ⏳ Planned (1 day)

---

## Conclusion

**Approach 1 (Graph Traversal) is VIABLE and RECOMMENDED.**

The Rive SDK provides sufficient APIs (`parent()`, `children()`) to implement complete component collection via BFS traversal. This will enable full keyed data preservation across multiple round-trips.

**Status:** ✅ **READY TO PROCEED TO PHASE 2**

---

**Investigation Date:** 2025-10-02  
**Branch:** pr-keyed-data-export  
**Next Phase:** Implementation  
**Estimated Time to Complete:** 3 days (Implementation + Testing + Docs)
