# AGENTS.md

## 1. Project Snapshot
- **Name**: Rive Runtime JSON → RIV Converter
- **Goal**: Take a constrained JSON scene description (artboard + shapes) and emit a valid `.riv` binary that loads in the official Rive runtime/UI.
- **Primary workspace**: `converter/`
- **Build outputs**: generated in `build_converter/`

### Current Status (2025-09-29)
- ✅ CLI tools (`rive_convert_cli`, `import_test`, `analyze_riv.py`) build and run locally.
- ✅ Generated RIVs import via `import_test` and artboards instantiate.
- ✅ **Multi-chunk support added**: Serializer now writes Asset Pack (typeKey 105) and Artboard Catalog (typeKey 8776) chunks.
- ⚠️ **Testing Required**: Need to verify multi-chunk RIVs work in Rive Play. Example: `converter/test_multi_chunk.riv`.

### Reference `.riv` Structure (taken from `converter/exampleriv/nature.riv`)
- **Asset Pack (typeKey 105/106)**: contains image/meta blobs; required by Play even if empty.
- **Artboard Catalog (typeKey 8726/8776)**: registers artboards before the actual `typeKey 2` Artboard object.
- **Drawable Chain (typeKey 6)**: draw order entries referencing paints and paths.
- **Header Property Keys**: includes extended asset/draw keys (e.g., 203–208, 236, 494, …) beyond our minimal set.

Our serializer currently emits only Backboard (23), Artboard (1), Shape (3/4/7), Fill/Stroke/SolidColor (20/24/18). To satisfy Rive Play we must add placeholder asset/catalog/drawable blocks mirroring the reference structure.

## 2. Repo Layout
```
converter/
├── include/
│   ├── core_builder.hpp      # Builds runtime Core objects from parsed shapes
│   ├── json_loader.hpp       # JSON schema definitions & parser structs
│   └── serializer.hpp        # Serializes CoreDocument → .riv bytes
├── src/
│   ├── core_builder.cpp
│   ├── json_loader.cpp
│   ├── main.cpp              # CLI entry (deprecated in favour of rive_convert_cli)
│   └── serializer.cpp
├── analyze_riv.py            # Binary inspector (works outside build dir too)
├── import_test.cpp           # Simple importer exercising rive::File::import
└── exampleriv/               # Known-good .riv reference files
```
Auxiliary tools live under `build_converter/` once built:
- `build_converter/rive_convert_cli`
- `build_converter/converter/import_test`
- `build_converter/simple_demo` (currently segfaults; ignore for now)

## 3. Build & Test Workflow
_All commands are run from repo root unless noted._

```bash
# Configure once
cmake -S . -B build_converter

# Incremental rebuild of converter + importer tools
cmake --build build_converter --target rive_convert_cli import_test

# Convert JSON to RIV
cd build_converter
./converter/rive_convert_cli ../test_shapes.json test_shapes.riv

# Validate with runtime importer
./converter/import_test test_shapes.riv
# Expected output: SUCCESS ... Artboard instance initialized.

# Inspect binary structure
python3 ../converter/analyze_riv.py test_shapes.riv
```

### Quick Rebuild Shortcut
```
cmake --build build_converter
```
This currently rebuilds most of the runtime (takes a while). Optimising target selection is a future task.

## 4. JSON Input Reference
Supported shapes live in a single `shapes` array. Rectangle & ellipse both accept fill/stroke blocks. Colours accept `#RRGGBB`, `#RRGGBBAA`, `0x` prefixes, or raw hex strings.

```json
{
  "artboard": {
    "name": "ShapesDemo",
    "width": 500,
    "height": 400
  },
  "shapes": [
    {
      "type": "rectangle",
      "x": 50,
      "y": 80,
      "width": 150,
      "height": 120,
      "fill": { "color": "#FF8800" },
      "stroke": { "color": "#0000FF", "thickness": 4 }
    },
    {
      "type": "ellipse",
      "x": 250,
      "y": 150,
      "width": 120,
      "height": 200,
      "fill": { "color": "#33CC99" }
    }
  ]
}
```
Parsing occurs in `converter/src/json_loader.cpp` and builds `ShapeData` records.

## 5. Known Issues / Next Steps
| Issue | Status / Notes |
|-------|----------------|
| Rive Play crash (`swift_unexpectedError`) | Happens after drop-import even though CLI importer succeeds. Need logging (`RIVE_RUNTIME_LOG_VALIDATION=1`) to see which component fails during scene setup. |
| Parent/ID hierarchy | Serializer currently writes sequential IDs with raw JSON ids. Need to confirm mapping vs official files (backboard=1? artboard index?). |
| `simple_demo` segfault | Non-blocking; tied to deprecated JSON-to-RIV path. |

### Suggested Debug Plan
1. Generate `test_shapes.riv` and drag into Rive Play.
2. Before launch, run `export RIVE_RUNTIME_LOG_VALIDATION=1` and start the app from terminal to capture runtime output.
3. Compare failing logs with a known-good file from `converter/exampleriv/*.riv`.
4. Adjust serializer ID/parent mapping in `converter/src/serializer.cpp` to match reference structure; re-test.

## 6. Troubleshooting Checklist
- **Malformed file in `import_test`**: run `python3 converter/analyze_riv.py <file>`; ensure header keys include all properties and stream terminates with `0,0`.
- **Unknown property key X**: verify `extendedMap` in serializer covers key X and that object actually writes that key.
- **Parent/ID mismatches**: compare with reference `analyze_riv.py --json converter/exampleriv/bee_baby.riv` to understand official hierarchy.
- **Rive Play crash even after CLI success**: capture runtime logs (see section 5) and record offending type/key.

## 7. Build/Test Quick Reference
- Configure: `cmake -S . -B build_converter`
- Build tools: `cmake --build build_converter --target rive_convert_cli import_test`
- Convert: `build_converter/converter/rive_convert_cli <json> <output.riv>`
- Import smoke test: `build_converter/converter/import_test <output.riv>`
- Hex dump: `hexdump -C build_converter/<file>.riv`
- Analyse: `python3 converter/analyze_riv.py build_converter/<file>.riv`

## 8. Agent Notes
- Work in the repo root (`/Users/adilyoltay/Desktop/rive-runtime`).
- Prefer `cmake --build` over manual `make` to avoid building the entire runtime when unnecessary.
- Keep JSON fixtures (e.g., `test_shapes.json`) in repo root for now; update docs if moved.
- Document any new shape/animation support you add so the schema above stays accurate.
