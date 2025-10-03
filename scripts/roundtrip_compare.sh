#!/bin/bash
# Round-trip RIV comparison workflow
# Extract original RIV → Convert back → Compare

set -e

REPO_ROOT=$(pwd)
export REPO_ROOT

get_riv_metrics() {
    local riv_path="$1"
    python3 - "$riv_path" <<'PY'
import json
import os
import sys
from pathlib import Path

repo_root = Path(os.environ["REPO_ROOT"])
sys.path.insert(0, str(repo_root / "converter"))

from analyze_riv import analyze  # noqa: E402

path = Path(sys.argv[1])
summary = analyze(path, return_data=True)
objects = summary.get("objects", [])

KEYED_TYPES = {25, 26, 28, 30, 37, 50, 84, 138, 139, 142, 171, 174, 175, 450}

metrics = {
    "objects": len(objects),
    "keyed": sum(1 for obj in objects if obj.get("typeKey") in KEYED_TYPES),
}

print(json.dumps(metrics))
PY
}

if [ $# -lt 1 ]; then
    echo "Usage: $0 <original.riv>"
    echo ""
    echo "Performs a complete round-trip comparison:"
    echo "  1. Extract original RIV to JSON"
    echo "  2. Convert JSON back to RIV"
    echo "  3. Import test (Rive Play compatibility)"
    echo "  4. Compare original vs round-trip"
    echo "  5. Track growth metrics"
    echo ""
    exit 1
fi

ORIGINAL_RIV="$1"
BASENAME=$(basename "$ORIGINAL_RIV" .riv)
OUTPUT_DIR="output/roundtrip"
EXTRACTOR="build_converter/converter/universal_extractor"
CONVERTER="build_converter/converter/rive_convert_cli"
IMPORT_TEST="build_converter/converter/import_test"

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
RED='\033[0;31m'
RESET='\033[0m'

echo "======================================"
echo "Round-trip RIV Comparison"
echo "======================================"
echo ""
echo "Original: $ORIGINAL_RIV"
echo ""

# Capture baseline metrics for original file
ORIGINAL_METRICS=$(get_riv_metrics "$ORIGINAL_RIV")
EXPECTED_OBJECTS=$(echo "$ORIGINAL_METRICS" | jq -r '.objects')
EXPECTED_KEYED=$(echo "$ORIGINAL_METRICS" | jq -r '.keyed')

# Create output directory
mkdir -p "$OUTPUT_DIR"

# Step 1: Extract to JSON
echo -e "${BLUE}[1/5]${RESET} Extracting RIV to JSON..."
EXTRACTED_JSON="$OUTPUT_DIR/${BASENAME}_extracted.json"
if ! "$EXTRACTOR" "$ORIGINAL_RIV" "$EXTRACTED_JSON" > /dev/null 2>&1; then
    echo "❌ FAILED: Extraction failed"
    "$EXTRACTOR" "$ORIGINAL_RIV" "$EXTRACTED_JSON"
    exit 1
fi
echo -e "${GREEN}✅${RESET} Extracted to: $EXTRACTED_JSON"

# Step 2: Convert back to RIV
echo -e "${BLUE}[2/5]${RESET} Converting JSON back to RIV..."
ROUNDTRIP_RIV="$OUTPUT_DIR/${BASENAME}_roundtrip.riv"

# Check if JSON is in exact mode
IS_EXACT=$(grep -q '"__riv_exact__".*true' "$EXTRACTED_JSON" && echo "yes" || echo "no")

if [ "$IS_EXACT" = "yes" ]; then
    # Use --exact flag for exact mode JSON
    if ! "$CONVERTER" --exact "$EXTRACTED_JSON" "$ROUNDTRIP_RIV" > /dev/null 2>&1; then
        echo "❌ FAILED: Exact conversion failed"
        "$CONVERTER" --exact "$EXTRACTED_JSON" "$ROUNDTRIP_RIV"
        exit 1
    fi
    echo -e "${GREEN}✅${RESET} Converted with --exact flag to: $ROUNDTRIP_RIV"
else
    # Use auto-detect mode for universal JSON
    if ! "$CONVERTER" "$EXTRACTED_JSON" "$ROUNDTRIP_RIV" > /dev/null 2>&1; then
        echo "❌ FAILED: Conversion failed"
        "$CONVERTER" "$EXTRACTED_JSON" "$ROUNDTRIP_RIV"
        exit 1
    fi
    echo -e "${GREEN}✅${RESET} Converted to: $ROUNDTRIP_RIV"
fi

# Step 3: Import test (Rive Play compatibility)
echo -e "${BLUE}[3/5]${RESET} Testing Rive Play compatibility..."
IMPORT_LOG="$OUTPUT_DIR/${BASENAME}_import.log"

# For exact mode, skip object count validation (byte-perfect reconstruction is the goal)
# For universal mode, validate object counts
if [ "$IS_EXACT" = "yes" ]; then
    if ! "$IMPORT_TEST" "$ROUNDTRIP_RIV" > "$IMPORT_LOG" 2>&1; then
        echo -e "${RED}❌ FAILED: Import test failed (Rive Play will crash!)${RESET}"
        echo ""
        echo "Import test output:"
        cat "$IMPORT_LOG"
        exit 1
    fi
else
    if ! "$IMPORT_TEST" "$ROUNDTRIP_RIV" "$EXPECTED_OBJECTS" > "$IMPORT_LOG" 2>&1; then
        echo -e "${RED}❌ FAILED: Import test failed (Rive Play will crash!)${RESET}"
        echo ""
        echo "Import test output:"
        cat "$IMPORT_LOG"
        exit 1
    fi
fi

# Check for NULL objects (now a hard failure thanks to import_test update)
if grep -q "NULL!" "$IMPORT_LOG"; then
    NULL_COUNT=$(grep -c "NULL!" "$IMPORT_LOG")
else
    NULL_COUNT=0
fi
if [ $NULL_COUNT -gt 0 ]; then
    echo -e "${RED}❌ FAILED: Found $NULL_COUNT NULL objects${RESET}"
    echo "   Serialization defects detected - these will crash Rive Play!"
    echo ""
    echo "NULL object details:"
    grep "NULL!" "$IMPORT_LOG" | head -20
    exit 1
else
    echo -e "${GREEN}✅${RESET} Import test passed (no NULL objects)"
fi

# For universal mode, validate object counts; for exact mode, rely on byte comparison
if [ "$IS_EXACT" = "no" ]; then
    ROUNDTRIP_METRICS=$(get_riv_metrics "$ROUNDTRIP_RIV")
    ROUNDTRIP_OBJECTS=$(echo "$ROUNDTRIP_METRICS" | jq -r '.objects')
    ROUNDTRIP_KEYED=$(echo "$ROUNDTRIP_METRICS" | jq -r '.keyed')

    if [ "$ROUNDTRIP_OBJECTS" -ne "$EXPECTED_OBJECTS" ]; then
        echo -e "${RED}❌ FAILED: Object count mismatch (expected $EXPECTED_OBJECTS, got $ROUNDTRIP_OBJECTS)${RESET}"
        exit 1
    else
        echo -e "${GREEN}✅${RESET} Object count matches source ($ROUNDTRIP_OBJECTS)"
    fi

    if [ "$ROUNDTRIP_KEYED" -ne "$EXPECTED_KEYED" ]; then
        echo -e "${RED}❌ FAILED: Keyed object count mismatch (expected $EXPECTED_KEYED, got $ROUNDTRIP_KEYED)${RESET}"
        exit 1
    else
        echo -e "${GREEN}✅${RESET} Keyed object count matches source ($ROUNDTRIP_KEYED)"
    fi
else
    echo -e "${GREEN}✅${RESET} Exact mode: Skipping object count validation (byte-perfect comparison used instead)"
fi

# Step 4: Compare
echo -e "${BLUE}[4/5]${RESET} Comparing original vs round-trip..."
echo ""

# For exact mode, perform byte-perfect comparison first
if [ "$IS_EXACT" = "yes" ]; then
    if cmp -s "$ORIGINAL_RIV" "$ROUNDTRIP_RIV"; then
        echo -e "${GREEN}✅${RESET} Byte-perfect round-trip: Files are identical!"
        BYTE_PERFECT=true
    else
        echo -e "${RED}❌${RESET} Byte-perfect round-trip FAILED: Files differ"
        BYTE_PERFECT=false
        COMPARISON_FAILED=1
    fi
    echo ""
fi

# Run visual comparison (don't exit on diff)
python3 scripts/compare_riv_files.py "$ORIGINAL_RIV" "$ROUNDTRIP_RIV" || COMPARISON_FAILED=$?

# Save JSON report (always generate, even on diff)
REPORT_JSON="$OUTPUT_DIR/${BASENAME}_comparison.json"
set +e  # Temporarily disable exit-on-error
python3 scripts/compare_riv_files.py "$ORIGINAL_RIV" "$ROUNDTRIP_RIV" --json > "$REPORT_JSON"
JSON_EXIT=$?
set -e  # Re-enable exit-on-error

# Check JSON generation result
if [ $JSON_EXIT -eq 0 ] || [ $JSON_EXIT -eq 1 ]; then
    # Exit 0 (match) or 1 (diff) are both OK
    echo -e "${YELLOW}📄${RESET} Comparison report saved: $REPORT_JSON"
else
    # Exit 2+ is a real error (file not found, parse error, etc.)
    echo -e "❌ FAILED: JSON report generation failed (exit code: $JSON_EXIT)"
    exit $JSON_EXIT
fi

# Step 5: Growth tracking
echo -e "${BLUE}[5/5]${RESET} Tracking growth metrics..."
echo ""

GROWTH_JSON="$OUTPUT_DIR/${BASENAME}_growth.json"
set +e  # Temporarily disable exit-on-error
python3 scripts/track_roundtrip_growth.py "$ORIGINAL_RIV" "$ROUNDTRIP_RIV" --json > "$GROWTH_JSON"
GROWTH_EXIT=$?

# Show growth summary (also capture exit code)
python3 scripts/track_roundtrip_growth.py "$ORIGINAL_RIV" "$ROUNDTRIP_RIV"
GROWTH_SUMMARY_EXIT=$?
set -e  # Re-enable exit-on-error

# Use whichever exit code is worse (higher value = worse)
if [ $GROWTH_SUMMARY_EXIT -gt $GROWTH_EXIT ]; then
    GROWTH_EXIT=$GROWTH_SUMMARY_EXIT
fi

echo -e "${YELLOW}📊${RESET} Growth metrics saved: $GROWTH_JSON"
echo ""

# Final status
if [ -n "$COMPARISON_FAILED" ]; then
    echo -e "${YELLOW}⚠️  Differences detected (exit code: $COMPARISON_FAILED)${RESET}"
    echo -e "   Review the report above for details."
    exit $COMPARISON_FAILED
elif [ $GROWTH_EXIT -eq 2 ]; then
    echo -e "${RED}❌ Growth threshold exceeded!${RESET}"
    exit 2
elif [ $GROWTH_EXIT -eq 1 ]; then
    echo -e "${YELLOW}⚠️  Growth warning${RESET}"
    exit 1
else
    echo -e "${GREEN}✅ Round-trip successful - files match!${RESET}"
fi
echo ""
