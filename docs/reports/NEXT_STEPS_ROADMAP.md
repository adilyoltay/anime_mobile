# Next Steps Roadmap - Post PR2 Series

**Date**: October 1, 2024  
**Status**: Ready for implementation  
**Prerequisites**: PR2/PR2b/PR2c/PR2d complete (converter validated)

---

## What's Proven (PR2 Series)

✅ **Converter is correct**:
- Serialization/ToC/bitmap/typing/graph validated
- No remap misses
- No header/type mismatches
- No cycles
- Zero converter bugs found

❌ **Input JSON has quality issues**:
- Incomplete/truncated extraction
- 34+ forward references
- TrimPath with empty properties
- Root cause: Hierarchical extractor

---

## Roadmap Overview

```
PR-Extractor-Fix (root cause)
    ↓
PR-JSON-Validator (preflight)
    ↓
PR-Converter-Guard (optional, feature-flagged)
    ↓
PR-Regression-Tests (automation)
    ↓
PR-Docs (finalize)
```

---

## PR-Extractor-Fix: Fix Hierarchical Extraction

**Priority**: 🔴 **HIGH** (root cause)  
**File**: `converter/src/hierarchical_parser.cpp`  
**Estimated effort**: 4-6 hours

### Objective

Fix hierarchical extractor to emit clean, complete JSON with:
1. Complete parent references (no forward refs)
2. Required properties with defaults
3. Topologically ordered output

### Issues to Fix

#### 1. TrimPath Empty Properties

**Current behavior**:
```json
{
  "typeKey": 47,
  "localId": 189,
  "parentId": 234,
  "properties": {}  // ❌ EMPTY
}
```

**Fixed behavior**:
```json
{
  "typeKey": 47,
  "localId": 189,
  "parentId": 234,
  "properties": {
    "start": 0.0,
    "end": 0.0,
    "offset": 0.0,
    "modeValue": 0
  }
}
```

**Implementation**:
```cpp
// In hierarchical_parser.cpp, after extracting TrimPath:
if (coreType == 47) { // TrimPath
    if (!obj.properties.contains("start")) obj.properties["start"] = 0.0;
    if (!obj.properties.contains("end")) obj.properties["end"] = 0.0;
    if (!obj.properties.contains("offset")) obj.properties["offset"] = 0.0;
    if (!obj.properties.contains("modeValue")) obj.properties["modeValue"] = 0;
}
```

#### 2. Forward References

**Current behavior**:
```
Object 5 → parent 204 (doesn't exist in extraction)
Object 188 → parent 234 (doesn't exist in extraction)
```

**Fixed behavior**:
- **Option A**: Topological sort (parents before children)
- **Option B**: Drop incomplete objects with warning

**Implementation (Option A - Preferred)**:
```cpp
// After extracting all objects, reorder by dependency
std::vector<ExtractedObject> topologicalSort(std::vector<ExtractedObject>& objects) {
    std::unordered_map<uint32_t, std::vector<uint32_t>> childToParent;
    std::unordered_map<uint32_t, std::vector<uint32_t>> parentToChildren;
    
    // Build dependency graph
    for (auto& obj : objects) {
        if (obj.parentId.has_value()) {
            childToParent[obj.localId] = {*obj.parentId};
            parentToChildren[*obj.parentId].push_back(obj.localId);
        }
    }
    
    // Topological sort using DFS
    std::vector<ExtractedObject> sorted;
    std::unordered_set<uint32_t> visited;
    
    std::function<void(uint32_t)> visit = [&](uint32_t id) {
        if (visited.count(id)) return;
        visited.insert(id);
        
        // Visit parent first
        if (childToParent.count(id)) {
            for (auto parentId : childToParent[id]) {
                visit(parentId);
            }
        }
        
        // Add this object
        auto it = std::find_if(objects.begin(), objects.end(),
                               [id](auto& o) { return o.localId == id; });
        if (it != objects.end()) {
            sorted.push_back(*it);
        }
    };
    
    // Visit all objects
    for (auto& obj : objects) {
        visit(obj.localId);
    }
    
    return sorted;
}
```

#### 3. Other Effect Types

Apply same default logic to:
- **Feather** (typeKey 49): properties 749/750/751 (doubles), 752 (bool)
- **GradientStop** (typeKey 19): properties 38 (color), 39 (double)
- **Dash** (typeKey 48): properties 692 (double), 693 (bool)
- **DashPath** (typeKey 46): properties 690 (double), 691 (bool)

### Acceptance Criteria

- [ ] Re-extract bee_baby.riv → bee_baby_clean.json
- [ ] Zero forward references detected
- [ ] All TrimPath objects have properties 114-117
- [ ] All objects ordered topologically (parents before children)
- [ ] Round-trip 189/190/273 → import_test SUCCESS

### Test Plan

```bash
# Re-extract
./universal_extractor bee_baby.riv > bee_baby_clean.json

# Validate (using PR-JSON-Validator)
./json_validator bee_baby_clean.json
# Expected: forwardRefs=0, missingProps=0, cycles=false

# Convert and test
./rive_convert_cli bee_baby_clean.json bee_baby_rebuilt.riv
./import_test bee_baby_rebuilt.riv
# Expected: SUCCESS
```

---

## PR-JSON-Validator: Input Validation Layer

**Priority**: 🟡 **MEDIUM** (preflight safety)  
**File**: New `converter/json_validator.cpp`  
**Estimated effort**: 2-3 hours

### Objective

Create standalone validator to check JSON quality before conversion.

### Checks to Implement

#### 1. Parent Reference Validation
```cpp
struct ValidationResult {
    int missingParents = 0;
    std::vector<std::pair<uint32_t, uint32_t>> missingParentPairs; // (childId, parentId)
    bool hasCycles = false;
    std::map<uint16_t, std::vector<uint32_t>> missingRequiredProps; // typeKey -> [localIds]
};

ValidationResult validateJSON(const nlohmann::json& data) {
    ValidationResult result;
    
    // Build localId set
    std::unordered_set<uint32_t> localIds;
    for (auto& obj : data["artboards"][0]["objects"]) {
        localIds.insert(obj["localId"]);
    }
    
    // Check parent references
    for (auto& obj : data["artboards"][0]["objects"]) {
        if (obj.contains("parentId")) {
            uint32_t parentId = obj["parentId"];
            if (localIds.find(parentId) == localIds.end()) {
                result.missingParents++;
                result.missingParentPairs.push_back({obj["localId"], parentId});
            }
        }
    }
    
    return result;
}
```

#### 2. Cycle Detection
```cpp
bool detectCycle(const nlohmann::json& data) {
    std::unordered_map<uint32_t, uint32_t> childToParent;
    
    for (auto& obj : data["artboards"][0]["objects"]) {
        if (obj.contains("parentId")) {
            childToParent[obj["localId"]] = obj["parentId"];
        }
    }
    
    // DFS cycle detection
    for (auto& [child, parent] : childToParent) {
        std::unordered_set<uint32_t> visited;
        uint32_t cur = child;
        while (true) {
            if (visited.count(cur)) return true; // Cycle found
            visited.insert(cur);
            auto it = childToParent.find(cur);
            if (it == childToParent.end()) break;
            cur = it->second;
        }
    }
    
    return false;
}
```

#### 3. Required Properties Check
```cpp
void checkRequiredProperties(const nlohmann::json& obj, ValidationResult& result) {
    uint16_t typeKey = obj["typeKey"];
    auto props = obj.value("properties", nlohmann::json::object());
    
    // TrimPath
    if (typeKey == 47) {
        if (!props.contains("start") || !props.contains("end") || 
            !props.contains("offset") || !props.contains("modeValue")) {
            result.missingRequiredProps[47].push_back(obj["localId"]);
        }
    }
    
    // Feather
    if (typeKey == 49) {
        if (!props.contains("strength") || !props.contains("offsetX") || 
            !props.contains("offsetY") || !props.contains("inner")) {
            result.missingRequiredProps[49].push_back(obj["localId"]);
        }
    }
    
    // Add more types as needed
}
```

### CLI Interface

```cpp
int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: json_validator <input.json>" << std::endl;
        return 1;
    }
    
    auto data = loadJSON(argv[1]);
    auto result = validateJSON(data);
    
    if (result.missingParents > 0) {
        std::cerr << "❌ VALIDATION FAILED" << std::endl;
        std::cerr << "Missing parents: " << result.missingParents << std::endl;
        for (auto& [child, parent] : result.missingParentPairs) {
            std::cerr << "  Object " << child << " → parent " << parent << " (not found)" << std::endl;
        }
    }
    
    if (result.hasCycles) {
        std::cerr << "❌ Cycle detected in parent graph" << std::endl;
    }
    
    if (!result.missingRequiredProps.empty()) {
        std::cerr << "❌ Missing required properties:" << std::endl;
        for (auto& [typeKey, ids] : result.missingRequiredProps) {
            std::cerr << "  TypeKey " << typeKey << ": " << ids.size() << " objects" << std::endl;
        }
    }
    
    if (result.missingParents == 0 && !result.hasCycles && result.missingRequiredProps.empty()) {
        std::cout << "✅ VALIDATION PASSED" << std::endl;
        return 0;
    }
    
    return 1;
}
```

### Acceptance Criteria

- [ ] Detects all forward references (bee_baby_extracted.json → 34 found)
- [ ] Detects cycles (if present)
- [ ] Detects missing TrimPath properties
- [ ] Clean exit code (0 = pass, 1 = fail)
- [ ] Clear, actionable error messages

### Integration

```bash
# In round-trip script:
./json_validator input.json || {
    echo "❌ JSON validation failed, skipping conversion"
    exit 1
}

./rive_convert_cli input.json output.riv
./import_test output.riv
```

---

## PR-Converter-Guard: Optional Sanitization

**Priority**: 🟢 **LOW** (optional safety net)  
**File**: `converter/src/universal_builder.cpp`  
**Estimated effort**: 1 hour

### Objective

Keep TrimPath sanitization and forward reference guard, but make it **feature-flagged** (default OFF).

### Implementation

```cpp
// At top of file
constexpr bool SANITIZE_INPUTS = false; // Default OFF - expect clean JSON

// In PASS 1:
if (SANITIZE_INPUTS) {
    // Forward reference guard
    if (parentLocalId != invalidParent && 
        localIdToBuilderObjectId.find(parentLocalId) == localIdToBuilderObjectId.end()) {
        std::cerr << "  ⚠️  Skipping object (forward ref guard)" << std::endl;
        continue;
    }
    
    // TrimPath defaults
    if (typeKey == 47) {
        // Inject defaults if missing
    }
}
```

### Use Cases

**Default (SANITIZE_INPUTS=false)**:
- Expects clean JSON from fixed extractor
- Fast path, no overhead
- Fails fast if input is bad

**Enabled (SANITIZE_INPUTS=true)**:
- Tolerates imperfect JSON
- Useful during extractor development
- Debugging/legacy support

### Acceptance Criteria

- [ ] Default OFF - clean JSON path has zero overhead
- [ ] When enabled, provides same protection as PR2d
- [ ] Compile-time flag (no runtime cost when disabled)

---

## PR-Regression-Tests: Automated Testing

**Priority**: 🟡 **MEDIUM** (prevent regressions)  
**File**: New `tests/regression_suite.sh`  
**Estimated effort**: 2-3 hours

### Objective

Automated test harness for subset testing and validation.

### Test Cases

#### 1. Threshold Tests (bee_baby)

```bash
#!/bin/bash
# tests/regression_suite.sh

set -e

INPUT="bee_baby_clean.json"
COUNTS=(189 190 273)

for count in "${COUNTS[@]}"; do
    echo "=== Testing $count objects ==="
    
    # Create subset
    python3 -c "
import json
data = json.load(open('$INPUT'))
data['artboards'][0]['objects'] = data['artboards'][0]['objects'][:$count]
json.dump(data, open('test_$count.json', 'w'))
"
    
    # Validate
    ./json_validator test_$count.json || {
        echo "❌ Validation failed at $count objects"
        exit 1
    }
    
    # Convert
    ./rive_convert_cli test_$count.json test_$count.riv
    
    # Import test
    ./import_test test_$count.riv || {
        echo "❌ Import failed at $count objects"
        exit 1
    }
    
    echo "✅ $count objects passed"
done

echo "✅ All threshold tests passed"
```

#### 2. TrimPath Empty Properties Test

```json
// tests/trimpath_empty.json
{
  "artboards": [{
    "name": "Test",
    "width": 500,
    "height": 500,
    "objects": [
      {"typeKey": 1, "localId": 0, "properties": {}},
      {"typeKey": 3, "localId": 1, "parentId": 0, "properties": {}},
      {"typeKey": 20, "localId": 2, "parentId": 1, "properties": {}},
      {"typeKey": 47, "localId": 3, "parentId": 2, "properties": {}}
    ]
  }]
}
```

```bash
# Should fail validation
./json_validator tests/trimpath_empty.json && {
    echo "❌ Validator should have failed"
    exit 1
}

echo "✅ Validator correctly detected missing TrimPath properties"
```

#### 3. Forward Reference Test

```json
// tests/forward_ref.json
{
  "artboards": [{
    "name": "Test",
    "width": 500,
    "height": 500,
    "objects": [
      {"typeKey": 1, "localId": 0, "properties": {}},
      {"typeKey": 18, "localId": 1, "parentId": 999, "properties": {}}
    ]
  }]
}
```

```bash
# Should fail validation
./json_validator tests/forward_ref.json && {
    echo "❌ Validator should have detected forward reference"
    exit 1
}

echo "✅ Validator correctly detected forward reference"
```

### Acceptance Criteria

- [ ] All threshold tests pass with clean JSON
- [ ] Empty properties test catches TrimPath issue
- [ ] Forward reference test catches missing parent
- [ ] Automated in CI/CD pipeline
- [ ] Clear pass/fail reporting

---

## PR-Docs: Documentation Updates

**Priority**: 🟢 **LOW** (finalize)  
**Files**: `converter/src/riv_structure.md`, new `docs/INPUT_JSON_SPEC.md`  
**Estimated effort**: 1 hour

### Updates to riv_structure.md

Already done in PR2c/PR2d:
- ✅ Rectangle linkCornerRadius = 164
- ✅ TrimPath defaults (114/115/116/117)

### New: INPUT_JSON_SPEC.md

```markdown
# Input JSON Specification

## Requirements

### 1. Parent References

All `parentId` values must reference existing objects:

```json
// ❌ BAD - forward reference
{"localId": 5, "parentId": 234}  // 234 doesn't exist

// ✅ GOOD - parent exists
{"localId": 5, "parentId": 1}    // 1 exists earlier
```

**Rule**: Parents must appear before children in the objects array.

### 2. Required Properties

#### TrimPath (typeKey 47)

Must have:
- `start` (double, default 0.0)
- `end` (double, default 0.0)
- `offset` (double, default 0.0)
- `modeValue` (uint, default 0)

```json
{
  "typeKey": 47,
  "localId": 189,
  "parentId": 188,
  "properties": {
    "start": 0.0,
    "end": 0.0,
    "offset": 0.0,
    "modeValue": 0
  }
}
```

#### Feather (typeKey 49)

Must have:
- `strength` (double, default 0.0)
- `offsetX` (double, default 0.0)
- `offsetY` (double, default 0.0)
- `inner` (bool, default false)

### 3. No Cycles

Parent graph must be acyclic:

```json
// ❌ BAD - cycle
{"localId": 1, "parentId": 2}
{"localId": 2, "parentId": 1}

// ✅ GOOD - tree structure
{"localId": 1, "parentId": 0}
{"localId": 2, "parentId": 0}
```

## Validation

Use `json_validator` before conversion:

```bash
./json_validator input.json
# Output: ✅ VALIDATION PASSED

./rive_convert_cli input.json output.riv
```

## Common Issues

### Issue: MALFORMED at specific object count

**Symptom**: Works with N objects, fails with N+1  
**Cause**: Forward reference at object N+1  
**Fix**: Run validator, reorder objects topologically

### Issue: TrimPath causes freeze

**Symptom**: Import hangs or crashes  
**Cause**: Empty properties on TrimPath  
**Fix**: Add default properties (114/115/116/117)

### Issue: Missing parent warnings

**Symptom**: "WARNING: Object has missing parent"  
**Cause**: Incomplete extraction or truncated JSON  
**Fix**: Re-extract with complete object set
```

### Acceptance Criteria

- [ ] riv_structure.md is accurate and complete
- [ ] INPUT_JSON_SPEC.md clearly documents requirements
- [ ] Examples for all common issues
- [ ] Linked from main README

---

## Implementation Order

Recommended sequence:

1. **PR-JSON-Validator** (2-3 hours)
   - Creates validation tool
   - Proves current inputs are bad
   - Provides clear requirements for extractor

2. **PR-Extractor-Fix** (4-6 hours)
   - Fixes root cause
   - Uses validator to verify fixes
   - Produces clean test JSON

3. **PR-Regression-Tests** (2-3 hours)
   - Automates validation
   - Prevents future regressions
   - Uses clean JSON from fixed extractor

4. **PR-Converter-Guard** (1 hour)
   - Optional safety net
   - Low priority
   - Only if needed for legacy support

5. **PR-Docs** (1 hour)
   - Final documentation
   - Captures lessons learned
   - Provides clear guidelines

**Total estimated effort**: 10-14 hours

---

## Success Criteria

### Final Validation

After all PRs complete:

```bash
# Extract clean JSON
./universal_extractor bee_baby.riv > bee_baby_clean.json

# Validate
./json_validator bee_baby_clean.json
# Expected: ✅ VALIDATION PASSED

# Test all thresholds
for count in 189 190 273; do
    python3 create_subset.py bee_baby_clean.json $count test_$count.json
    ./json_validator test_$count.json
    ./rive_convert_cli test_$count.json test_$count.riv
    ./import_test test_$count.riv
done
# Expected: All SUCCESS
```

### Metrics

- [ ] Zero forward references in extracted JSON
- [ ] Zero missing required properties
- [ ] Zero cycles detected
- [ ] 189/190/273 all import SUCCESS
- [ ] No freeze or hang
- [ ] Regression suite passes

---

## Rollback Plan

If issues arise:

1. **Validator issues**: Use manual checks, skip automated validation
2. **Extractor issues**: Revert to manual JSON creation, fix extractor later
3. **Converter guard issues**: Disable flag, ensure clean input
4. **Test issues**: Run manually, fix automation later

All PRs are independent and can be reverted without affecting others.

---

## Contact / Escalation

If clean JSON with all requirements still fails:

1. Create minimal repro (single TrimPath + valid parent)
2. Capture import_test output and logs
3. File issue with Rive team
4. Include:
   - JSON input
   - RIV output
   - Import test behavior (hang location if debugging)
   - Converter diagnostic logs

---

**Next step**: Choose which PR to implement first (recommend PR-JSON-Validator)
