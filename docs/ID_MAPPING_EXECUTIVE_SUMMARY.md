# ID Mapping & Hierarchy - Executive Summary

**Date:** October 2, 2024  
**Status:** Production System - Code-Referenced Analysis

---

## TL;DR

**SDK Principle:** Runtime ID = Array Index  
**Dual Storage:** Components (`m_Objects`) vs Animations (`m_Animations`)  
**Converter Principle:** JSON localId → Array Index Mapping  
**Resolution:** `artboard->resolve(id)` = `m_Objects[id]` (O(1))  
**Critical:** `resolve()` ONLY searches `m_Objects`, NOT `m_Animations`

---

## SDK ID ARCHITECTURE

### Core Design
```cpp
// SDK: Direct array index lookup
Core* Artboard::resolve(uint32_t id) const {
    return m_Objects[id];  // O(1) - NO HASH, NO MAP!
}
```

**File:** `src/artboard.cpp:586-593`

### Key Properties
| Property | Key | Type | Purpose |
|----------|-----|------|---------|
| `parentId` | 5 | uint32 | Parent component index |
| `objectId` | 51 | uint32 | Animation target index |
| `targetId` | 173 | uint32 | Constraint target index |
| `interpolatorId` | 69 | uint32 | Interpolator index |

**All values are array indices into `Artboard::m_Objects`**

**Animation Graph Exception:** KeyedObject, KeyedProperty, KeyFrame types are NOT in `m_Objects`

---

## ARTBOARD DUAL STORAGE

### Separate Collections
```cpp
// File: include/rive/artboard.hpp:69-143
class Artboard {
private:
    std::vector<Core*> m_Objects;              // Components only
    std::vector<LinearAnimation*> m_Animations; // Animations only
    
public:
    void addObject(Core* object);               // Component path
    void addAnimation(LinearAnimation* object); // Animation path
    Core* resolve(uint32_t id);                 // m_Objects ONLY!
};
```

### Animation Graph Hierarchy
```
Artboard
├─ m_Objects[0] = Artboard
├─ m_Objects[1] = Shape
├─ m_Objects[2] = Rectangle
└─ m_Animations[0] = LinearAnimation (NOT in m_Objects!)
    └─ KeyedObject (objectId = 2, references Rectangle)
        └─ KeyedProperty
            └─ KeyFrame
```

**Key Insight:** Animation graph bypasses Component hierarchy entirely.

---

## CONVERTER ID MAPPING

### 3-Pass Architecture

```
PASS 0: Type Mapping
  ├─ Build localIdToType[localId] → typeKey
  ├─ Detect forward references
  └─ Build parent dependency graph

PASS 1: Object Creation
  ├─ Topological sort (parent-first)
  ├─ Create runtime objects
  ├─ Map: localIdToBuilderObjectId[jsonLocalId] → runtimeIndex
  └─ Set direct properties (name, transform, etc.)

PASS 3: ID Remapping
  ├─ Remap objectId (KeyedObject → Component)
  ├─ Remap targetId (Constraint → Component)
  ├─ Remap interpolatorId (KeyFrame → Interpolator)
  └─ Remap drawableId, styleId, sourceId
```

**File:** `converter/src/universal_builder.cpp:738-1617`

### Animation Graph Handling
```cpp
// File: converter/src/universal_builder.cpp:101-113
static bool isAnimGraphType(uint16_t typeKey) {
    return typeKey == 25 ||  // KeyedObject
           typeKey == 26 ||  // KeyedProperty
           typeKey == 30 ||  // KeyFrameDouble
           typeKey == 31 ||  // LinearAnimation
           // ... (other KeyFrame/Interpolator types)
}

// Skip setParent() for animation graph
if (isAnimGraphType(pending.typeKey)) {
    continue;  // Animation system manages these!
}
```

**Reason:** SDK routes animation objects via ImportStack, not Component hierarchy.

### Critical Code: ID Remapping
```cpp
// PASS 1: Build mapping table
uint32_t builderId = builder.getObjectCount() - 1;  // Array index
localIdToBuilderObjectId[jsonLocalId] = builderId;

// PASS 3: Remap references
else if (key == "objectId") {
    uint32_t localId = value.get<uint32_t>();
    auto it = idMapping.find(localId);
    if (it != idMapping.end()) {
        builder.set(obj, 51, it->second);  // Property key 51 = objectId
    }
}
```

**File:** `converter/src/universal_builder.cpp:362-375, 1100`

---

## PARENT-CHILD HIERARCHY

### SDK: Two-Phase Resolution

```cpp
// Phase 1: onAddedDirty - Resolve direct references
StatusCode Component::onAddedDirty(CoreContext* context) {
    m_Parent = context->resolve(parentId())->as<ContainerComponent>();
    m_Parent->addChild(this);  // Link pointers
    return StatusCode::Ok;
}

// Phase 2: onAddedClean - Resolve multi-level hierarchy
StatusCode TransformComponent::onAddedClean(CoreContext* context) {
    m_ParentTransformComponent = 
        parent()->is<WorldTransformComponent>() 
            ? parent()->as<WorldTransformComponent>() 
            : nullptr;
    return StatusCode::Ok;
}
```

**Files:**  
- `src/component.cpp:18-29`  
- `src/transform_component.cpp:9-15`

### Converter: Pointer → ID Export

```cpp
// Export: Runtime pointer → JSON localId
if (auto* comp = dynamic_cast<Component*>(obj)) {
    auto* parent = comp->parent();  // Runtime pointer!
    if (parent) {
        auto it = compToLocalId.find(parent);
        if (it != compToLocalId.end()) {
            objJson["parentId"] = it->second;  // Remap to localId
        }
    }
}
```

**File:** `converter/universal_extractor.cpp:241-329`

---

## KEYED DATA RESOLUTION

### Animation Target Lookup (Hot Path)

```cpp
void KeyedObject::apply(Artboard* artboard, float time, float mix) {
    Core* object = artboard->resolve(objectId());  // ← EVERY FRAME!
    if (object == nullptr) return;
    
    for (auto& property : m_keyedProperties) {
        property->apply(object, time, mix);
    }
}
```

**File:** `src/animation/keyed_object.cpp:76-91`

**Performance:** O(1) array access - critical for animation hot path

---

## CRITICAL FINDINGS

### ✅ Architecture Strengths
1. **O(1) Resolution:** Array index lookup is cache-friendly
2. **Platform Independent:** No pointer serialization in .riv
3. **Graceful Degradation:** NULL objects, invalid IDs handled
4. **Type Safety:** `resolve()` + `is<T>()` validation

### ⚠️ Architectural Limitations

#### 1. Runtime Objects Invisible
```cpp
uint32_t Artboard::idOf(Core* object) const {
    auto it = std::find(m_Objects.begin(), m_Objects.end(), object);
    return (it != m_Objects.end()) ? (it - m_Objects.begin()) : 0;
}
```
**File:** `src/artboard.cpp:595-607`

**Problem:** Objects created post-import (runtime animations) return `idOf() == 0`  
**Impact:** Can't export runtime-created objects (multi round-trip blocked)

#### 2. Linear Reverse Lookup
- **Forward:** `resolve(id)` → `O(1)`  
- **Reverse:** `idOf(pointer)` → `O(n)` linear search  
- **Impact:** Extractor performance degradation on large artboards

#### 3. Index Alignment Required
```cpp
bool ArtboardImporter::readNullObject() {
    addComponent(nullptr);  // NULL placeholder!
    return true;
}
```
**File:** `src/importers/artboard_importer.cpp:43-46`

**Requirement:** NULL objects must be imported to preserve indices  
**Impact:** Converter must emit NULL placeholders for skipped objects

---

## SYNTHETIC ID WORKAROUND

### Problem
Runtime objects (e.g., KeyFrames) have `objectId == 0` (not in `m_Objects`)

### Solution
```cpp
// Extractor: Assign synthetic IDs to runtime objects
std::map<const KeyedObject*, uint32_t> koToSyntheticId;
uint32_t nextSyntheticId = 0x80000000;  // Start at 2^31

if (ko->objectId() == 0) {
    koToSyntheticId[ko] = nextSyntheticId++;  // Per-KeyedObject ID
    coreIdToLocalId[nextSyntheticId - 1] = localId;
}
```

**File:** `converter/universal_extractor.cpp:234-280`

**Limitation:** Can't access target pointer from KeyedObject (SDK API missing)  
**Result:** Single round-trip works, multi round-trip blocked

---

## ID REMAP FAILURE MODES

| Failure | Symptom | Root Cause | Mitigation |
|---------|---------|------------|------------|
| **Dangling objectId** | Animation skipped | Target deleted | Validation |
| **Forward reference** | NULL object | Child before parent | Topological sort |
| **Index misalignment** | Crash/corruption | NULL not preserved | Import NULL |
| **Circular parent** | Hang | Parent cycle | Cycle detection |
| **Synthetic collision** | Wrong target | ID overlap | 0x80000000+ space |

---

## PROOF MATRIX

| Component | Evidence File | Lines | Proof Type |
|-----------|--------------|-------|------------|
| **SDK: ID = Array Index** | `artboard.cpp` | 586-607 | Implementation |
| **SDK: Dual Storage** | `artboard.hpp` | 69-143 | Architecture |
| **SDK: parentId Property** | `component_base.hpp` | 33-59 | Generated |
| **SDK: objectId Property** | `keyed_object_base.hpp` | 30-56 | Generated |
| **SDK: Parent Resolution** | `component.cpp` | 18-29 | Implementation |
| **SDK: Animation Target** | `keyed_object.cpp` | 76-91 | Hot Path |
| **SDK: ImportStack Routing** | `keyed_object.cpp` | 93-104 | Ownership |
| **Converter: Animation Types** | `universal_builder.cpp` | 101-113 | Classification |
| **Converter: Type Mapping** | `universal_builder.cpp` | 738-766 | PASS 0 |
| **Converter: Topological Sort** | `universal_builder.cpp` | 782-920 | PASS 1 |
| **Converter: ID Remapping** | `universal_builder.cpp` | 362-375 | Property |
| **Converter: PASS3 Remap** | `universal_builder.cpp` | 1403-1617 | Deferred |
| **Converter: Skip setParent** | `universal_builder.cpp` | 1531-1534 | Exclusion |
| **Extractor: Parent Export** | `universal_extractor.cpp` | 241-329 | Pointer→ID |
| **Extractor: Animation Hierarchy** | `universal_extractor.cpp` | 649-804 | Parent chain |
| **Extractor: Synthetic IDs** | `universal_extractor.cpp` | 234-280 | Workaround |

---

## PRODUCTION STATUS

| Feature | Status | Coverage |
|---------|--------|----------|
| Builder ID Remap | ✅ Production | 100% |
| Extractor ID Remap | ✅ Production | Single round-trip |
| Parent-Child Hierarchy | ✅ Production | 100% |
| Keyed Data (File Objects) | ✅ Production | 100% |
| Keyed Data (Runtime Objects) | ⚠️ Limited | Synthetic IDs |
| Multi Round-Trip | ❌ Blocked | SDK `resolve()` limitation |

---

## REFERENCES

### Key SDK Files
- `include/rive/core.hpp` - Base Core class (no ID field)
- `include/rive/generated/component_base.hpp` - parentId property
- `include/rive/generated/animation/keyed_object_base.hpp` - objectId property
- `src/artboard.cpp` - resolve() / idOf() implementation
- `src/component.cpp` - Parent linking (onAddedDirty)

### Key Converter Files
- `converter/src/universal_builder.cpp` - 3-pass ID remapping
- `converter/universal_extractor.cpp` - Pointer→ID export + synthetic IDs
- `converter/src/serializer.cpp` - Property serialization

### Documentation
- `docs/ID_MAPPING_HIERARCHY_ANALYSIS.md` - Full Turkish analysis with code proofs
- `AGENTS.md` - Repository rules (Section 17: PR-KEYED-DATA-EXPORT)
- `docs/NEXT_SESSION_HIERARCHICAL.md` - Implementation guide

---

**Report End** • All code references verified • October 2, 2024

### Additional SDK Proofs

```1:95: