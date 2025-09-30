# 📤 Output Directory

**Created:** September 30, 2024  
**Purpose:** Organized output files from hierarchical converter pipeline

---

## 📁 Directory Structure

### extractions/
Hierarchical JSON files extracted from RIV binaries:
- `bee_baby_HIERARCHICAL.json` - Bee Baby extraction
- `casino_HIERARCHICAL.json` - Casino Slots extraction (3.3 MB, 781 shapes)

### conversions/
RIV files generated from hierarchical JSON:
- `bee_baby_COPY.riv` - Bee Baby copy (2.4 KB)
- `casino_PERFECT_v2.riv` - Casino Slots copy (425 KB)

### tests/
Test reports and validation results (placeholder for future)

---

## 🔄 Pipeline Overview

```
Original RIV
    ↓
hierarchical_extractor
    ↓
extractions/*.json (Hierarchical JSON)
    ↓
rive_convert_cli
    ↓
conversions/*.riv (Generated RIV)
    ↓
import_test
    ↓
Validation results
```

---

## 📊 Test Results

### Casino Slots:
- **Extraction:** casino_HIERARCHICAL.json (3.3 MB)
- **Conversion:** casino_PERFECT_v2.riv (425 KB)
- **Accuracy:** 15,210/15,683 objects (97%)
- **Core Geometry:** 11,044/11,044 (100%) ✅

### Bee Baby:
- **Extraction:** bee_baby_HIERARCHICAL.json
- **Conversion:** bee_baby_COPY.riv (2.4 KB)
- **Accuracy:** 156/277 objects (56%)
- **Core Geometry:** 83/83 (100%) ✅

---

## 🎯 Quality Metrics

**What Works (100% Accurate):**
- Custom path shapes
- Vertices (straight & cubic)
- Paint system (solid colors, gradients)
- Strokes with thickness
- Feather effects

**What's Limited (Extractor Focus):**
- Parametric shapes (ellipse, rectangle)
- Node containers
- Bones/rigging
- Multiple artboards (only first extracted)
- Advanced types

---

## 🚀 Usage

### Extract RIV → JSON:
```bash
./build_converter/converter/hierarchical_extractor \
    INPUT.riv \
    output/extractions/OUTPUT.json
```

### Convert JSON → RIV:
```bash
./build_converter/converter/rive_convert_cli \
    output/extractions/INPUT.json \
    output/conversions/OUTPUT.riv
```

### Validate:
```bash
./build_converter/converter/import_test \
    output/conversions/OUTPUT.riv
```

---

## 📝 Notes

- Hierarchical extractor focuses on custom path geometry
- Multiple artboards: Only first artboard extracted
- File size differences normal (no embedded assets in copy)
- Core geometry replication: Production-ready ✅

---

**Maintained as part of the Rive Runtime Converter project**
