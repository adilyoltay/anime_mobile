# RIV File Structure Analysis

## Working RIV Structure (from nature.riv)

```
HEADER:
  - Magic: "RIVE"
  - Version: 7.0
  - FileID: unique identifier
  - Property Table: list of property keys used
  - Bitmap: 2-bit field types for each property

CHUNK 1: Asset Pack (Optional but expected by Rive Play)
  - TypeKey: 105 (FileAssetContents)
  - Asset data length: N bytes
  - Raw asset bytes (images, fonts, etc.)
  - Terminator: 0x00

CHUNK 2: Main Scene Graph
  - TypeKey: 23 (Backboard)
  - TypeKey: 1 (Artboard)
  - Shapes, Fills, Animations, etc.
  - Terminator: 0x00

CHUNK 3: Artboard Catalog (Optional but expected by Rive Play)
  - TypeKey: 8776 (ArtboardListItem?)
  - References artboard IDs
  - Provides UI metadata
  - Terminator: 0x00

END: Multiple 0x00 terminators
```

## Our Current Structure (Incomplete)

```
HEADER: ✅
MAIN SCENE: ✅ (but missing catalog reference)
ASSET PACK: ❌ Missing
ARTBOARD CATALOG: ❌ Missing
```

## Solution: Add Minimal Chunks

1. **Empty Asset Pack**: Write typeKey 105 with 0-length data
2. **Simple Catalog**: Write typeKey 8776 referencing our artboard
3. **Proper Terminators**: Add chunk boundaries with 0x00

This should satisfy Rive Play's file format expectations.
