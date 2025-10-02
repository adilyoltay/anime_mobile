#!/bin/bash
# PR-VALIDATION: Comprehensive test suite for orphan paint auto-fix
# Tests all scenarios: orphan detection, synthetic shapes, gradient preservation, dash support

set -e  # Exit on error

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$REPO_ROOT/build_converter"
CONVERTER="$BUILD_DIR/converter/rive_convert_cli"
IMPORT_TEST="$BUILD_DIR/converter/import_test"
ANALYZER="$REPO_ROOT/converter/analyze_riv.py"
TEST_DIR="$REPO_ROOT/converter"
OUTPUT_DIR="$REPO_ROOT/output/validation"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Counters
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0

echo -e "${BLUE}╔════════════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║     PR-VALIDATION: Orphan Paint Auto-Fix Test Suite           ║${NC}"
echo -e "${BLUE}╚════════════════════════════════════════════════════════════════╝${NC}"
echo ""

# Setup
mkdir -p "$OUTPUT_DIR"
cd "$REPO_ROOT"

# Helper functions
run_test() {
    local test_name="$1"
    local json_file="$2"
    local expected_orphans="$3"
    local expected_success="$4"
    
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    echo -e "${YELLOW}═══════════════════════════════════════════════════════════════${NC}"
    echo -e "${YELLOW}TEST $TOTAL_TESTS: $test_name${NC}"
    echo -e "${YELLOW}═══════════════════════════════════════════════════════════════${NC}"
    
    local output_riv="$OUTPUT_DIR/$(basename "$json_file" .json).riv"
    
    # Convert JSON to RIV
    echo -e "${BLUE}→ Converting JSON to RIV...${NC}"
    if ! "$CONVERTER" "$json_file" "$output_riv" > "$OUTPUT_DIR/convert.log" 2>&1; then
        echo -e "${RED}✗ FAILED: Conversion failed${NC}"
        cat "$OUTPUT_DIR/convert.log"
        FAILED_TESTS=$((FAILED_TESTS + 1))
        return 1
    fi
    
    # Check orphan fix count
    local orphan_count=$(grep "Fixed.*orphan" "$OUTPUT_DIR/convert.log" | grep -oE '[0-9]+' | head -1)
    if [[ "$orphan_count" != "$expected_orphans" ]]; then
        echo -e "${RED}✗ FAILED: Expected $expected_orphans orphans fixed, got $orphan_count${NC}"
        FAILED_TESTS=$((FAILED_TESTS + 1))
        return 1
    fi
    echo -e "${GREEN}✓ Orphan count correct: $orphan_count${NC}"
    
    # Import test
    echo -e "${BLUE}→ Running import test...${NC}"
    if ! "$IMPORT_TEST" "$output_riv" > "$OUTPUT_DIR/import.log" 2>&1; then
        echo -e "${RED}✗ FAILED: Import test failed${NC}"
        cat "$OUTPUT_DIR/import.log"
        FAILED_TESTS=$((FAILED_TESTS + 1))
        return 1
    fi
    
    if ! grep -q "SUCCESS" "$OUTPUT_DIR/import.log"; then
        echo -e "${RED}✗ FAILED: Import test did not succeed${NC}"
        FAILED_TESTS=$((FAILED_TESTS + 1))
        return 1
    fi
    echo -e "${GREEN}✓ Import test passed${NC}"
    
    # Binary analysis
    echo -e "${BLUE}→ Analyzing binary structure...${NC}"
    python3 "$ANALYZER" "$output_riv" > "$OUTPUT_DIR/analysis.log" 2>&1
    
    # Verify expected types present
    if [[ "$expected_success" == "true" ]]; then
        if ! grep -q "type_3" "$OUTPUT_DIR/analysis.log"; then
            echo -e "${RED}✗ FAILED: No Shape objects found${NC}"
            FAILED_TESTS=$((FAILED_TESTS + 1))
            return 1
        fi
        echo -e "${GREEN}✓ Binary structure valid${NC}"
    fi
    
    echo -e "${GREEN}✓ TEST PASSED: $test_name${NC}"
    PASSED_TESTS=$((PASSED_TESTS + 1))
    return 0
}

# Test Suite
echo -e "${BLUE}════════════════════════════════════════════════════════════════${NC}"
echo -e "${BLUE}                    STARTING TEST SUITE                         ${NC}"
echo -e "${BLUE}════════════════════════════════════════════════════════════════${NC}"
echo ""

# Test 1: Orphan paint detection
run_test "Orphan Paint Detection" \
    "$TEST_DIR/test_orphan_paint.json" \
    "1" \
    "true"

# Test 2: Synthetic Shape reuse
run_test "Synthetic Shape Reuse (Node + PointsPath + Fill)" \
    "$TEST_DIR/test_shape_reuse.json" \
    "1" \
    "true"

# Test 3: Gradient hierarchy preservation
run_test "Gradient Hierarchy Preservation" \
    "$TEST_DIR/test_gradient_hierarchy.json" \
    "0" \
    "true"

# Test 4: Gradient with parametric path
run_test "Gradient with Parametric Path" \
    "$TEST_DIR/test_gradient_with_path.json" \
    "1" \
    "true"

# Test 5: Dash support
run_test "Dash/DashPath Support" \
    "$TEST_DIR/test_dash.json" \
    "0" \
    "true"

# Summary
echo ""
echo -e "${BLUE}════════════════════════════════════════════════════════════════${NC}"
echo -e "${BLUE}                       TEST SUMMARY                             ${NC}"
echo -e "${BLUE}════════════════════════════════════════════════════════════════${NC}"
echo ""
echo -e "Total Tests:  ${BLUE}$TOTAL_TESTS${NC}"
echo -e "Passed:       ${GREEN}$PASSED_TESTS${NC}"
echo -e "Failed:       ${RED}$FAILED_TESTS${NC}"
echo ""

if [[ $FAILED_TESTS -eq 0 ]]; then
    echo -e "${GREEN}╔════════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${GREEN}║                    ✓ ALL TESTS PASSED ✓                       ║${NC}"
    echo -e "${GREEN}╚════════════════════════════════════════════════════════════════╝${NC}"
    exit 0
else
    echo -e "${RED}╔════════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${RED}║                    ✗ SOME TESTS FAILED ✗                      ║${NC}"
    echo -e "${RED}╚════════════════════════════════════════════════════════════════╝${NC}"
    exit 1
fi
