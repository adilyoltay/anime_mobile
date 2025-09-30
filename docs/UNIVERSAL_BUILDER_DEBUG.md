# 🔬 Universal Builder - Debug Notes

**Date:** September 30, 2024  
**Status:** 🟡 NEEDS DEBUG

---

## ⚠️ PROBLEM

Universal builder creates files that:
- ✅ Import test: SUCCESS
- ❌ Rive Play: Malformed/crash/blank

---

## ✅ FIXES APPLIED

1. **Component::id() removed** - Components don't have id() accessor
2. **ParentId hierarchy mapping** - comp->parent() pointer mapping
3. **Vertex coordinate keys** - x/y use keys 24/25 for vertices (not 13/14)
4. **<map> include added** - Compilation fix

---

## 🔍 REMAINING ISSUES

**Still malformed after all fixes!**

Possible causes:
- Missing default animation?
- Object order incorrect?
- Some critical property missing?
- Binary format issue?

---

## 🧪 TEST FILES

All fail in Rive Play:
- `output/minimal_test.riv` (116 bytes) - Simple ellipse
- `output/rect_MAP_FIX.riv` (229 bytes) - Rectangle
- `output/bee_baby_FIXED.riv` (7 KB) - Complex file

All pass import_test but fail in Rive Play.

---

## 💡 COMPARISON NEEDED

Working (Hierarchical):
- `output/COMPLETE_SHOWCASE.riv` (947 bytes, import SUCCESS)
- `output/conversions/casino_PERFECT_v2.riv` (435 KB, import SUCCESS)

Failing (Universal):
- All universal builder outputs

**Next step:** Byte-by-byte comparison to find the difference.

---

## 🎯 RECOMMENDATION

Use hierarchical pipeline for production.  
Debug universal builder in next session (2-3 hours estimated).

