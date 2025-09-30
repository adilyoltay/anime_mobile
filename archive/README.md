# Archive - Old Project Files

**Created:** September 30, 2024  
**Purpose:** Archive of old experimental files and builds during Rive converter development

---

## 📦 Contents

### old_builds/ (~516 MB)
Old CMake build directories from early converter experiments:
- `build_real/` - Early "real" converter attempt
- `build_working/` - Early "working" converter attempt  
- `build/` - Initial build directory
- Build scripts: `build_converter.bat`, `build_converter.sh`

**Note:** Active build is `../build_converter/` (not archived)

### old_converters/ (9 files)
Legacy converter source files before hierarchical implementation:
- `json_to_riv_converter.*` - First converter attempt
- `real_json_to_riv_converter.*` - Second attempt
- `improved_json_converter.cpp` - Third iteration
- `simple_json_converter.cpp` - Minimal version
- `working_riv_converter.cpp` - Working prototype
- `test_converter.cpp` - Test harness
- `simple_demo.cpp` - Demo file

**Note:** Current production converter is in `../converter/src/`

### old_cmake/ (5 files)
CMakeLists.txt variants from different converter iterations:
- `CMakeLists_heart.txt`
- `CMakeLists_improved.txt`
- `CMakeLists_real.txt`
- `CMakeLists_simple.txt`
- `CMakeLists_working.txt`

**Note:** Active CMakeLists.txt is in project root

### test_jsons/ (9 files)
Test JSON files used during development:
- `ALL_CASINO_TYPES_TEST.json` - Type coverage test
- `advanced_effects.json` - Gradient/feather tests
- `all_shapes.json` - Shape type tests
- `artboard_only.json` - Minimal artboard test
- `bee_detailed.json` - Complex path test
- `bouncing_ball.json` - Animation test
- `breathe.json` - Transform test
- `complete_demo.json` - Feature showcase
- `dash_test.json` - Dash path test

**Note:** Active test file is `../casino_HIERARCHICAL.json`

### test_rivs/ (3 files)
Test RIV output files:
- `artboard_only.riv` - Minimal test
- `casino_PERFECT.riv` - v1 output (superseded)
- `test_multi_chunk.riv` - Multi-chunk test

**Note:** Current output is `../casino_PERFECT_v2.riv`

### scripts/ (6 files)
Python analysis and generation scripts:
- `analyze_casino_detailed.py` - Detailed RIV analysis
- `compare_exact_copy.py` - Binary comparison
- `extract_casino_structure.py` - Structure extraction
- `generate_casino_exact_copy.py` - Copy generator
- `riv_to_json_complete.py` - Complete RIV→JSON export
- `validate_exact_match.py` - Validation tool

**Note:** Active tools are in `../converter/` (analyze_riv.py, hierarchical_extractor)

### docs/ (2 files)
Temporary planning documents:
- `CLEANUP_PLAN.md` - This cleanup plan
- `FIXES_NEEDED.md` - Fix list (all completed)

---

## 🗑️ Deletion Guidelines

**Can be deleted after 1-2 weeks if not needed:**
- `old_builds/` - Takes 516 MB, can rebuild if needed
- `old_converters/` - Superseded by production converter
- `old_cmake/` - Superseded by main CMakeLists.txt
- `docs/` - Temporary planning docs

**Keep for reference:**
- `test_jsons/` - Useful for regression testing
- `test_rivs/` - Reference outputs
- `scripts/` - Analysis tools might be useful

---

## ⚠️ Recovery

If you need any archived file:
```bash
# Copy back from archive
cp archive/test_jsons/bouncing_ball.json .
cp -r archive/old_builds/build_working .
```

To delete entire archive:
```bash
# WARNING: Permanent deletion!
rm -rf archive/
```

---

**Archive created during hierarchical parser implementation - Sep 30, 2024**
