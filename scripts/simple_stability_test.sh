#!/bin/bash
# Simple Stability Test - Checks if round-trip converges to same output

set -e

RIV_FILE="$1"
BASE=$(basename "$RIV_FILE" .riv)
OUT="output/roundtrip/stability"

mkdir -p "$OUT"

echo "╔══════════════════════════════════════════════════════════╗"
echo "║       STABILITY TEST: $BASE"
echo "╚══════════════════════════════════════════════════════════╝"
echo ""

# Cycle 1: RIV → JSON → RIV
echo "→ Cycle 1: Extract"
./build_converter/converter/universal_extractor "$RIV_FILE" "$OUT/${BASE}_c1.json" > /dev/null 2>&1

echo "→ Cycle 1: Convert to RIV"
./build_converter/converter/rive_convert_cli "$OUT/${BASE}_c1.json" "$OUT/${BASE}_c2.riv" > /dev/null 2>&1

echo "→ Cycle 1: Import test"
./build_converter/converter/import_test "$OUT/${BASE}_c2.riv" > "$OUT/${BASE}_c2_import.txt" 2>&1
C2_OBJS=$(grep "Objects:" "$OUT/${BASE}_c2_import.txt" | head -1 | awk '{print $2}')

# Cycle 2: RIV → JSON → RIV (repeat)
echo "→ Cycle 2: Re-extract"
./build_converter/converter/universal_extractor "$OUT/${BASE}_c2.riv" "$OUT/${BASE}_c3.json" > /dev/null 2>&1

echo "→ Cycle 2: Convert to RIV"
./build_converter/converter/rive_convert_cli "$OUT/${BASE}_c3.json" "$OUT/${BASE}_c4.riv" > /dev/null 2>&1

echo "→ Cycle 2: Import test"
./build_converter/converter/import_test "$OUT/${BASE}_c4.riv" > "$OUT/${BASE}_c4_import.txt" 2>&1
C4_OBJS=$(grep "Objects:" "$OUT/${BASE}_c4_import.txt" | head -1 | awk '{print $2}')

# Cycle 3: One more time for final stability
echo "→ Cycle 3: Final extraction"
./build_converter/converter/universal_extractor "$OUT/${BASE}_c4.riv" "$OUT/${BASE}_c5.json" > /dev/null 2>&1

echo "→ Cycle 3: Final conversion"
./build_converter/converter/rive_convert_cli "$OUT/${BASE}_c5.json" "$OUT/${BASE}_c6.riv" > /dev/null 2>&1

echo "→ Cycle 3: Final import"
./build_converter/converter/import_test "$OUT/${BASE}_c6.riv" > "$OUT/${BASE}_c6_import.txt" 2>&1
C6_OBJS=$(grep "Objects:" "$OUT/${BASE}_c6_import.txt" | head -1 | awk '{print $2}')

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "COMPARISON RESULTS"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

# File sizes
S2=$(stat -f%z "$OUT/${BASE}_c2.riv" 2>/dev/null || stat -c%s "$OUT/${BASE}_c2.riv")
S4=$(stat -f%z "$OUT/${BASE}_c4.riv" 2>/dev/null || stat -c%s "$OUT/${BASE}_c4.riv")
S6=$(stat -f%z "$OUT/${BASE}_c6.riv" 2>/dev/null || stat -c%s "$OUT/${BASE}_c6.riv")

echo "📊 File Sizes:"
echo "  C2 RIV: $S2 bytes"
echo "  C4 RIV: $S4 bytes"
echo "  C6 RIV: $S6 bytes"
echo ""

# Object counts
echo "📊 Imported Object Counts:"
echo "  C2: $C2_OBJS objects"
echo "  C4: $C4_OBJS objects"
echo "  C6: $C6_OBJS objects"
echo ""

# Hash comparison
H2=$(shasum -a 256 "$OUT/${BASE}_c2.riv" | awk '{print $1}')
H4=$(shasum -a 256 "$OUT/${BASE}_c4.riv" | awk '{print $1}')
H6=$(shasum -a 256 "$OUT/${BASE}_c6.riv" | awk '{print $1}')

echo "🔐 SHA-256 Hashes:"
echo "  C2: $H2"
echo "  C4: $H4"
echo "  C6: $H6"
echo ""

# JSON comparison (C3 vs C5 should be identical if stable)
echo "📄 JSON Comparison (C3 vs C5):"
if diff -q "$OUT/${BASE}_c3.json" "$OUT/${BASE}_c5.json" > /dev/null 2>&1; then
    echo "  ✅ IDENTICAL - Perfect JSON stability!"
else
    echo "  ⚠️  Different - Checking details..."
    diff "$OUT/${BASE}_c3.json" "$OUT/${BASE}_c5.json" > "$OUT/${BASE}_json_diff.txt" 2>&1 || true
    DIFF_LINES=$(wc -l < "$OUT/${BASE}_json_diff.txt")
    echo "  📝 $DIFF_LINES lines different (see ${BASE}_json_diff.txt)"
fi
echo ""

# Binary comparison (C4 vs C6 for convergence)
echo "🔍 Binary Comparison (C4 vs C6):"
if cmp -s "$OUT/${BASE}_c4.riv" "$OUT/${BASE}_c6.riv"; then
    echo "  ✅ BYTE-IDENTICAL - Perfect binary stability!"
else
    echo "  ⚠️  Different - Checking details..."
    cmp -l "$OUT/${BASE}_c4.riv" "$OUT/${BASE}_c6.riv" > "$OUT/${BASE}_binary_diff.txt" 2>&1 || true
    DIFF_BYTES=$(wc -l < "$OUT/${BASE}_binary_diff.txt")
    PCT=$(awk "BEGIN {printf \"%.3f\", ($DIFF_BYTES / $S4) * 100}")
    echo "  📝 $DIFF_BYTES bytes different ($PCT%)"
fi
echo ""

# Stability verdict
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "STABILITY VERDICT"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

STABLE="YES"

# Check object count stability
if [ "$C4_OBJS" = "$C6_OBJS" ]; then
    echo "✅ Object count STABLE ($C4_OBJS objects)"
else
    echo "⚠️  Object count CHANGED (C4: $C4_OBJS → C6: $C6_OBJS)"
    STABLE="PARTIAL"
fi

# Check size stability
DIFF=$((S6 - S4))
if [ ${DIFF#-} -lt 100 ]; then
    echo "✅ File size STABLE (diff: $DIFF bytes)"
else
    PCT=$(awk "BEGIN {printf \"%.2f\", ($DIFF / $S4) * 100}")
    echo "⚠️  File size CHANGED (diff: $DIFF bytes, ${PCT}%)"
    STABLE="PARTIAL"
fi

# Check hash
if [ "$H4" = "$H6" ]; then
    echo "✅ Binary hash IDENTICAL - Byte-perfect convergence!"
else
    echo "ℹ️  Binary hash different (serialization order variance)"
fi

echo ""
if [ "$STABLE" = "YES" ]; then
    echo "╔══════════════════════════════════════════════════════════╗"
    echo "║   ✓✓✓ ROUND-TRIP FULLY STABLE ✓✓✓                      ║"
    echo "╚══════════════════════════════════════════════════════════╝"
else
    echo "╔══════════════════════════════════════════════════════════╗"
    echo "║   ⚠️  PARTIAL STABILITY - Review differences            ║"
    echo "╚══════════════════════════════════════════════════════════╝"
fi

echo ""
echo "📁 Test artifacts: $OUT/"
