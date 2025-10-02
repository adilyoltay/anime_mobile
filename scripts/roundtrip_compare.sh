#!/bin/bash
# Round-trip RIV comparison workflow
# Extract original RIV → Convert back → Compare

set -e

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
if ! "$CONVERTER" "$EXTRACTED_JSON" "$ROUNDTRIP_RIV" > /dev/null 2>&1; then
    echo "❌ FAILED: Conversion failed"
    "$CONVERTER" "$EXTRACTED_JSON" "$ROUNDTRIP_RIV"
    exit 1
fi
echo -e "${GREEN}✅${RESET} Converted to: $ROUNDTRIP_RIV"

# Step 3: Import test (Rive Play compatibility)
echo -e "${BLUE}[3/5]${RESET} Testing Rive Play compatibility..."
IMPORT_LOG="$OUTPUT_DIR/${BASENAME}_import.log"
if ! "$IMPORT_TEST" "$ROUNDTRIP_RIV" > "$IMPORT_LOG" 2>&1; then
    echo -e "${RED}❌ FAILED: Import test failed (Rive Play will crash!)${RESET}"
    echo ""
    echo "Import test output:"
    cat "$IMPORT_LOG"
    exit 1
fi

# Check for NULL objects (warning sign)
NULL_COUNT=$(grep -c "NULL!" "$IMPORT_LOG" || echo 0)
if [ $NULL_COUNT -gt 0 ]; then
    echo -e "${YELLOW}⚠️  WARNING: Found $NULL_COUNT NULL objects${RESET}"
    echo "   This may cause issues in Rive Play!"
else
    echo -e "${GREEN}✅${RESET} Import test passed"
fi

# Step 4: Compare
echo -e "${BLUE}[4/5]${RESET} Comparing original vs round-trip..."
echo ""

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
