# ID Mapping Quick Reference Card

## 🎯 Core Principle
**SDK:** `ID = Array Index`  
**Converter:** `JSON localId → Array Index Mapping`

---

## 📊 Property Keys Reference

| Property | Key | Type | Source → Target | When Resolved |
|----------|-----|------|-----------------|---------------|
| `parentId` | 5 | uint32 | Component → Container | PASS1 (direct) |
| `objectId` | 51 | uint32 | KeyedObject → Component | PASS1 (direct) |
| `interpolatorId` | 69 | uint32 | KeyFrame → Interpolator | PASS3 (deferred) |
| `drawableId` | 119 | uint32 | DrawTarget → Drawable | PASS3 (deferred) |
| `drawTargetId` | 121 | uint32 | DrawRules → DrawTarget | PASS3 (deferred) |
| `targetId` | 173 | uint32 | Constraint → Component | PASS3 (deferred) |
| `styleId` | 272 | uint32 | TextStylePaint → TextStyle | PASS3 (deferred) |

---

## 🔧 SDK APIs

### Resolve ID → Object
```cpp
Core* obj = artboard->resolve(id);  // O(1) - Direct array access
```

### Resolve Object → ID
```cpp
uint32_t id = artboard->idOf(obj);  // O(n) - Linear search
// WARNING: Runtime objects return 0!
```

### Parent Resolution
```cpp
// Phase 1: onAddedDirty
m_Parent = context->resolve(parentId())->as<ContainerComponent>();

// Phase 2: onAddedClean
m_ParentTransformComponent = parent()->as<WorldTransformComponent>();
```

---

## 🏗️ Converter 3-Pass Architecture

### PASS 0: Type Mapping
```cpp
localIdToType[localId] = typeKey;
localIdToParent[localId] = parentId;
```

### PASS 1: Object Creation
```cpp
// Topological sort (parent-first)
visitObject(parent);  // Recurse
visitObject(child);

// Create & map
CoreObject* obj = builder.addCore(new Type());
uint32_t builderId = builder.getObjectCount() - 1;
localIdToBuilderObjectId[jsonLocalId] = builderId;
```

### PASS 3: ID Remapping
```cpp
// Remap deferred references
auto it = localIdToBuilderObjectId.find(jsonLocalId);
if (it != idMapping.end()) {
    builder.set(obj, propertyKey, it->second);
}
```

---

## 🚨 Common Pitfalls

### ❌ DON'T: Write ID directly
```cpp
// BAD: JSON localId written as-is
builder.set(obj, 51, jsonLocalId);  // Wrong!
```

### ✅ DO: Remap via mapping table
```cpp
// GOOD: Lookup runtime ID first
auto it = idMapping.find(jsonLocalId);
if (it != idMapping.end()) {
    builder.set(obj, 51, it->second);  // Correct!
}
```

### ❌ DON'T: Assume order
```cpp
// BAD: Assume parent appears first in JSON
createObject(child);
createObject(parent);  // Too late!
```

### ✅ DO: Topological sort
```cpp
// GOOD: Parent-first ordering
std::function<void(json)> visitObject = [&](auto& obj) {
    visitObject(parent);  // Recurse
    createObject(obj);
};
```

---

## 🔍 Debugging Checklist

### ID Remap Failures
- [ ] Check `localIdToBuilderObjectId` contains source ID
- [ ] Verify target object was created (not skipped)
- [ ] Confirm no forward references (child before parent)
- [ ] Check for circular parent dependencies

### NULL Objects
- [ ] Verify topological sort (parent-first)
- [ ] Check `__unsupported__` flag handling
- [ ] Confirm NULL placeholders emitted (index alignment)

### Animation Issues
- [ ] Verify `objectId` remapped correctly (property key 51)
- [ ] Check KeyedObject target exists in `m_Objects`
- [ ] Confirm runtime objects have synthetic IDs (0x80000000+)

### Constraint Issues
- [ ] Verify `targetId` remapped (property key 173)
- [ ] Check constraint target type (valid for constraint)
- [ ] Confirm `missingId = -1` handled explicitly

---

## 📈 Performance Notes

| Operation | Complexity | Location |
|-----------|-----------|----------|
| `resolve(id)` | O(1) | Hot path (every frame) |
| `idOf(obj)` | O(n) | Cold path (export only) |
| Topological sort | O(V+E) | Build time |
| ID remapping | O(n) | Build time |

---

## 🎓 Learning Path

1. **Read:** `docs/ID_MAPPING_EXECUTIVE_SUMMARY.md` (English overview)
2. **Deep Dive:** `docs/ID_MAPPING_HIERARCHY_ANALYSIS.md` (Turkish, code proofs)
3. **Implement:** `converter/src/universal_builder.cpp` (3-pass pattern)
4. **Export:** `converter/universal_extractor.cpp` (pointer→ID remapping)
5. **Test:** `converter/import_test.cpp` (validation)

---

## 📞 Key Files

| Purpose | File | Key Functions |
|---------|------|---------------|
| SDK Resolve | `src/artboard.cpp` | `resolve()`, `idOf()` |
| SDK Parent Link | `src/component.cpp` | `onAddedDirty()` |
| Builder PASS1 | `universal_builder.cpp` | Object creation |
| Builder PASS3 | `universal_builder.cpp` | ID remapping |
| Extractor | `universal_extractor.cpp` | Pointer→ID export |

---

**Quick Ref v1.0** • October 2, 2024

## 🧭 Topological Sort (Extractor + Builder)

```71:118:converter/extractor_postprocess.hpp
// Topological sort: parents before children + keyed data after targets
for (size_t i = 0; i < objects.size(); i++) {
    // KeyedObject (25) has objectId reference
    if (typeKey == 25 && objects[i].contains("properties")) {
        if (props.contains("objectId")) {
            uint32_t targetId = props["objectId"];
            // KeyedObject depends on its target (must come after)
            refDependencies[targetId].push_back(localId);
        }
    }
}
```

```783:821:converter/src/universal_builder.cpp
// PASS 1: Topological sort + KeyedObject waits for target
bool parentReady = (typeKeyCandidate == 1) || (emittedLocalIds.count(parentId) > 0);
if (parentReady && typeKeyCandidate == 25 && objJson.contains("properties")) {
    if (props.contains("objectId")) {
        uint32_t targetLocalId = props["objectId"].get<uint32_t>();
        if (emittedLocalIds.count(targetLocalId) == 0) {
            parentReady = false;
        }
    }
}
```

