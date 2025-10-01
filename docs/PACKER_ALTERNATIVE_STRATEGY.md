# 🎯 Animation Packer - Alternative Strategy

**Date:** October 1, 2024, 21:40  
**Status:** 🟡 EXPERIMENTAL - Simple Repacking Approach  
**Risk Level:** MEDIUM (not attempting Rive's proprietary format)

---

## 💡 New Strategy: Custom Compact Format

Instead of reverse-engineering Rive's packed blobs (8064/7776),  
create our **own compact representation** that's:
- Simpler to implement
- Fully documented
- Easier to debug
- Still reduces file size significantly

---

## 🎯 Approach: Property Deduplication

### Current Bloat Analysis

**Bee_baby Round-Trip:**
```
KeyFrameDouble (30): 345 objects
Each has properties:
  - frame (67): timestamp
  - value (68): keyframe value  
  - interpolatorId (69): reference
  
Total: 345 × 3 properties = 1,035 property writes
```

### Observation: Massive Redundancy

**Same properties repeated 345 times:**
- Property KEY written 345 times (wasteful!)
- Only VALUES change

### Solution: Array Format

**Instead of:**
```json
{typeKey: 30, properties: {67: 0.0, 68: 1.0, 69: 5}}
{typeKey: 30, properties: {67: 0.5, 68: 2.0, 69: 5}}
{typeKey: 30, properties: {67: 1.0, 68: 1.5, 69: 5}}
```

**Write as:**
```json
{
  typeKey: 999,  // Custom "KeyFrameArray" type
  propertyKeys: [67, 68, 69],  // Write ONCE
  values: [
    [0.0, 1.0, 5],
    [0.5, 2.0, 5],
    [1.0, 1.5, 5]
  ]
}
```

**Savings:**
- Property keys: 1,035 → 3 writes (99.7% reduction!)
- File size: ~50% smaller (estimate)

---

## 🛠️ Implementation Plan

### Step 1: Define Custom Type

```cpp
// New typeKey: 999 = KeyFrameDoubleArray
const uint16_t TYPE_KEYFRAME_DOUBLE_ARRAY = 999;

// Properties:
// - 9990: property key list (array of uint16_t)
// - 9991: value matrix (flattened array)
// - 9992: row count
// - 9993: column count
```

### Step 2: Detect Compatible KeyFrames

```cpp
// In PASS 4: After all objects created
std::vector<std::vector<CoreObject*>> bucketKeyFrames(
    const std::vector<PendingObject>& objects)
{
    std::map<uint32_t, std::vector<CoreObject*>> byParent;
    
    for (const auto& obj : objects) {
        if (obj.typeKey == 30) {  // KeyFrameDouble
            byParent[obj.parentLocalId].push_back(obj.coreObj);
        }
    }
    
    std::vector<std::vector<CoreObject*>> buckets;
    for (const auto& [parent, frames] : byParent) {
        if (frames.size() >= 3) {  // Only pack if 3+ frames
            buckets.push_back(frames);
        }
    }
    
    return buckets;
}
```

### Step 3: Create Array Objects

```cpp
void createKeyFrameArrays(
    CoreBuilder& builder,
    const std::vector<std::vector<CoreObject*>>& buckets)
{
    for (const auto& frames : buckets) {
        // Create array container
        auto* arrayObj = new CustomKeyFrameArray();
        arrayObj->frameCount = frames.size();
        
        // Extract property values
        std::vector<float> frameValues;
        std::vector<float> keyValues;
        std::vector<uint32_t> interpIds;
        
        for (auto* frame : frames) {
            frameValues.push_back(getProperty(frame, 67));  // frame time
            keyValues.push_back(getProperty(frame, 68));    // value
            interpIds.push_back(getProperty(frame, 69));    // interpolatorId
        }
        
        // Store in array format
        arrayObj->setFrameTimes(frameValues);
        arrayObj->setValues(keyValues);
        arrayObj->setInterpolatorIds(interpIds);
        
        builder.addCore(*arrayObj);
        
        // Mark original frames for skip
        for (auto* frame : frames) {
            markForSkip(frame);
        }
    }
}
```

### Step 4: Serialize Array Format

```cpp
// In serializer.cpp
if (obj->typeKey() == 999) {  // KeyFrameDoubleArray
    auto* arr = static_cast<CustomKeyFrameArray*>(obj);
    
    // Write custom format
    writeVarUint(999);  // typeKey
    
    // Property 9990: key list
    writeVarUint(9990);  // property key
    writeVarUint(3);     // array length
    writeVarUint(67);    // frame
    writeVarUint(68);    // value
    writeVarUint(69);    // interpolatorId
    
    // Property 9991: value matrix (flattened)
    writeVarUint(9991);
    writeVarUint(arr->frameCount * 3);  // total values
    for (size_t i = 0; i < arr->frameCount; i++) {
        writeDouble(arr->frameTimes[i]);
        writeDouble(arr->values[i]);
        writeVarUint(arr->interpolatorIds[i]);
    }
}
```

---

## ⚠️ Problem: Runtime Compatibility

**Critical Issue:** Runtime won't recognize type 999!

**Solutions:**

### Option A: Dual Format (SAFE)
```cpp
// Write BOTH formats:
1. Custom array (for file size)
2. Individual keyframes (for runtime)

// Runtime ignores type 999, uses individual frames
// File is larger than pure packed, but smaller than current
```

### Option B: Runtime Patch (UNSAFE)
```cpp
// Modify runtime to understand type 999
// Requires Rive runtime source access
// HIGH RISK - not recommended
```

### Option C: Pre-Expansion (HYBRID)
```cpp
// Converter writes compact format
// Separate tool expands before Rive import
//   compact.riv → expander → full.riv → Rive
```

---

## 📊 Expected Results

### File Size Comparison

```
Current:
  bee_baby.riv round-trip: 19KB
  345 KeyFrameDouble objects
  
With Custom Array:
  ~12KB (estimated 35% reduction)
  1 KeyFrameDoubleArray + metadata
  
Rive Original (ideal):
  9.5KB (native packed format)
```

### Trade-offs

| Aspect | Custom Array | Rive Native | Current |
|--------|--------------|-------------|---------|
| **File Size** | ~12KB | 9.5KB ✅ | 19KB |
| **Compatibility** | ❌ Needs dual format | ✅ | ✅ |
| **Maintenance** | 🟡 Our code | ❌ Proprietary | ✅ |
| **Risk** | 🟡 Medium | ❌ High | ✅ Low |

---

## 🎯 Recommendation

### If Accepting Risk:

**Try Option A (Dual Format):**
1. Implement custom array format
2. Write BOTH array + individual frames
3. Runtime uses individual frames (ignores array)
4. File size: ~15KB (smaller than current 19KB)
5. Risk: LOW (runtime fallback guaranteed)

**Effort:** 6-8 hours

### Conservative:

**Keep current solution:**
- ✅ Works perfectly
- ✅ Zero risk
- ⚠️ 2x file size

---

## 🚀 Decision Point

**Question:** Which approach?

1. **🟢 Conservative:** Accept current (19KB, safe)
2. **🟡 Custom Array:** Dual format (15KB, medium risk, 6-8h)
3. **🔴 Rive Native:** Reverse engineer (9.5KB, high risk, 40h+)

**My Recommendation:** 
- If file size is critical: Try #2 (Custom Array with dual format)
- Otherwise: Stick with #1 (Current solution)

---

**Status:** Waiting for decision  
**Next:** Implement based on choice
