# Critical Fixes Needed for Exact Copy

## 1. ❌ Text Rendering (Still Broken in Text Files)

**Problem 1a: TextValueRun parent wrong**
```cpp
// converter/src/core_builder.cpp:507-509
builder.setParent(textRun, textStylePaint.id); // ❌ WRONG!
```
**Fix:**
```cpp
builder.setParent(textRun, text.id); // ✅ Must be under Text!
```

**Problem 1b: TextValueRun::text wrong key**
```cpp
// converter/src/core_builder.cpp:509
builder.set(textRun, static_cast<uint16_t>(271), textData.content); // ❌ WRONG!
```
**Fix:**
```cpp
builder.set(textRun, static_cast<uint16_t>(268), textData.content); // ✅ Key 268!
```

**Problem 1c: Font bytes in wrong place**
```cpp
// converter/src/serializer.cpp:287-305
// Writing font to ImageAsset placeholder ❌ WRONG!
```
**Fix:**
```cpp
// Must write FileAssetContents immediately after FontAsset
FontAsset (141)
  └─ FileAssetContents (106) with bytes (212)
```

---

## 2. ❌ Reference Remapping Incomplete

**Missing remaps in serializer:**
```cpp
case 51:  // KeyedObject::objectId - animation references
case 92:  // ClippingShape::sourceId - clipping references
case 272: // TextValueRun::styleId - text style references
```

These must use `localComponentIndex` mapping!

---

## 3. ❌ Asset Streaming Order

**Current:** Font bytes after Backboard in ImageAsset placeholder
**Correct:** FileAssetContents must follow its FontAsset

```
Backboard
FontAsset (141, assetId=0)
FileAssetContents (106, bytes=<TTF data>)  // ← Immediately after!
Artboard
  Text
    TextStylePaint (fontAssetId=0)  // ← References asset index 0
```

---

## 4. ✅ Property Filter (Partially Done)

We're already skipping many defaults, but should be more strict:
- Don't write pathFlags=0
- Don't write isHole=false
- Don't write identity transforms unless original has them

---

## Priority Fix Order

1. **Text rendering** (fixes 1a, 1b, 1c) - Makes text visible
2. **Reference remapping** (fix 2) - Makes animations/clipping work
3. **Asset ordering** (fix 3) - Proper font binding
4. **Property filter** (fix 4) - Size optimization

