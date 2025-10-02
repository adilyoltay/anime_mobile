# Deep Analysis: Render Issue - Objects Not Drawing

## Executive Summary
The grey screen issue is not just about clipping - **objects are not rendering** because of incorrect parent-child relationships in the extracted JSON.

## Critical Finding #1: Orphaned Fill Object

### The Problem
```json
// FIRST OBJECT IN JSON (after Artboard)
{
  "localId": 203,
  "parentId": 0,  // ❌ ATTACHED TO ARTBOARD!
  "typeKey": 20,  // Fill
  "typeName": "Fill"
}
```

**Issue:** Fill (localId:203) is directly attached to Artboard (parentId:0) instead of a Shape.
**Impact:** Fills MUST be attached to Shapes to render. Orphaned Fills are ignored by the renderer.

### Evidence in Binary
```
Object type_20 (20) -> ['3:?=1', '5:?=0', '41:?=1']
                              ↑        ↑
                          localId  parentId=0 (Artboard)
```

## Critical Finding #2: Object Count Explosion

### Comparison
| File | Object Count | File Size |
|------|-------------|-----------|
| Original bee_baby.riv | 273 objects | 9.5KB |
| Round-trip bee_fixed.riv | 604 objects | 19KB |

### Object Distribution in Round-Trip
```
350 - KeyFrame objects (typeKey 30)
315 - CubicEaseInterpolator (typeKey 28)
93  - KeyedProperty (typeKey 26)
55  - CubicValueInterpolator (typeKey 138)
35  - Shape (typeKey 3) ✓
25  - Fill (typeKey 20) ✓
36  - SolidColor (typeKey 18) ✓
```

**Issue:** Animation data expanded from packed to hierarchical format, doubling object count.

## Critical Finding #3: Correct Hierarchy (for most objects)

### Working Example
```
Shape (199) → Artboard (0)
  ├── Fill (238) → Shape (199) ✓
  │   └── SolidColor (201) → Fill (238) ✓
  └── Rectangle (200) → Shape (199) ✓
```

This hierarchy renders correctly.

## Root Causes

### 1. Extractor Issue
The universal_extractor is creating an orphaned Fill at the beginning:
- It's extracting a Fill that should be attached to a Shape
- But it's setting parentId=0 (Artboard) instead

### 2. Missing Legacy Arrays
The extracted JSON has empty legacy arrays:
```json
{
  "shapes": [],    // ❌ Empty
  "texts": [],     // ❌ Empty  
  "paints": [],    // ❌ Empty
  "objects": [...]  // ✓ All objects here
}
```
While the builder reads from `objects[]`, the empty legacy arrays might indicate extraction issues.

### 3. Object Order Issue
Original file: First child is typeKey=420 (LayoutComponentStyle)
Round-trip: First child is typeKey=20 (orphaned Fill)

## Visual Symptoms

1. **Grey Background:** Visible because objects aren't rendering
2. **White Line:** Possibly the only object that renders (maybe the orphaned Fill?)
3. **No Shapes Visible:** Because the first Fill breaks the rendering pipeline

## Verification Tests

### Test 1: Check for more orphaned Fills
```bash
jq '.artboards[0].objects | 
    map(select(.typeKey == 20 and .parentId == 0)) | 
    length' output/rt_fixed/bee_extracted.json
```

### Test 2: Validate all Fill parents
```bash
jq '.artboards[0].objects | 
    map(select(.typeKey == 20)) | 
    map({localId, parentId, parentIsShape: 
         ((.parentId as $p) | 
          (.. | select(.localId? == $p and .typeKey == 3) | length) > 0)})' 
    output/rt_fixed/bee_extracted.json
```

## Solution

The extractor needs to be fixed to:
1. Never attach Fills directly to Artboards
2. Ensure all Fills have a Shape parent
3. Maintain correct object ordering

## Impact Analysis

- **Severity:** CRITICAL
- **User Impact:** Complete rendering failure
- **Root Component:** universal_extractor.cpp
- **Fix Location:** Parent assignment logic during extraction

## Additional Findings

### Finding #4: Orphaned Fill is from Original
The orphaned Fill (localId: 203, parentId: 0) exists in the original bee_baby.riv file:
- This is NOT a bug in our extractor
- Original file may have been authored incorrectly
- Rive runtime appears to tolerate orphaned Fills

### Finding #5: NULL Object Issue
Both round-trip and minimal test show NULL objects:
- Round-trip: Object[30] is NULL (FollowPathConstraint)
- Minimal test: Object[5] is NULL (after Auto Generated State Machine)
- This appears to be a separate issue from rendering

### Finding #6: Auto Generated State Machine
The builder adds an "Auto Generated State Machine" even when not requested:
- This adds extra objects to the file
- May contribute to object count mismatch

## Root Cause Analysis - UPDATED

The grey screen issue is likely caused by:
1. **Object count mismatch** (273 → 604) due to animation expansion
2. **Index remapping errors** during round-trip
3. **Auto-generated objects** interfering with render pipeline
4. **NOT the orphaned Fill** (this exists in original and renders fine)

## Next Steps

1. ✅ Artboard clip default fixed (already done)
2. Investigate why object count doubles during round-trip
3. Fix NULL object creation (FollowPathConstraint and others)
4. Consider disabling auto-generated state machine
5. Verify render pipeline with minimal examples
