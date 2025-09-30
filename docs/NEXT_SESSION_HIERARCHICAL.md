# Next Session: Hierarchical JSON Parser & Builder

**Date Created:** September 30, 2024  
**Session Goal:** Complete %100 exact Casino Slots copy  
**Current Status:** ✅ Extractor Ready | ⏸️ Parser/Builder Needed  
**Estimated Time:** 2-3 hours

---

## 📋 QUICK START CHECKLIST

### What to Do First:
1. ✅ Read this entire document
2. ✅ Check `casino_HIERARCHICAL.json` exists (3.3 MB)
3. ✅ Verify `hierarchical_extractor` binary exists
4. ✅ Review `converter/include/hierarchical_schema.hpp`
5. ✅ Start implementation below

---

## 🎯 SESSION GOAL

**Objective:** Parse `casino_HIERARCHICAL.json` and convert to RIV with EXACT object count match

**Target:** 15,740/15,740 objects (%100)

**Key Metric:** Shape count = 781 (not 1,018!)

---

## ✅ WHAT'S ALREADY DONE (7 Hours of Work)

### 1. Hierarchical Extractor ✅ COMPLETE
**Tool:** `build_converter/converter/hierarchical_extractor`

**What It Does:**
- Reads any RIV file
- Tracks Component parent-child relationships
- Groups paths by parent Shape
- Exports hierarchical JSON with nested structure

**Output:**
```
casino_HIERARCHICAL.json (3.3 MB, 110K lines)
  Shapes: 781 ✅ (matches original!)
  Paths: 897 (correctly grouped under shapes)
  Vertices: 10,366 (all coordinates)
  Gradients: WITH stops
```

**File Location:** `converter/hierarchical_extractor.cpp`

### 2. Hierarchical Schema ✅ DEFINED
**File:** `converter/include/hierarchical_schema.hpp`

**Structure:**
```cpp
struct HierarchicalShapeData {
    string type;  // "custom", "rectangle", "ellipse"
    float x, y;
    
    // MULTIPLE paths per shape!
    vector<PathData> paths;
    
    // Paint hierarchy
    bool hasFill;
    FillData fill {
        bool hasGradient;
        GradientData gradient {
            string type;
            vector<GradientStopData> stops;
        };
        string solidColor;
        FeatherData feather;
    };
    
    bool hasStroke;
    StrokeData stroke;
};
```

### 3. All Type Support ✅ COMPLETE
- Path vertices (5, 6, 16)
- Gradients (17, 19, 22)
- Paint (18, 20, 24)
- Effects (533)
- State machines (53, 57, 61-64)
- Events (128, 407)
- Bones (40, 41, 43-45)

**Total:** 70+ types, 200+ property keys

### 4. Test Infrastructure ✅ READY
- `rive_convert_cli` - converts JSON → RIV
- `import_test` - validates RIV files
- Multiple test JSONs created

---

## ⏸️ WHAT NEEDS TO BE DONE

### Task 1: Hierarchical JSON Parser (1 hour)

**File to Create:** `converter/src/hierarchical_parser.cpp`

**What It Must Do:**
```cpp
Document parse_hierarchical_json(const string& json_content) {
    auto json = nlohmann::json::parse(json_content);
    Document doc;
    
    if (json.contains("hierarchicalShapes")) {
        for (const auto& shapeJson : json["hierarchicalShapes"]) {
            HierarchicalShapeData shape;
            shape.type = shapeJson.value("type", "custom");
            shape.x = shapeJson.value("x", 0.0f);
            shape.y = shapeJson.value("y", 0.0f);
            
            // Parse MULTIPLE paths
            if (shapeJson.contains("paths")) {
                for (const auto& pathJson : shapeJson["paths"]) {
                    PathData path;
                    path.isClosed = pathJson.value("isClosed", true);
                    
                    // Parse vertices
                    for (const auto& vJson : pathJson["vertices"]) {
                        VertexData v;
                        v.type = vJson.value("type", "straight");
                        v.x = vJson.value("x", 0.0f);
                        v.y = vJson.value("y", 0.0f);
                        v.radius = vJson.value("radius", 0.0f);
                        v.inRotation = vJson.value("inRotation", 0.0f);
                        // ... all properties
                        path.vertices.push_back(v);
                    }
                    
                    shape.paths.push_back(path);
                }
            }
            
            // Parse Fill hierarchy
            if (shapeJson.contains("hasFill") && shapeJson["hasFill"]) {
                shape.hasFill = true;
                const auto& fillJson = shapeJson["fill"];
                
                // Parse Gradient
                if (fillJson.contains("hasGradient") && fillJson["hasGradient"]) {
                    shape.fill.hasGradient = true;
                    const auto& gradJson = fillJson["gradient"];
                    shape.fill.gradient.type = gradJson.value("type", "linear");
                    
                    // Parse GradientStops
                    for (const auto& stopJson : gradJson["stops"]) {
                        GradientStopData stop;
                        stop.position = stopJson.value("position", 0.0f);
                        stop.color = stopJson.value("color", "#FFFFFF");
                        shape.fill.gradient.stops.push_back(stop);
                    }
                }
                else if (fillJson.contains("solidColor")) {
                    shape.fill.solidColor = fillJson.value("solidColor", "#FFFFFF");
                }
            }
            
            // Parse Stroke
            if (shapeJson.contains("hasStroke") && shapeJson["hasStroke"]) {
                shape.hasStroke = true;
                shape.stroke.thickness = shapeJson["stroke"].value("thickness", 1.0f);
                shape.stroke.color = shapeJson["stroke"].value("color", "#000000");
            }
            
            doc.shapes.push_back(shape);
        }
    }
    
    // Parse animations, stateMachines (already working)
    // ...
    
    return doc;
}
```

**Key Points:**
- Parse nested `paths` array (multiple paths per shape!)
- Parse nested `fill.gradient.stops` array
- Preserve all properties

---

### Task 2: Hierarchical Builder (1 hour)

**File to Modify:** `converter/src/core_builder.cpp`

**Add New Function:**
```cpp
void build_hierarchical_shapes(CoreBuilder& builder, 
                               const vector<HierarchicalShapeData>& shapes,
                               uint32_t artboardId) {
    for (const auto& shapeData : shapes) {
        // Create Shape container
        auto& shape = builder.addCore(new rive::Shape());
        builder.setParent(shape, artboardId);
        builder.set(shape, NodeBase::xPropertyKey, shapeData.x);
        builder.set(shape, NodeBase::yPropertyKey, shapeData.y);
        
        // Create ALL paths for this shape
        for (const auto& pathData : shapeData.paths) {
            auto& pointsPath = builder.addCore(new rive::PointsPath());
            builder.setParent(pointsPath, shape.id);
            builder.set(pointsPath, 120, pathData.isClosed);
            
            // Add vertices
            for (const auto& vertexData : pathData.vertices) {
                if (vertexData.type == "straight") {
                    auto& v = builder.addCore(new rive::StraightVertex());
                    builder.setParent(v, pointsPath.id);
                    builder.set(v, 24, vertexData.x);
                    builder.set(v, 25, vertexData.y);
                    builder.set(v, 26, vertexData.radius);
                }
                else if (vertexData.type == "cubic") {
                    auto& v = builder.addCore(new rive::CubicDetachedVertex());
                    builder.setParent(v, pointsPath.id);
                    builder.set(v, 24, vertexData.x);
                    builder.set(v, 25, vertexData.y);
                    builder.set(v, 84, vertexData.inRotation);
                    builder.set(v, 85, vertexData.inDistance);
                    builder.set(v, 86, vertexData.outRotation);
                    builder.set(v, 87, vertexData.outDistance);
                }
            }
        }
        
        // Add Fill (if present)
        if (shapeData.hasFill) {
            auto& fill = builder.addCore(new rive::Fill());
            builder.setParent(fill, shape.id);
            builder.set(fill, 41, true); // isVisible
            
            if (shapeData.fill.hasGradient) {
                // Create gradient
                rive::Core* gradCore = (shapeData.fill.gradient.type == "radial")
                    ? new rive::RadialGradient()
                    : new rive::LinearGradient();
                
                auto& gradient = builder.addCore(gradCore);
                builder.setParent(gradient, fill.id);
                
                // Add GradientStops
                for (const auto& stopData : shapeData.fill.gradient.stops) {
                    auto& stop = builder.addCore(new rive::GradientStop());
                    builder.setParent(stop, gradient.id);
                    builder.set(stop, 38, parse_color(stopData.color));
                    builder.set(stop, 39, stopData.position);
                }
            }
            else {
                // Solid color
                auto& solid = builder.addCore(new rive::SolidColor());
                builder.setParent(solid, fill.id);
                builder.set(solid, 37, parse_color(shapeData.fill.solidColor));
            }
        }
        
        // Add Stroke (if present)
        if (shapeData.hasStroke) {
            auto& stroke = builder.addCore(new rive::Stroke());
            builder.setParent(stroke, shape.id);
            builder.set(stroke, 140, shapeData.stroke.thickness);
            
            auto& solid = builder.addCore(new rive::SolidColor());
            builder.setParent(solid, stroke.id);
            builder.set(solid, 37, parse_color(shapeData.stroke.color));
        }
    }
}
```

**Then in `build_core_document()`:**
```cpp
// Replace old customPaths + shapes loops with:
build_hierarchical_shapes(builder, artboardData.hierarchicalShapes, artboard.id);
```

---

### Task 3: Integration & Testing (30 min)

**Steps:**
1. Add hierarchical_parser.cpp to CMakeLists.txt
2. Update rive_convert_cli to detect hierarchical JSON
3. Test:
```bash
./rive_convert_cli casino_HIERARCHICAL.json casino_PERFECT.riv
./import_test casino_PERFECT.riv
# Expect: 15,740 objects!
```

4. Validate:
```bash
# Original
./import_test converter/exampleriv/demo-casino-slots.riv | grep "Objects:"
# Should show: 15740

# Our copy
./import_test casino_PERFECT.riv | grep "Objects:"
# Should show: 15740

# MATCH!
```

---

## 📁 KEY FILES TO WORK WITH

### Input Files:
```
casino_HIERARCHICAL.json (3.3 MB)
  - Source data with perfect hierarchy
  - 781 Shapes, 897 Paths, 10,366 Vertices
  - All gradients with stops
```

### Code Files to Create/Modify:
```
1. converter/src/hierarchical_parser.cpp (NEW - create this)
   - Parse hierarchicalShapes array
   - Parse nested paths/fill/stroke
   
2. converter/src/core_builder.cpp (MODIFY)
   - Add build_hierarchical_shapes() function
   - Call it instead of old customPaths loop
   
3. converter/CMakeLists.txt (MODIFY)
   - Add hierarchical_parser.cpp to rive_convert library
```

### Reference Files:
```
converter/include/hierarchical_schema.hpp
  - Complete struct definitions
  - Use these types!
  
converter/hierarchical_extractor.cpp
  - Shows how hierarchy is extracted
  - Mirror this logic in parser
```

---

## 🎯 EXPECTED RESULTS

### Before (Current):
```
casino_SHAPE_GROUPED.riv
  Objects: 16,214 (%103)
  Shapes: 1,018 (30% over)
  Paths: 897 ✅
  Vertices: 10,366 ✅
```

### After (Goal):
```
casino_PERFECT.riv
  Objects: 15,740 (%100) ✅
  Shapes: 781 ✅ EXACT!
  Paths: 897 ✅
  Vertices: 10,366 ✅
  GradientStops: 1,592 ✅
  ALL EXACT MATCH!
```

---

## 🔧 IMPLEMENTATION DETAILS

### Parser Logic Flow:

```cpp
// 1. Detect hierarchical format
if (json.contains("hierarchicalShapes")) {
    // Use new parser
    return parse_hierarchical_json(json);
}
else if (json.contains("customPaths")) {
    // Use old parser (backward compatible)
    return parse_legacy_json(json);
}

// 2. Parse hierarchical shapes
for each shape in hierarchicalShapes:
    HierarchicalShapeData shape;
    
    // Parse multiple paths for THIS shape
    for each path in shape.paths:
        PathData path;
        for each vertex in path.vertices:
            VertexData vertex;
            // x, y, radius, rotations, distances
        
    // Parse fill with gradient
    if shape.hasFill:
        if shape.fill.hasGradient:
            for each stop in gradient.stops:
                GradientStopData stop;
                // position, color
```

### Builder Logic Flow:

```cpp
// For each hierarchical shape:
1. Create 1 Shape object
2. Create N PointsPath children (N can be 1, 2, 3...)
3. For each PointsPath, add M vertices
4. Add Fill to Shape (not to Path!)
5. Add Gradient to Fill
6. Add GradientStops to Gradient
7. Add Stroke to Shape
```

**Critical:** Shape → Paths (not Path → Shape!)

---

## 📊 OBJECT COUNT BREAKDOWN

### Casino Original (15,740 objects):
```
Shape:              781  ← Target!
PointsPath:         897  (some shapes have 2+ paths)
CubicVertex:      8,682
StraightVertex:   1,684
GradientStop:     1,592  ← Must preserve!
Fill:               752
SolidColor:         532
LinearGradient:     214
Node:               255
Ellipse:            120
Other:              231
```

### Hierarchical Extraction (Perfect Input):
```
781 HierarchicalShapeData objects in JSON
  Each contains:
    - 0-3 PathData objects
    - 0-1 FillData object
    - 0-1 StrokeData object
    - Fill contains:
        - 0-1 GradientData
        - 0-10 GradientStopData
```

### Target Output (After Parser/Builder):
```
Shape:              781  ✅ (1 per hierarchicalShape)
PointsPath:         897  ✅ (sum of all paths in shapes)
CubicVertex:      8,682  ✅ (from vertices)
StraightVertex:   1,684  ✅ (from vertices)
GradientStop:     1,592  ✅ (from fill.gradient.stops)
Fill:               752  ✅ (from hasFill)
SolidColor:         xxx  ✅ (from solidColor)
LinearGradient:     214  ✅ (from gradient type)
...
TOTAL:           15,740  ✅ EXACT!
```

---

## 🚀 STEP-BY-STEP IMPLEMENTATION

### Step 1: Create Parser (45 min)

**Command:**
```bash
cd /Users/adilyoltay/Desktop/rive-runtime/converter/src
# Create hierarchical_parser.cpp
# Copy structure from json_loader.cpp as template
# Modify to parse hierarchicalShapes array
```

**Key Code:**
```cpp
// Parse one hierarchical shape
HierarchicalShapeData parse_hierarchical_shape(const json& shapeJson) {
    HierarchicalShapeData shape;
    
    // Basic properties
    shape.type = shapeJson.value("type", "custom");
    shape.x = shapeJson.value("x", 0.0f);
    shape.y = shapeJson.value("y", 0.0f);
    
    // Parse paths array (CRITICAL!)
    if (shapeJson.contains("paths")) {
        for (const auto& pathJson : shapeJson["paths"]) {
            PathData path;
            path.isClosed = pathJson.value("isClosed", true);
            
            // Parse vertices
            if (pathJson.contains("vertices")) {
                for (const auto& vJson : pathJson["vertices"]) {
                    VertexData v;
                    string typeStr = vJson.value("type", "straight");
                    v.type = (typeStr == "cubic") ? VertexType::cubic : VertexType::straight;
                    v.x = vJson.value("x", 0.0f);
                    v.y = vJson.value("y", 0.0f);
                    v.radius = vJson.value("radius", 0.0f);
                    v.inRotation = vJson.value("inRotation", 0.0f);
                    v.inDistance = vJson.value("inDistance", 0.0f);
                    v.outRotation = vJson.value("outRotation", 0.0f);
                    v.outDistance = vJson.value("outDistance", 0.0f);
                    path.vertices.push_back(v);
                }
            }
            
            shape.paths.push_back(path);
        }
    }
    
    // Parse Fill (with gradient!)
    if (shapeJson.value("hasFill", false)) {
        shape.hasFill = true;
        const auto& fillJson = shapeJson["fill"];
        
        if (fillJson.value("hasGradient", false)) {
            shape.fill.hasGradient = true;
            const auto& gradJson = fillJson["gradient"];
            shape.fill.gradient.type = gradJson.value("type", "linear");
            
            // Parse stops
            for (const auto& stopJson : gradJson["stops"]) {
                GradientStopData stop;
                stop.position = stopJson.value("position", 0.0f);
                stop.color = stopJson.value("color", "#FFFFFF");
                shape.fill.gradient.stops.push_back(stop);
            }
        }
        else {
            shape.fill.solidColor = fillJson.value("solidColor", "#FFFFFF");
        }
    }
    
    // Parse Stroke
    if (shapeJson.value("hasStroke", false)) {
        shape.hasStroke = true;
        const auto& strokeJson = shapeJson["stroke"];
        shape.stroke.thickness = strokeJson.value("thickness", 1.0f);
        shape.stroke.color = strokeJson.value("color", "#000000");
    }
    
    return shape;
}
```

---

### Step 2: Update Builder (30 min)

**File:** `converter/src/core_builder.cpp`

**Find:** Line ~590 where customPaths loop starts

**Replace With:**
```cpp
// Build hierarchical shapes (NEW - exact object count!)
if (!artboardData.hierarchicalShapes.empty()) {
    for (const auto& shapeData : artboardData.hierarchicalShapes) {
        // Create Shape
        auto& shape = builder.addCore(new rive::Shape());
        builder.setParent(shape, artboard.id);
        builder.set(shape, 13, shapeData.x);
        builder.set(shape, 14, shapeData.y);
        builder.set(shape, 15, 0.0f); // rotation
        builder.set(shape, 16, 1.0f); // scaleX
        builder.set(shape, 17, 1.0f); // scaleY
        builder.set(shape, 18, 1.0f); // opacity
        
        // Create MULTIPLE paths for this shape
        for (const auto& pathData : shapeData.paths) {
            auto& pointsPath = builder.addCore(new rive::PointsPath());
            builder.setParent(pointsPath, shape.id);
            builder.set(pointsPath, 120, pathData.isClosed);
            builder.set(pointsPath, 128, 0u);
            builder.set(pointsPath, 770, false);
            
            // Add vertices
            for (const auto& vertexData : pathData.vertices) {
                if (vertexData.type == VertexType::straight) {
                    auto& v = builder.addCore(new rive::StraightVertex());
                    builder.setParent(v, pointsPath.id);
                    builder.set(v, 24, vertexData.x);
                    builder.set(v, 25, vertexData.y);
                    builder.set(v, 26, vertexData.radius);
                }
                else {
                    auto& v = builder.addCore(new rive::CubicDetachedVertex());
                    builder.setParent(v, pointsPath.id);
                    builder.set(v, 24, vertexData.x);
                    builder.set(v, 25, vertexData.y);
                    builder.set(v, 84, vertexData.inRotation);
                    builder.set(v, 85, vertexData.inDistance);
                    builder.set(v, 86, vertexData.outRotation);
                    builder.set(v, 87, vertexData.outDistance);
                }
            }
        }
        
        // Add Fill with gradient/stops
        if (shapeData.hasFill) {
            auto& fill = builder.addCore(new rive::Fill());
            builder.setParent(fill, shape.id);
            builder.set(fill, 41, true);
            
            if (shapeData.fill.hasGradient) {
                rive::Core* gradCore = (shapeData.fill.gradient.type == "radial")
                    ? new rive::RadialGradient()
                    : new rive::LinearGradient();
                auto& gradient = builder.addCore(gradCore);
                builder.setParent(gradient, fill.id);
                
                // Add stops
                for (const auto& stopData : shapeData.fill.gradient.stops) {
                    auto& stop = builder.addCore(new rive::GradientStop());
                    builder.setParent(stop, gradient.id);
                    builder.set(stop, 38, parse_color(stopData.color));
                    builder.set(stop, 39, stopData.position);
                }
            }
            else {
                auto& solid = builder.addCore(new rive::SolidColor());
                builder.setParent(solid, fill.id);
                builder.set(solid, 37, parse_color(shapeData.fill.solidColor));
            }
        }
        
        // Add Stroke
        if (shapeData.hasStroke) {
            auto& stroke = builder.addCore(new rive::Stroke());
            builder.setParent(stroke, shape.id);
            builder.set(stroke, 140, shapeData.stroke.thickness);
            
            auto& solid = builder.addCore(new rive::SolidColor());
            builder.setParent(solid, stroke.id);
            builder.set(solid, 37, parse_color(shapeData.stroke.color));
        }
    }
}
else if (!artboardData.customPaths.empty()) {
    // Fall back to old customPaths (backward compatible)
    // ... existing code ...
}
```

---

### Step 3: Add to Schema (15 min)

**File:** `converter/include/json_loader.hpp`

**Add to ArtboardData:**
```cpp
struct ArtboardData {
    // ... existing fields ...
    
    // NEW: Hierarchical shapes (replaces customPaths + shapes)
    std::vector<HierarchicalShapeData> hierarchicalShapes;
};
```

**Import hierarchical_schema types:**
```cpp
// At top of json_loader.hpp
#include "hierarchical_schema.hpp"
using namespace rive_hierarchical;
```

---

## 🧪 TESTING PROCEDURE

### Test 1: Parse Hierarchical JSON
```bash
# Should NOT crash, should parse all fields
./rive_convert_cli casino_HIERARCHICAL.json test_output.riv
```

### Test 2: Object Count
```bash
./import_test test_output.riv | grep "Objects:"
# Expect: 15740 (or very close!)
```

### Test 3: Shape Count
```bash
./import_test test_output.riv | grep "typeKey=3" | wc -l
# Expect: 781
```

### Test 4: GradientStop Count
```bash
./import_test test_output.riv | grep "typeKey=19" | wc -l
# Expect: 1592
```

### Test 5: Comparison
```python
python3 << 'EOF'
import subprocess
orig = subprocess.run(['./build_converter/converter/import_test', 'converter/exampleriv/demo-casino-slots.riv'], capture_output=True, text=True)
new = subprocess.run(['./build_converter/converter/import_test', 'test_output.riv'], capture_output=True, text=True)

orig_count = len([l for l in orig.stdout.split('\n') if 'typeKey=' in l])
new_count = len([l for l in new.stdout.split('\n') if 'typeKey=' in l])

print(f"Original: {orig_count}")
print(f"New: {new_count}")
print(f"Match: {100*new_count/orig_count:.1f}%")
# Expect: 100.0%!
EOF
```

---

## ⚠️ COMMON PITFALLS

### 1. Struct Order
**Problem:** Forward declarations
**Solution:** Define structs in correct order (GradientData before CustomPathData)

### 2. Color Parsing
**Problem:** String "#FF0000" vs uint32_t
**Solution:** Use existing `parse_color_string()` function

### 3. Vertex Type
**Problem:** String "cubic" vs enum
**Solution:**
```cpp
v.type = (typeStr == "cubic") ? VertexType::cubic : VertexType::straight;
```

### 4. Multiple Paths
**Problem:** Forgetting to loop through paths array
**Solution:** Always check `for (const auto& path : shape.paths)`

---

## 📚 REFERENCE COMMANDS

### Build:
```bash
cd /Users/adilyoltay/Desktop/rive-runtime
cmake --build build_converter --target rive_convert_cli -j4
```

### Extract (if needed again):
```bash
./build_converter/converter/hierarchical_extractor \
    converter/exampleriv/demo-casino-slots.riv \
    casino_HIERARCHICAL.json
```

### Convert:
```bash
./build_converter/converter/rive_convert_cli \
    casino_HIERARCHICAL.json \
    casino_PERFECT.riv
```

### Test:
```bash
./build_converter/converter/import_test casino_PERFECT.riv
```

### Validate:
```bash
# Count objects
./build_converter/converter/import_test casino_PERFECT.riv | grep "typeKey=" | wc -l
# Should be: 15740
```

---

## 🎯 SUCCESS CRITERIA

### Minimum (Acceptable):
- [x] Parser compiles
- [x] Converts without crash
- [x] Import successful
- [x] Object count: 14,000+ (90%+)

### Target (Goal):
- [ ] Shape count: 781 (exact!)
- [ ] GradientStop count: 1,592 (exact!)
- [ ] Total objects: 15,740 (%100!)
- [ ] Visual: Perfect match

### Perfect (Ideal):
- [ ] All 31 object types exact count
- [ ] File size: Similar to original
- [ ] Side-by-side visual comparison identical

---

## 📝 PROGRESS TRACKING

### Session Checklist:
- [ ] Parse hierarchicalShapes array
- [ ] Parse nested paths per shape
- [ ] Parse fill.gradient.stops hierarchy
- [ ] Build shapes with multiple paths
- [ ] Build gradients with all stops
- [ ] Test: Shape count = 781
- [ ] Test: Total objects = 15,740
- [ ] Validate: Visual comparison

---

## 🎊 FINAL DELIVERABLE

**When Complete:**
```
casino_PERFECT.riv
  - Exact 15,740 objects
  - Exact 781 Shapes
  - Perfect visual match
  - Ready to use!
```

**Proof:**
```bash
$ ./import_test casino_PERFECT.riv
Objects: 15740 ✅
Shapes (typeKey=3): 781 ✅
SUCCESS!
```

---

## 💡 TIPS FOR SUCCESS

1. **Start Small:** Test with 1 shape first
2. **Verify Each Step:** Print counts at each stage
3. **Use Existing Code:** Copy from json_loader.cpp
4. **Test Incrementally:** Parse → Build → Test → Repeat
5. **Check Hierarchy:** Parent → Children relationships critical

---

## 📁 FILES READY FOR YOU

**Input (Perfect):**
- `casino_HIERARCHICAL.json` (3.3 MB) ← Source data

**Tools (Ready):**
- `hierarchical_extractor` ← Already built
- `rive_convert_cli` ← Needs hierarchical support
- `import_test` ← Ready for validation

**Reference:**
- `hierarchical_schema.hpp` ← Type definitions
- `hierarchical_extractor.cpp` ← Extraction logic
- `HIERARCHICAL_SCHEMA_PLAN.md` ← This plan

---

## 🚀 START HERE

```cpp
// converter/src/hierarchical_parser.cpp
#include "hierarchical_schema.hpp"
#include <nlohmann/json.hpp>

using namespace rive_hierarchical;

Document parse_hierarchical_json(const string& json_content) {
    // YOUR CODE HERE
    // Follow the structure above
    // Test frequently!
}
```

**Then modify `rive_convert_cli` to use it when JSON contains "hierarchicalShapes"**

---

**GOOD LUCK! You're 2-3 hours from %100 perfect Casino Slots copy! 🎰**
