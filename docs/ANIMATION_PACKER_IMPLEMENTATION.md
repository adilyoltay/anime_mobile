# 🎯 Animation Data Packer - Implementation Guide

**Goal:** Identical object counts between original and round-trip RIV files  
**Method:** Pack hierarchical keyframes/interpolators → binary blobs (types 8064/7776)  
**Complexity:** High (8-12 hours)  
**Priority:** Low (current behavior is correct and lossless)

---

## 📋 Problem Statement

### Current Behavior
```
Original RIV:    540 objects (packed blobs)
Round-Trip RIV: 1135 objects (hierarchical)
Growth: 2.1x (expected, harmless)
```

### Packed Format (Original)
```
KeyedProperty (26) → contains packed blob
  ├─ Type 8064: Packed keyframe array
  │   └─ Multiple keyframes in single binary buffer
  └─ Type 7776: Packed interpolator array
      └─ Multiple interpolators in single binary buffer
```

### Hierarchical Format (Current Round-Trip)
```
KeyedProperty (26)
  ├─ KeyFrameDouble (30) - individual object
  ├─ CubicEaseInterpolator (28) - individual object
  ├─ KeyFrameDouble (30) - individual object
  └─ ... (expanded tree)
```

---

## 🎯 Two Implementation Approaches

### Option A: Converter Packer (Recommended)

**Pros:**
- Flexible: Works with any JSON input
- Clean separation: Extractor stays simple
- Full control over packing logic

**Cons:**
- Complex implementation
- Must mirror official serializer
- Requires deep format knowledge

### Option B: Extractor Preservation

**Pros:**
- Simpler: Just preserve original bytes
- Guaranteed identical output
- Less code to write

**Cons:**
- Less flexible: Can't edit animation data
- Dual format: Packed + hierarchical in JSON
- Larger JSON files

---

## 🛠️ Option A: Converter Packer Implementation

### Architecture

```
PASS 1: Build objects (current)
  ↓
PASS 2: Set parents (current)
  ↓
PASS 3: Remap IDs (current)
  ↓
NEW PASS 4: Pack Animation Data
  ├─ Detect KeyedProperty trees
  ├─ Bucket compatible keyframes
  ├─ Emit packed blob (type 8064)
  ├─ Bucket interpolators
  └─ Emit packed blob (type 7776)
  ↓
Serialize (modified: skip hierarchical if packed)
```

### Step 1: Detect KeyedProperty Trees

```cpp
// In universal_builder.cpp after PASS 3

struct KeyedPropertyContext {
    CoreObject* keyedProperty;
    std::vector<CoreObject*> keyframes;
    std::vector<CoreObject*> interpolators;
    uint16_t keyframeType; // 30/37/50/84/142/450
};

std::vector<KeyedPropertyContext> collectKeyedProperties(
    const std::vector<PendingObject>& objects) 
{
    std::vector<KeyedPropertyContext> contexts;
    std::unordered_map<uint32_t, KeyedPropertyContext*> kpMap;
    
    // Pass 1: Find all KeyedProperty objects
    for (const auto& pending : objects) {
        if (pending.typeKey == 26) { // KeyedProperty
            contexts.push_back({pending.obj, {}, {}, 0});
            kpMap[pending.localId.value()] = &contexts.back();
        }
    }
    
    // Pass 2: Collect children (keyframes/interpolators)
    for (const auto& pending : objects) {
        uint16_t tk = pending.typeKey;
        
        // Keyframe types
        if (tk == 30 || tk == 37 || tk == 50 || 
            tk == 84 || tk == 142 || tk == 450) {
            
            auto it = kpMap.find(pending.parentLocalId);
            if (it != kpMap.end()) {
                it->second->keyframes.push_back(pending.obj);
                it->second->keyframeType = tk;
            }
        }
        
        // Interpolator types
        if (tk == 28 || tk == 138 || tk == 139 || tk == 174 || tk == 175) {
            auto it = kpMap.find(pending.parentLocalId);
            if (it != kpMap.end()) {
                it->second->interpolators.push_back(pending.obj);
            }
        }
    }
    
    return contexts;
}
```

### Step 2: Create Packed Blobs

```cpp
// Type 8064: Packed KeyFrame Blob
rive::Core* createPackedKeyframes(
    const std::vector<CoreObject*>& keyframes,
    uint16_t keyframeType)
{
    // Create packed container
    // This is the hard part - need to reverse-engineer Rive's format!
    
    // Pseudo-structure (needs verification from Rive source):
    struct PackedKeyframes {
        uint32_t count;
        struct Entry {
            float time;      // frame time
            float value;     // keyframe value
            uint32_t interpId; // interpolator reference
            // ... additional fields based on type
        };
        Entry entries[count];
    };
    
    // This requires deep knowledge of Rive's binary format
    // May need to analyze original .riv files extensively
    
    return nullptr; // Placeholder
}
```

### Step 3: Replace Hierarchical with Packed

```cpp
void replaceWithPackedBlobs(
    CoreBuilder& builder,
    const std::vector<KeyedPropertyContext>& contexts)
{
    for (const auto& ctx : contexts) {
        if (ctx.keyframes.size() > 1) { // Only pack if multiple frames
            
            // Create packed blob
            auto* packedKF = createPackedKeyframes(
                ctx.keyframes, 
                ctx.keyframeType
            );
            
            if (packedKF) {
                // Add packed blob as child of KeyedProperty
                builder.addCore(*packedKF);
                builder.setParent(*packedKF, *ctx.keyedProperty);
                
                // REMOVE original keyframes from serialization
                for (auto* kf : ctx.keyframes) {
                    markForSkip(kf); // Flag to skip in serializer
                }
            }
        }
        
        // Same for interpolators → type 7776
        if (ctx.interpolators.size() > 1) {
            // Similar logic for interpolators
        }
    }
}
```

### Step 4: Modify Serializer

```cpp
// In serializer.cpp
void serialize_core_document(...) {
    for (auto* obj : objects) {
        // Skip if marked (replaced by packed blob)
        if (isMarkedForSkip(obj)) continue;
        
        // ... existing serialization
    }
}
```

---

## 🛠️ Option B: Extractor Preservation Implementation

### Step 1: Detect Packed Blobs in Extractor

```cpp
// In universal_extractor.cpp

struct PackedBlob {
    uint16_t typeKey; // 8064, 7776, 64
    std::vector<uint8_t> rawBytes;
    uint32_t parentId;
};

std::vector<PackedBlob> extractPackedBlobs(File* file) {
    std::vector<PackedBlob> blobs;
    
    // During extraction, when encountering type 8064/7776:
    for (auto* obj : artboard->objects()) {
        uint16_t tk = obj->coreType();
        
        if (tk == 8064 || tk == 7776 || tk == 64) {
            PackedBlob blob;
            blob.typeKey = tk;
            
            // Serialize this object to raw bytes
            // (requires access to BinaryWriter internals)
            blob.rawBytes = serializeToBytes(obj);
            blob.parentId = getLocalId(obj->parent());
            
            blobs.push_back(blob);
        }
    }
    
    return blobs;
}
```

### Step 2: Dual Format JSON

```json
{
  "artboards": [{
    "objects": [
      {
        "typeKey": 26,
        "localId": 100,
        "properties": {},
        "packedData": {
          "keyframes": {
            "typeKey": 8064,
            "rawBytes": "base64_encoded_blob..."
          },
          "interpolators": {
            "typeKey": 7776,
            "rawBytes": "base64_encoded_blob..."
          }
        },
        "hierarchical": [
          {"typeKey": 30, "localId": 101, ...},
          {"typeKey": 28, "localId": 102, ...}
        ]
      }
    ]
  }]
}
```

### Step 3: Converter Writes Packed Data

```cpp
// In universal_builder.cpp

if (objJson.contains("packedData")) {
    auto& packed = objJson["packedData"];
    
    // Deserialize raw bytes → Core object
    if (packed.contains("keyframes")) {
        std::string base64 = packed["keyframes"]["rawBytes"];
        auto rawBytes = base64Decode(base64);
        
        // Create Core object from bytes
        auto* packedObj = deserializeFromBytes(rawBytes);
        builder.addCore(*packedObj);
        builder.setParent(*packedObj, obj);
    }
    
    // Skip hierarchical children (already in packed form)
    continue;
}
```

---

## 🔍 Required Research

### Rive Format Analysis Needed

1. **Type 8064 Structure:**
   - What fields does it contain?
   - How are keyframes stored?
   - What's the binary layout?

2. **Type 7776 Structure:**
   - Interpolator array format
   - How are they linked to keyframes?

3. **Type 64 Usage:**
   - When is this used vs 8064?

### Tools for Investigation

```bash
# Analyze original RIV files
python3 converter/analyze_riv.py original.riv > analysis.txt

# Extract and compare
hexdump -C original.riv | grep "8064\|7776" > packed_sections.hex

# Study runtime source (if available)
# Look for: BinaryWriter::writeKeyedProperty, writeKeyFrames
```

---

## 📊 Effort Estimation

### Option A (Packer)
- **Research:** 2-3 hours (analyze packed format)
- **Implementation:** 4-6 hours (packer logic)
- **Testing:** 2-3 hours (validate output)
- **Total:** 8-12 hours

### Option B (Preservation)
- **Extractor:** 2-3 hours (raw byte extraction)
- **JSON format:** 1 hour (dual format schema)
- **Converter:** 2-3 hours (deserialization)
- **Testing:** 1-2 hours
- **Total:** 6-9 hours

---

## ✅ Validation Criteria

After implementation, verify:

```bash
# Extract
./universal_extractor original.riv extracted.json

# Convert
./rive_convert_cli extracted.json roundtrip.riv

# Compare object counts
python3 -c "
import subprocess
orig = subprocess.check_output(['python3', 'converter/analyze_riv.py', 'original.riv'])
rt = subprocess.check_output(['python3', 'converter/analyze_riv.py', 'roundtrip.riv'])

orig_count = orig.decode().count('Object type_')
rt_count = rt.decode().count('Object type_')

print(f'Original: {orig_count} objects')
print(f'Round-trip: {rt_count} objects')
print(f'Match: {orig_count == rt_count}')
"

# Expected: Match: True ✅
```

---

## ⚠️ Risks & Considerations

### Implementation Risks
1. **Format Complexity:** Packed format may be complex/undocumented
2. **Version Compatibility:** Format may change between Rive versions
3. **Maintenance:** Requires keeping in sync with official runtime

### Alternative: Accept Current Behavior
- ✅ Current implementation is **correct**
- ✅ Runtime re-packs automatically (lossless)
- ✅ All tests passing
- ✅ File size increase is acceptable for most use cases

### When to Implement
Only if:
- File size is critical constraint
- Need exact binary reproduction
- Have access to official format documentation
- Team has time for 8-12 hour task

---

## 🎯 Recommendation

**Current Status:** ✅ **NO ACTION NEEDED**

The 2x object count increase is:
- ✅ Expected (format conversion)
- ✅ Harmless (lossless round-trip)
- ✅ Correct (runtime handles it)

**Implement packer only if:**
- Customer requirement for exact reproduction
- File size is critical (rare)
- Official format documentation available

**Priority:** 🟢 LOW (Enhancement, not bug)

---

**Last Updated:** October 1, 2024  
**Status:** Planning Document - Not Implemented  
**Decision:** Accept current behavior as correct
