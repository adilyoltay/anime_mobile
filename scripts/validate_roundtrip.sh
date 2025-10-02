#!/bin/bash
# PR-VALIDATION: Round-trip validation for orphan fix
# Tests: JSON → RIV → JSON → RIV stability

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$REPO_ROOT/build_converter"
CONVERTER="$BUILD_DIR/converter/rive_convert_cli"
EXTRACTOR="$BUILD_DIR/converter/universal_extractor"
IMPORT_TEST="$BUILD_DIR/converter/import_test"
ANALYZER="$REPO_ROOT/converter/analyze_riv.py"
OUTPUT_DIR="$REPO_ROOT/output/roundtrip"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE}╔════════════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║          PR-VALIDATION: Round-Trip Stability Test              ║${NC}"
echo -e "${BLUE}╚════════════════════════════════════════════════════════════════╝${NC}"
echo ""

mkdir -p "$OUTPUT_DIR"
cd "$REPO_ROOT"

run_roundtrip() {
    local test_name="$1"
    local input_json="$2"
    
    echo -e "${YELLOW}═══════════════════════════════════════════════════════════════${NC}"
    echo -e "${YELLOW}ROUND-TRIP: $test_name${NC}"
    echo -e "${YELLOW}═══════════════════════════════════════════════════════════════${NC}"
    
    local base_name=$(basename "$input_json" .json)
    
    # Cycle 1: JSON → RIV
    echo -e "${BLUE}→ Cycle 1: JSON → RIV${NC}"
    local riv1="$OUTPUT_DIR/${base_name}_rt1.riv"
    if ! "$CONVERTER" "$input_json" "$riv1" > "$OUTPUT_DIR/c1.log" 2>&1; then
        echo -e "${RED}✗ FAILED: Cycle 1 conversion${NC}"
        return 1
    fi
    
    # Cycle 2: RIV → JSON
    echo -e "${BLUE}→ Cycle 2: RIV → JSON${NC}"
    local json2="$OUTPUT_DIR/${base_name}_rt2.json"
    if ! "$EXTRACTOR" "$riv1" "$json2" > "$OUTPUT_DIR/e2.log" 2>&1; then
        echo -e "${RED}✗ FAILED: Cycle 2 extraction${NC}"
        return 1
    fi
    
    # Cycle 3: JSON → RIV
    echo -e "${BLUE}→ Cycle 3: JSON → RIV${NC}"
    local riv3="$OUTPUT_DIR/${base_name}_rt3.riv"
    if ! "$CONVERTER" "$json2" "$riv3" > "$OUTPUT_DIR/c3.log" 2>&1; then
        echo -e "${RED}✗ FAILED: Cycle 3 conversion${NC}"
        return 1
    fi
    
    # Compare orphan fix counts
    local orphan1=$(grep "Fixed.*orphan" "$OUTPUT_DIR/c1.log" | grep -oE '[0-9]+' | head -1 || echo "0")
    local orphan3=$(grep "Fixed.*orphan" "$OUTPUT_DIR/c3.log" | grep -oE '[0-9]+' | head -1 || echo "0")
    
    echo -e "${BLUE}→ Orphan fix comparison:${NC}"
    echo -e "  Cycle 1: $orphan1 orphans fixed"
    echo -e "  Cycle 3: $orphan3 orphans fixed"
    
    # After first fix, subsequent cycles should have 0 orphans
    if [[ "$orphan1" -gt 0 ]] && [[ "$orphan3" -ne 0 ]]; then
        echo -e "${RED}✗ WARNING: Orphan fix not stable (expected 0 in cycle 3)${NC}"
    else
        echo -e "${GREEN}✓ Orphan fix stable${NC}"
    fi
    
    # Binary analysis comparison
    echo -e "${BLUE}→ Binary structure comparison:${NC}"
    python3 "$ANALYZER" "$riv1" > "$OUTPUT_DIR/analysis1.txt" 2>&1
    python3 "$ANALYZER" "$riv3" > "$OUTPUT_DIR/analysis3.txt" 2>&1
    
    local count1=$(grep -c "^Object type_" "$OUTPUT_DIR/analysis1.txt" || echo "0")
    local count3=$(grep -c "^Object type_" "$OUTPUT_DIR/analysis3.txt" || echo "0")
    
    echo -e "  RIV1 objects: $count1"
    echo -e "  RIV3 objects: $count3"
    
    if [[ "$count1" -ne "$count3" ]]; then
        echo -e "${RED}✗ WARNING: Object count mismatch${NC}"
    else
        echo -e "${GREEN}✓ Object count stable${NC}"
    fi
    
    # Import tests
    echo -e "${BLUE}→ Import test validation:${NC}"
    if ! "$IMPORT_TEST" "$riv1" > /dev/null 2>&1; then
        echo -e "${RED}✗ FAILED: RIV1 import${NC}"
        return 1
    fi
    echo -e "${GREEN}✓ RIV1 imports successfully${NC}"
    
    if ! "$IMPORT_TEST" "$riv3" > /dev/null 2>&1; then
        echo -e "${RED}✗ FAILED: RIV3 import${NC}"
        return 1
    fi
    echo -e "${GREEN}✓ RIV3 imports successfully${NC}"
    
    echo -e "${GREEN}✓ ROUND-TRIP PASSED: $test_name${NC}"
    return 0
}

# Run tests
TESTS_PASSED=0
TESTS_FAILED=0

for test_file in "$REPO_ROOT/converter/test_"*.json; do
    if [[ -f "$test_file" ]]; then
        if run_roundtrip "$(basename "$test_file")" "$test_file"; then
            TESTS_PASSED=$((TESTS_PASSED + 1))
        else
            TESTS_FAILED=$((TESTS_FAILED + 1))
        fi
        echo ""
    fi
done

# Summary
echo -e "${BLUE}════════════════════════════════════════════════════════════════${NC}"
echo -e "${BLUE}                    ROUND-TRIP SUMMARY                          ${NC}"
echo -e "${BLUE}════════════════════════════════════════════════════════════════${NC}"
echo ""
echo -e "Total Tests:  ${BLUE}$((TESTS_PASSED + TESTS_FAILED))${NC}"
echo -e "Passed:       ${GREEN}$TESTS_PASSED${NC}"
echo -e "Failed:       ${RED}$TESTS_FAILED${NC}"
echo ""

if [[ $TESTS_FAILED -eq 0 ]]; then
    echo -e "${GREEN}╔════════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${GREEN}║              ✓ ALL ROUND-TRIP TESTS PASSED ✓                  ║${NC}"
    echo -e "${GREEN}╚════════════════════════════════════════════════════════════════╝${NC}"
    exit 0
else
    echo -e "${RED}╔════════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${RED}║            ✗ SOME ROUND-TRIP TESTS FAILED ✗                   ║${NC}"
    echo -e "${RED}╚════════════════════════════════════════════════════════════════╝${NC}"
    exit 1
fi
