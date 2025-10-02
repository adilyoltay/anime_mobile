# PR Plan: Grey Screen Fix - Implementation Guide

**Tarih:** 2 Ekim 2025, 08:57 (UPDATED)  
**Status:** 🔴 READY FOR IMPLEMENTATION  
**Priority:** CRITICAL  
**Total ETA:** 12-15 hours  
**Validation:** ✅ CONFIRMED with codebase analysis

---

## 🎯 Executive Summary

Grey screen sorunu **multi-layer root cause**. İki ana PR gerekli:

```
🔴 PR-DRAWTARGET (PRIMARY - CRITICAL)
   → DrawTarget extraction & building (typeKey 48)
   → Packed draw chunks (typeKey 4992) - OPTIONAL
   → m_FirstDrawable linked list fix
   → ETA: 6-8 hours (DrawRules not needed!)

🟡 PR-ORPHAN-FIX (SECONDARY - HIGH)  
   → Orphan Fill/Stroke auto-fix
   → Depends on PR-DRAWTARGET
   → ETA: 2-3 hours

🟢 PR-VALIDATION (MANDATORY)
   → Comprehensive tests
   → ETA: 1-2 hours
```

**✅ VERIFIED:** bee_baby.riv contains DrawTarget (typeKey 48) but extractor SKIPS it!
**CRITICAL:** PR-DRAWTARGET mutlaka önce yapılmalı!

---

## 🔬 Root Cause - Final (CODEBASE VERIFIED)

### LAYER 1 (PRIMARY): Draw Order Graph Missing 🔴

**Problem (CONFIRMED):**
```bash
# Original bee_baby.riv analysis:
python3 converter/analyze_riv.py bee_baby.riv | grep "type_48"
# Output: 2 DrawTarget objects found ✅

# Extracted JSON check:
jq '.artboards[0].objects | map(select(.typeKey == 48))' output.json
# Output: [] (EMPTY!) ❌

# Reason: universal_extractor.cpp does NOT handle DrawTarget!
# Line 72-110: getTypeName() missing case 48
# Line 184-400: No DrawTarget extraction code
```

**Runtime Impact:**
```cpp
// artboard.cpp:217-250 - Artboard::initialize()
std::unordered_map<Core*, DrawRules*> componentDrawRules;

// Without DrawTarget objects:
m_DrawTargets = []  // EMPTY VECTOR!
m_FirstDrawable = nullptr  // NO RENDER LIST!

// Render loop (line 1244):
for (auto drawable = m_FirstDrawable; drawable; drawable = drawable->prev) {
    // ❌ NEVER EXECUTES - List is empty
    drawable->draw(renderer);
}
```

**Visual Result:** 🔲 **GRİ EKRAN**

**Property Keys (VERIFIED from draw_target_base.hpp):**
- Property 119: drawableIdPropertyKey
- Property 120: placementValuePropertyKey

---

### LAYER 2 (SECONDARY): Orphan Fill/Stroke 🟡

**Problem:**
```json
{
  "localId": 203,
  "parentId": 0,     // ARTBOARD (should be Shape!)
  "typeKey": 20      // Fill
}
```

**Impact:** Fill without Shape → No geometry → Partial render

---

## 📋 PR-DRAWTARGET: Draw Order Graph

### ETA: 8-12 hours | Priority: 🔴 CRITICAL

### Phase 0: Investigation (30 min) ✅ ALREADY DONE

**Results (CONFIRMED):**
```bash
# Original bee_baby.riv contains:
python3 converter/analyze_riv.py bee_baby.riv | grep "type_48"
# Result: 2 DrawTarget objects ✅

# DrawRules (typeKey 49): NOT FOUND in bee_baby.riv
# Note: bee_baby uses DrawTarget only (simpler format)

# Packed chunks (typeKey 4992): FOUND ✅
# This is advanced draw order graph (can defer)

# Current extractor status:
jq '.artboards[0].objects | map(select(.typeKey == 48))' extracted.json
# Result: 0 objects ❌ (extractor skips them!)
```

**Conclusion:** DrawTarget extraction is MISSING in universal_extractor.cpp

---

### Phase 1: Extractor (3-4 hours)

**File:** `converter/universal_extractor.cpp`

```cpp
// Add DrawTarget import (around line 56)
#include "rive/draw_target.hpp"
#include "rive/draw_rules.hpp"

// Update getTypeName() (around line 72)
case 48: return "DrawTarget";
case 49: return "DrawRules";  // Optional, bee_baby doesn't use it

// Add DrawTarget extraction (around line 300, after Fill/Stroke)
if (auto* drawTarget = dynamic_cast<DrawTarget*>(obj)) {
    objJson["properties"]["drawableId"] = drawTarget->drawableId();        // Property 119
    objJson["properties"]["placementValue"] = drawTarget->placementValue(); // Property 120
    
    std::cout << "  [DrawTarget] localId=" << compToLocalId[comp] 
              << " drawableId=" << drawTarget->drawableId() 
              << " placement=" << drawTarget->placementValue() << std::endl;
}

// Add DrawRules extraction (OPTIONAL - bee_baby doesn't use it)
if (auto* drawRules = dynamic_cast<DrawRules*>(obj)) {
    objJson["properties"]["drawTargetId"] = drawRules->drawTargetId();  // Property 121
    
    std::cout << "  [DrawRules] localId=" << compToLocalId[comp]
              << " drawTargetId=" << drawRules->drawTargetId() << std::endl;
}
```

**Validate:**
```bash
./universal_extractor bee_baby.riv output/new.json
jq '.artboards[0].objects | map(select(.typeKey == 48 or .typeKey == 49))' output/new.json
# Expected: Multiple objects
```

---

### Phase 2: Builder (3-4 hours)

**File:** `converter/src/universal_builder.cpp`

**2.1: Object Creation (around line 129)**
```cpp
case 48: return new rive::DrawTarget();
case 49: return new rive::DrawRules();
```

**2.2: Type Map (around line 420)**
```cpp
// CORRECTED property keys from draw_target_base.hpp:
typeMap[119] = rive::CoreUintType::id;  // drawableIdPropertyKey
typeMap[120] = rive::CoreUintType::id;  // placementValuePropertyKey
typeMap[121] = rive::CoreUintType::id;  // drawTargetIdPropertyKey (DrawRules)
```

**2.3: Property Handling (around line 1114)**
```cpp
if (typeKey == 48) {  // DrawTarget
    if (key == "drawableId") {
        // CORRECTED: Property 119 (not 122!)
        deferredComponentRefs.push_back({&obj, 119, value.get<uint32_t>()});
    }
    else if (key == "placementValue") {
        // CORRECTED: Property 120 (not 123!)
        builder.set(obj, 120, value.get<uint32_t>());
    }
}

if (typeKey == 49) {  // DrawRules (OPTIONAL)
    if (key == "drawTargetId") {
        deferredComponentRefs.push_back({&obj, 121, value.get<uint32_t>()});
    }
}
```

**2.4: PASS 3 ID Remapping (around line 1250)**
```cpp
// CORRECTED: Property 119 (not 122!)
if (propertyKey == 119) {  // drawableIdPropertyKey
    auto it = localIdToBuilderObjectId.find(originalId);
    if (it != localIdToBuilderObjectId.end()) {
        builder.set(*obj, 119, it->second);
        std::cout << "  DrawTarget.drawableId remap: " << originalId 
                  << " → " << it->second << std::endl;
    } else {
        std::cerr << "  ⚠️  DrawTarget.drawableId remap FAILED: " 
                  << originalId << std::endl;
    }
}

if (propertyKey == 121) {  // drawTargetIdPropertyKey (DrawRules)
    auto it = localIdToBuilderObjectId.find(originalId);
    if (it != localIdToBuilderObjectId.end()) {
        builder.set(*obj, 121, it->second);
    }
}
```

**Validate:**
```bash
./rive_convert_cli output/new.json output/drawtarget.riv
python3 converter/analyze_riv.py output/drawtarget.riv | grep "type_48"
# Expected: 2 DrawTarget objects (matching original)

# Verify properties:
python3 converter/analyze_riv.py output/drawtarget.riv | grep -A2 "type_48"
# Expected: Properties 119 (drawableId) and 120 (placementValue) present
```

---

### Phase 3: Import Test (30 min)

**File:** `converter/import_test.cpp`

```cpp
// Add after import (around line 50)
std::cout << "m_DrawTargets.size(): " << artboard->m_DrawTargets.size() << std::endl;
std::cout << "m_FirstDrawable: " << (artboard->m_FirstDrawable ? "SET" : "NULL") << std::endl;
```

**Validate:**
```bash
./import_test output/drawtarget.riv
# Expected output:
# Artboard instance initialized.
# m_DrawTargets.size(): 2 (not 0!) ✅
# m_FirstDrawable: SET ✅
# Drawable list length: 35+ (not 0!) ✅
# SUCCESS
```

---

### Phase 4: Visual Test (15 min)

```bash
# Rive Play test
open output/drawtarget.riv

# Expected: ✅ Objects render (no grey screen)
```

---

### Success Criteria - PR-DRAWTARGET

```
✅ DrawTarget extraction works
✅ DrawRules extraction works
✅ Building works
✅ m_DrawTargets.size() > 0
✅ m_FirstDrawable != nullptr
✅ No grey screen in Rive Play
```

---

## 📋 PR-ORPHAN-FIX: Orphan Fill/Stroke Auto-Fix

### ETA: 2-3 hours | Priority: 🟡 HIGH
### Depends On: PR-DRAWTARGET complete

### Implementation

**File:** `converter/src/universal_builder.cpp`

**Location:** After PASS 1, before PASS 2 (around line 970)

```cpp
// PASS 1.5: Auto-fix orphan Fill/Stroke
std::cout << "  PASS 1.5: Fixing orphan paints..." << std::endl;

int orphanFixed = 0;
std::vector<PendingObject> newShapes;

for (auto& pending : pendingObjects) {
    if (isPaintOrDecorator(pending.typeKey) && 
        pending.parentLocalId != invalidParent)
    {
        uint16_t parentType = parentTypeFor(pending.parentLocalId);
        
        if (parentType != 0 && parentType != 3) {  // Not a Shape
            // Create synthetic Shape
            uint32_t shapeLocalId = nextSyntheticLocalId++;
            auto& shapeObj = builder.addCore(new rive::Shape());
            
            builder.set(shapeObj, 23, static_cast<uint32_t>(3));   // blendMode
            builder.set(shapeObj, 129, static_cast<uint32_t>(4));  // drawableFlags
            
            localIdToBuilderObjectId[shapeLocalId] = shapeObj.id;
            localIdToType[shapeLocalId] = 3;
            
            uint32_t originalParent = pending.parentLocalId;
            newShapes.push_back({&shapeObj, 3, shapeLocalId, originalParent});
            
            pending.parentLocalId = shapeLocalId;
            orphanFixed++;
            
            std::cerr << "  ⚠️  AUTO-FIX: Orphan paint (typeKey " << pending.typeKey 
                      << ") → synthetic Shape " << shapeLocalId << std::endl;
        }
    }
}

for (auto& newShape : newShapes) {
    pendingObjects.push_back(newShape);
}

std::cout << "  ✅ Fixed " << orphanFixed << " orphan paints" << std::endl;
```

---

### Success Criteria - PR-ORPHAN-FIX

```
✅ Orphan detection works
✅ Synthetic Shapes created
✅ Paints reparented
✅ All objects render correctly
```

---

## 📋 PR-VALIDATION: Testing

### ETA: 1-2 hours | Priority: 🟢 MANDATORY

### Test 1: Binary Analysis

```bash
python3 converter/analyze_riv.py output/complete.riv > complete.txt

# Verify DrawTarget count matches original
echo "Original:"
grep "type_48" original_analysis.txt | wc -l
echo "Round-trip:"
grep "type_48" complete.txt | wc -l
# Expected: Same count (2 for bee_baby)

# Verify DrawTarget properties
grep -A2 "type_48" complete.txt | grep "119:.*120:"
# Expected: Properties 119 and 120 present
```

---

### Test 2: Import Test

```bash
./import_test output/complete.riv
# Expected:
# m_DrawTargets.size(): X (>0)
# m_FirstDrawable: SET
# SUCCESS
```

---

### Test 3: Visual Test

```
✅ No grey screen
✅ All shapes render
✅ Colors correct
✅ Transforms correct
```

---

### Test 4: Round-Trip

```bash
# Full cycle
./universal_extractor bee_baby.riv rt1.json
./rive_convert_cli rt1.json rt1.riv
./universal_extractor rt1.riv rt2.json
./rive_convert_cli rt2.json rt2.riv

# Compare DrawTarget counts
python3 converter/analyze_riv.py rt1.riv | grep "type_48" | wc -l
python3 converter/analyze_riv.py rt2.riv | grep "type_48" | wc -l
# Should match
```

---

## 🚀 Timeline

**Day 1 (8h):** PR-DRAWTARGET Phase 1-2  
**Day 2 (6h):** PR-DRAWTARGET Phase 3-4 + PR-ORPHAN-FIX  
**Day 3 (2h):** PR-VALIDATION

---

## ⚠️ Risk Mitigation

### High Risk: DrawTarget Structure Unknown

**Mitigation:**
- Analyze original RIV thoroughly
- Study `draw_target_base.hpp`
- Check `artboard.cpp` runtime code
- Start with minimal test

---

### Medium Risk: ID Remapping Fails

**Mitigation:**
- Comprehensive PASS 3 logging
- Validate all references
- Test simple files first

---

## 🎯 Expected Outcomes

**Before (CURRENT STATE - VERIFIED):**
```bash
# Extracted JSON:
jq '.artboards[0].objects | map(select(.typeKey == 48)) | length'
# Output: 0 ❌

# Import test:
m_DrawTargets.size(): 0 ❌
m_FirstDrawable: nullptr ❌

# Rive Play:
🔲 GRİ EKRAN ❌
```

**After PR-DRAWTARGET (EXPECTED):**
```bash
# Extracted JSON:
jq '.artboards[0].objects | map(select(.typeKey == 48)) | length'
# Output: 2 ✅ (matches original)

# Import test:
m_DrawTargets.size(): 2 ✅
m_FirstDrawable: SET ✅
Drawable list: 35+ objects ✅

# Rive Play:
Objects render ⚠️ (some may lack geometry)
```

**After PR-DRAWTARGET + PR-ORPHAN-FIX (FINAL):**
```bash
m_DrawTargets.size(): 2 ✅
m_FirstDrawable: SET ✅
All Fills have Shapes: ✅
All geometry correct: ✅

Rive Play: ✅ PERFECT RENDER! ✅
```

---

## 📝 PR Checklist

### PR-DRAWTARGET
```
✅ Code committed
✅ Tests pass
✅ m_DrawTargets > 0
✅ Visual test (no grey)
✅ Screenshots in PR
```

### PR-ORPHAN-FIX
```
✅ Depends on PR-DRAWTARGET
✅ Tests pass
✅ All objects render
```

### PR-VALIDATION
```
✅ Both PRs merged
✅ Full test suite pass
✅ CI green
✅ Docs updated
```

---

**Implementation Ready:** 2 Ekim 2025  
**Total Effort:** 12-15 hours  
**Priority:** CRITICAL  
**Confidence:** 95%+
