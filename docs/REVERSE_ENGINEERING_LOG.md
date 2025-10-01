# 🔬 Rive Packed Format Reverse Engineering Log

**Start Date:** October 1, 2024, 21:43  
**Mission:** Decode types 8064, 7776, 64 (packed animation blobs)  
**Estimated Time:** 40 hours  
**Success Probability:** 40%  
**Risk Level:** 🔴 HIGH

---

## 📋 Session 1: Initial Analysis (Oct 1, 21:43)

### Goals
1. Analyze Type 8064 structure (keyframe blobs)
2. Analyze Type 7776 structure (interpolator blobs)
3. Analyze Type 64 structure (unknown)
4. Find patterns and commonalities

### Tools
- `analyze_riv.py` - Property extraction
- `analyze_packed_blobs.py` - Hex dump analysis
- Binary hex editor
- Pattern matching scripts

---

## 🔍 Findings: Type 8064 (Keyframe Blobs)

### Sample 1
```
Offset: 245
Hex: 807c 430e 1944 5343
Type: 8064 (varint: 0x80 0x7C)
```

**Decoder attempt:**
- `0x80 0x7C` = 8064 varint ✅
- Next bytes: `43 0e 19 44 53 43`
- Could be: properties or raw data?

### Sample 2 (from analyzer)
```
Object type_8064 (8064) -> ['14:?=15310', '8484:?=0', '4:?=5', '193:?=13']
```

**Properties detected:**
- `14` = ? (unknown Rive property)
- `8484` = ? (very high, possibly data marker)
- `4` = Component name (but wrong context!)
- `193` = ? (unknown)

**Hypothesis 1:** Properties might be misinterpreted. Could be:
- Raw binary data (not properties)
- Custom encoding scheme
- Packed array format

---

## 🔍 Findings: Type 7776 (Interpolator Blobs)

### Sample 1
```
Offset: 726
Hex: e03c 0012 05d2 0125 2173 0bff 0095 0405 d201 ed05 0000 f041 002a 0522 5c2c 0002
Type: 7776 (varint: 0xE0 0x3C)
```

**Breakdown:**
- `0xE0 0x3C` = 7776 varint ✅
- Next: `00 12` = property key 18?
- Then: `05 d2 01` = property 5, value 210?

**Pattern observation:**
- Starts with `00` frequently
- Contains property-like sequences
- Mix of small and large values

---

## 🔍 Findings: Type 64

### Pattern Discovery! 🎯

**All Type 64 start with `0x40` (decimal 64)!**

```
Offset 919:  40 52 03 63 0a c0 53...
Offset 1304: 40 0e 80 ba e9 40 00...
Offset 1892: 40 53 4c 13 18 42 00...
Offset 2428: 40 19 cd 4d 66 40 00...
```

**Hypothesis 2:** Type 64 might be:
- Animation state marker
- Keyframe container
- Timeline segment

**Properties seen:**
- Floats detected (0x3F800000 = 1.0f)
- Property keys: 67, 68, 69 (frame, value, interpolatorId!)
- Repeating patterns

---

## 📊 Cross-Reference with Known Types

### KeyFrameDouble (Type 30) Properties
```
Known from extractor:
- 67: frame time (float)
- 68: value (float)
- 69: interpolatorId (uint)
```

### Type 64 Contains These! 🎯

```
Sample from offset 4804:
40 00 1e 43 30 44 02 45 f0 01 46 2b 0c 2f

Possible decode:
- 00: marker
- 1e: 30 decimal = KeyFrameDouble typeKey!
- 43: property 67 marker?
- 30: value...
- 44: property 68 marker?
- 02: value...
- 45: property 69 marker?
- f0 01: value 240...
```

**BREAKTHROUGH HYPOTHESIS:**

Type 64 = **Packed KeyFrame Array!**
- Starts with 0x40 (type marker)
- Contains multiple KeyFrameDouble data
- Properties: 67, 68, 69 (frame, value, interpolatorId)
- Compact representation!

---

## 🧪 Next Steps (Session 2)

### Immediate Actions
1. ✅ Identify property encoding in Type 64
2. ⏳ Decode float format (IEEE 754?)
3. ⏳ Find array length encoding
4. ⏳ Verify hypothesis with multiple samples

### Test Plan
1. Create minimal JSON with 3 keyframes
2. Convert to RIV (hierarchical)
3. Manually pack into Type 64 format
4. Test import

### Research Questions
- How is array length encoded?
- Are properties always 67/68/69 or variable?
- What's the 0x43/0x44/0x45 pattern?
- How are interpolatorIds stored?

---

## 📝 Hypotheses to Test

1. **Type 64 = KeyFrame Array**
   - Status: Strong evidence
   - Next: Decode exact structure

2. **Type 8064 = Animation Metadata**
   - Status: Unknown
   - Next: Find in original vs round-trip

3. **Type 7776 = Interpolator Array**
   - Status: Likely
   - Next: Cross-reference with Type 28 data

---

## ⏱️ Time Tracking

**Session 1:** 30 minutes (analysis)  
**Total:** 0.5 / 40 hours  
**Progress:** 1.25%

---

**Status:** 🟡 In Progress  
**Next Session:** Decode Type 64 structure  
**Confidence:** 🔴 Low → 🟡 Medium (breakthrough on Type 64!)

---

## 📋 Session 2: Type 8064 Deep Dive (Oct 1, 21:46)

### Goals
- Decode Type 8064 binary structure
- Identify encoding scheme (property vs raw binary)
- Extract float patterns (keyframe data)

### Tools Created
- `deep_dive_8064.py` - Extract all 8064 blobs
- `detect_floats_in_8064.py` - Float pattern detection

### Findings

**Problem:** Varuint detection inconsistent
- Analyzer reports: 25 Type 8064 blobs
- Manual search finds: 1 Type 8064 blob
- **Root cause:** False positives in pattern matching

**Type 8064 Structure Hypothesis:**
```
Offset 245: 80 7c 43 0e 19 44 53 43 00 03 05...
            ^^^^^ ^^^^^^^^^^^^^^^^^^^^^^^^^^
            type  data (not properties!)
```

**Float Detection:**
- Found: 72.733994, -31.0, 0.0 patterns
- Range: Valid keyframe data (0-100)
- **Issue:** High noise ratio, many false positives

**Property Key Pattern:**
- Keys 66, 138, 63, 64, 65 detected
- But parse is inconsistent
- **Conclusion:** NOT standard property format

### Challenges Discovered

1. **Varuint Ambiguity**
   - 0x80 0x7C can appear in data, not just type markers
   - Need better detection (context-aware)

2. **Binary vs Property Format**
   - Type 8064 seems to be RAW BINARY
   - Not property key-value pairs
   - Need to understand binary layout

3. **Multiple Encoding Schemes**
   - Type 7776: Different from 8064
   - Type 64: Different again
   - Each needs separate decoder

### Reality Check

**Initial estimate:** 40 hours  
**Current progress:** 1 hour (2.5%)  
**Complexity:** Higher than expected

**Blockers:**
- Format is not property-based
- Multiple incompatible encodings
- High false positive rate in detection
- No documentation or reference

### Next Steps

1. ✅ Fix varuint detection (context-aware)
2. ⏳ Compare with expanded keyframes (1:1 mapping)
3. ⏳ Try binary diff approach
4. ⏳ Consider reaching out to Rive team

---

**Time Spent:** 1.0 / 40 hours (2.5%)  
**Status:** 🟡 In Progress (slower than expected)  
**Confidence:** 🔴 Low (format more complex than anticipated)
