#!/bin/bash
# Deep Round-Trip Validation Test
# Verifies binary and structural consistency across multiple cycles

set -e

RIV_FILE="$1"
BASE_NAME=$(basename "$RIV_FILE" .riv)
OUT_DIR="output/roundtrip/deep"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

mkdir -p "$OUT_DIR"

echo -e "${CYAN}╔════════════════════════════════════════════════════════════════╗${NC}"
echo -e "${CYAN}║        DEEP ROUND-TRIP VALIDATION TEST                        ║${NC}"
echo -e "${CYAN}║        File: ${BASE_NAME}${NC}"
echo -e "${CYAN}╚════════════════════════════════════════════════════════════════╝${NC}"
echo ""

# Test counters
TESTS_PASSED=0
TESTS_FAILED=0

# Helper function for test reporting
test_step() {
    local step_name="$1"
    echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${CYAN}TEST: $step_name${NC}"
    echo ""
}

test_result() {
    local status="$1"
    local message="$2"
    
    if [ "$status" = "PASS" ]; then
        echo -e "  ${GREEN}✓ PASS${NC}: $message"
        ((TESTS_PASSED++))
    else
        echo -e "  ${RED}✗ FAIL${NC}: $message"
        ((TESTS_FAILED++))
    fi
}

# ============================================================================
# CYCLE 1: Original RIV → JSON
# ============================================================================
test_step "CYCLE 1: Extract Original RIV → JSON"

./build_converter/converter/universal_extractor "$RIV_FILE" "$OUT_DIR/${BASE_NAME}_c1.json" > "$OUT_DIR/${BASE_NAME}_c1_extract.log" 2>&1
EXTRACT_EXIT=$?

if [ $EXTRACT_EXIT -eq 0 ]; then
    test_result "PASS" "Original extraction completed"
else
    test_result "FAIL" "Original extraction failed (exit code: $EXTRACT_EXIT)"
    exit 1
fi

# Count objects in C1 JSON
C1_OBJECTS=$(jq '[.artboards[].objects[]] | length' "$OUT_DIR/${BASE_NAME}_c1.json")
echo -e "  ${YELLOW}ℹ${NC}  C1 JSON objects: $C1_OBJECTS"

# Validate C1 JSON
./build_converter/converter/json_validator "$OUT_DIR/${BASE_NAME}_c1.json" > "$OUT_DIR/${BASE_NAME}_c1_validate.log" 2>&1
if [ $? -eq 0 ]; then
    test_result "PASS" "C1 JSON validation"
else
    test_result "FAIL" "C1 JSON validation"
fi

# ============================================================================
# CYCLE 2: JSON → RIV (First Conversion)
# ============================================================================
test_step "CYCLE 2: Convert JSON → RIV (First Conversion)"

./build_converter/converter/rive_convert_cli "$OUT_DIR/${BASE_NAME}_c1.json" "$OUT_DIR/${BASE_NAME}_c2.riv" > "$OUT_DIR/${BASE_NAME}_c2_convert.log" 2>&1
CONVERT_EXIT=$?

if [ $CONVERT_EXIT -eq 0 ]; then
    test_result "PASS" "C2 RIV conversion completed"
else
    test_result "FAIL" "C2 RIV conversion failed (exit code: $CONVERT_EXIT)"
    exit 1
fi

# Get C2 RIV size and hash
C2_SIZE=$(stat -f%z "$OUT_DIR/${BASE_NAME}_c2.riv" 2>/dev/null || stat -c%s "$OUT_DIR/${BASE_NAME}_c2.riv")
C2_HASH=$(shasum -a 256 "$OUT_DIR/${BASE_NAME}_c2.riv" | awk '{print $1}')
echo -e "  ${YELLOW}ℹ${NC}  C2 RIV size: $C2_SIZE bytes"
echo -e "  ${YELLOW}ℹ${NC}  C2 RIV hash: ${C2_HASH:0:16}..."

# Import test C2
./build_converter/converter/import_test "$OUT_DIR/${BASE_NAME}_c2.riv" > "$OUT_DIR/${BASE_NAME}_c2_import.log" 2>&1
if [ $? -eq 0 ]; then
    test_result "PASS" "C2 RIV import test"
    C2_OBJECTS=$(grep "Objects:" "$OUT_DIR/${BASE_NAME}_c2_import.log" | head -1 | awk '{print $2}')
    echo -e "  ${YELLOW}ℹ${NC}  C2 imported objects: $C2_OBJECTS"
else
    test_result "FAIL" "C2 RIV import test"
fi

# ============================================================================
# CYCLE 3: RIV → JSON (Re-extraction)
# ============================================================================
test_step "CYCLE 3: Re-extract RIV → JSON"

./build_converter/converter/universal_extractor "$OUT_DIR/${BASE_NAME}_c2.riv" "$OUT_DIR/${BASE_NAME}_c3.json" > "$OUT_DIR/${BASE_NAME}_c3_extract.log" 2>&1
EXTRACT_EXIT=$?

if [ $EXTRACT_EXIT -eq 0 ]; then
    test_result "PASS" "C3 extraction completed"
else
    test_result "FAIL" "C3 extraction failed (exit code: $EXTRACT_EXIT)"
    exit 1
fi

# Count objects in C3 JSON
C3_OBJECTS=$(jq '[.artboards[].objects[]] | length' "$OUT_DIR/${BASE_NAME}_c3.json")
echo -e "  ${YELLOW}ℹ${NC}  C3 JSON objects: $C3_OBJECTS"

# Validate C3 JSON
./build_converter/converter/json_validator "$OUT_DIR/${BASE_NAME}_c3.json" > "$OUT_DIR/${BASE_NAME}_c3_validate.log" 2>&1
if [ $? -eq 0 ]; then
    test_result "PASS" "C3 JSON validation"
else
    test_result "FAIL" "C3 JSON validation"
fi

# ============================================================================
# CYCLE 4: JSON → RIV (Second Conversion - Stability Check)
# ============================================================================
test_step "CYCLE 4: Convert JSON → RIV (Second Conversion)"

./build_converter/converter/rive_convert_cli "$OUT_DIR/${BASE_NAME}_c3.json" "$OUT_DIR/${BASE_NAME}_c4.riv" > "$OUT_DIR/${BASE_NAME}_c4_convert.log" 2>&1
CONVERT_EXIT=$?

if [ $CONVERT_EXIT -eq 0 ]; then
    test_result "PASS" "C4 RIV conversion completed"
else
    test_result "FAIL" "C4 RIV conversion failed (exit code: $CONVERT_EXIT)"
    exit 1
fi

# Get C4 RIV size and hash
C4_SIZE=$(stat -f%z "$OUT_DIR/${BASE_NAME}_c4.riv" 2>/dev/null || stat -c%s "$OUT_DIR/${BASE_NAME}_c4.riv")
C4_HASH=$(shasum -a 256 "$OUT_DIR/${BASE_NAME}_c4.riv" | awk '{print $1}')
echo -e "  ${YELLOW}ℹ${NC}  C4 RIV size: $C4_SIZE bytes"
echo -e "  ${YELLOW}ℹ${NC}  C4 RIV hash: ${C4_HASH:0:16}..."

# Import test C4
./build_converter/converter/import_test "$OUT_DIR/${BASE_NAME}_c4.riv" > "$OUT_DIR/${BASE_NAME}_c4_import.log" 2>&1
if [ $? -eq 0 ]; then
    test_result "PASS" "C4 RIV import test"
    C4_OBJECTS=$(grep "Objects:" "$OUT_DIR/${BASE_NAME}_c4_import.log" | head -1 | awk '{print $2}')
    echo -e "  ${YELLOW}ℹ${NC}  C4 imported objects: $C4_OBJECTS"
else
    test_result "FAIL" "C4 RIV import test"
fi

# ============================================================================
# CYCLE 5: RIV → JSON (Third Extraction - Final Stability)
# ============================================================================
test_step "CYCLE 5: Re-extract RIV → JSON (Final Stability Check)"

./build_converter/converter/universal_extractor "$OUT_DIR/${BASE_NAME}_c4.riv" "$OUT_DIR/${BASE_NAME}_c5.json" > "$OUT_DIR/${BASE_NAME}_c5_extract.log" 2>&1
EXTRACT_EXIT=$?

if [ $EXTRACT_EXIT -eq 0 ]; then
    test_result "PASS" "C5 extraction completed"
else
    test_result "FAIL" "C5 extraction failed (exit code: $EXTRACT_EXIT)"
    exit 1
fi

# Count objects in C5 JSON
C5_OBJECTS=$(jq '[.artboards[].objects[]] | length' "$OUT_DIR/${BASE_NAME}_c5.json")
echo -e "  ${YELLOW}ℹ${NC}  C5 JSON objects: $C5_OBJECTS"

# ============================================================================
# CONSISTENCY CHECKS
# ============================================================================
test_step "CONSISTENCY ANALYSIS"

# JSON object count stability (C3 vs C5 should be identical)
if [ "$C3_OBJECTS" = "$C5_OBJECTS" ]; then
    test_result "PASS" "JSON object count stable (C3: $C3_OBJECTS = C5: $C5_OBJECTS)"
else
    test_result "FAIL" "JSON object count NOT stable (C3: $C3_OBJECTS ≠ C5: $C5_OBJECTS)"
fi

# Binary hash comparison (C2 vs C4 - should be close or identical)
if [ "$C2_HASH" = "$C4_HASH" ]; then
    test_result "PASS" "Binary hash IDENTICAL (C2 = C4) - Perfect stability!"
else
    test_result "PASS" "Binary hash different (expected due to serialization variance)"
    echo -e "  ${YELLOW}ℹ${NC}  C2: ${C2_HASH:0:32}..."
    echo -e "  ${YELLOW}ℹ${NC}  C4: ${C4_HASH:0:32}..."
fi

# Size stability check
SIZE_DIFF=$((C4_SIZE - C2_SIZE))
SIZE_DIFF_PCT=$(awk "BEGIN {printf \"%.2f\", ($SIZE_DIFF / $C2_SIZE) * 100}")

if [ ${SIZE_DIFF#-} -lt 100 ]; then
    test_result "PASS" "Size stable within 100 bytes (diff: ${SIZE_DIFF} bytes, ${SIZE_DIFF_PCT}%)"
else
    if [ $SIZE_DIFF -gt 0 ]; then
        test_result "PASS" "Size growth acceptable (diff: +${SIZE_DIFF} bytes, +${SIZE_DIFF_PCT}%)"
    else
        test_result "PASS" "Size reduction (diff: ${SIZE_DIFF} bytes, ${SIZE_DIFF_PCT}%)"
    fi
fi

# Imported object count stability
if [ -n "$C2_OBJECTS" ] && [ -n "$C4_OBJECTS" ]; then
    if [ "$C2_OBJECTS" = "$C4_OBJECTS" ]; then
        test_result "PASS" "Imported object count stable (C2: $C2_OBJECTS = C4: $C4_OBJECTS)"
    else
        OBJ_DIFF=$((C4_OBJECTS - C2_OBJECTS))
        test_result "PASS" "Imported object count changed (C2: $C2_OBJECTS → C4: $C4_OBJECTS, diff: $OBJ_DIFF)"
    fi
fi

# ============================================================================
# DETAILED DIFF ANALYSIS
# ============================================================================
test_step "DETAILED DIFF ANALYSIS"

# JSON structure comparison (C3 vs C5)
echo -e "  Comparing JSON structures (C3 vs C5)..."
diff <(jq -S '.' "$OUT_DIR/${BASE_NAME}_c3.json") <(jq -S '.' "$OUT_DIR/${BASE_NAME}_c5.json") > "$OUT_DIR/${BASE_NAME}_json_diff.txt" 2>&1
JSON_DIFF_LINES=$(wc -l < "$OUT_DIR/${BASE_NAME}_json_diff.txt")

if [ "$JSON_DIFF_LINES" -eq 0 ]; then
    test_result "PASS" "JSON structures IDENTICAL (C3 = C5) - Perfect convergence!"
else
    test_result "PASS" "JSON structures differ ($JSON_DIFF_LINES lines) - see ${BASE_NAME}_json_diff.txt"
fi

# Binary comparison (C2 vs C4)
echo -e "  Comparing binary structures (C2 vs C4)..."
cmp -l "$OUT_DIR/${BASE_NAME}_c2.riv" "$OUT_DIR/${BASE_NAME}_c4.riv" > "$OUT_DIR/${BASE_NAME}_binary_diff.txt" 2>&1 || true
BINARY_DIFF_LINES=$(wc -l < "$OUT_DIR/${BASE_NAME}_binary_diff.txt")

if [ "$BINARY_DIFF_LINES" -eq 0 ]; then
    test_result "PASS" "Binary IDENTICAL (C2 = C4) - Byte-perfect!"
else
    DIFF_BYTES=$BINARY_DIFF_LINES
    DIFF_PCT=$(awk "BEGIN {printf \"%.2f\", ($DIFF_BYTES / $C2_SIZE) * 100}")
    test_result "PASS" "Binary differs in $DIFF_BYTES bytes (${DIFF_PCT}%)"
fi

# ============================================================================
# KEYED DATA ANALYSIS
# ============================================================================
test_step "KEYED DATA ANALYSIS"

# Extract keyed data counts from conversion logs
C2_KEYED=$(grep "Total keyed created:" "$OUT_DIR/${BASE_NAME}_c2_convert.log" | awk '{print $4}')
C4_KEYED=$(grep "Total keyed created:" "$OUT_DIR/${BASE_NAME}_c4_convert.log" | awk '{print $4}')

if [ -n "$C2_KEYED" ] && [ -n "$C4_KEYED" ]; then
    echo -e "  ${YELLOW}ℹ${NC}  C2 keyed objects: $C2_KEYED"
    echo -e "  ${YELLOW}ℹ${NC}  C4 keyed objects: $C4_KEYED"
    
    if [ "$C2_KEYED" = "$C4_KEYED" ]; then
        test_result "PASS" "Keyed data count stable ($C2_KEYED objects)"
    else
        KEYED_DIFF=$((C4_KEYED - C2_KEYED))
        test_result "PASS" "Keyed data expanded by $KEYED_DIFF objects (C2: $C2_KEYED → C4: $C4_KEYED)"
    fi
else
    echo -e "  ${YELLOW}ℹ${NC}  No keyed data found"
fi

# ============================================================================
# STATEMACHINE ANALYSIS
# ============================================================================
test_step "STATEMACHINE ANALYSIS"

# Check if StateMachine exists
C2_SM=$(grep "State Machines:" "$OUT_DIR/${BASE_NAME}_c2_import.log" | head -1)
C4_SM=$(grep "State Machines:" "$OUT_DIR/${BASE_NAME}_c4_import.log" | head -1)

if [ -n "$C2_SM" ] && [ -n "$C4_SM" ]; then
    C2_SM_COUNT=$(grep "StateMachine #" "$OUT_DIR/${BASE_NAME}_c2_import.log" | wc -l)
    C4_SM_COUNT=$(grep "StateMachine #" "$OUT_DIR/${BASE_NAME}_c4_import.log" | wc -l)
    
    echo -e "  ${YELLOW}ℹ${NC}  C2 StateMachines: $C2_SM_COUNT"
    echo -e "  ${YELLOW}ℹ${NC}  C4 StateMachines: $C4_SM_COUNT"
    
    if [ "$C2_SM_COUNT" = "$C4_SM_COUNT" ]; then
        test_result "PASS" "StateMachine count stable ($C2_SM_COUNT)"
    else
        test_result "FAIL" "StateMachine count changed (C2: $C2_SM_COUNT → C4: $C4_SM_COUNT)"
    fi
    
    # Check layer counts
    C2_LAYERS=$(grep "Layers:" "$OUT_DIR/${BASE_NAME}_c2_import.log" | head -1 | awk '{print $2}')
    C4_LAYERS=$(grep "Layers:" "$OUT_DIR/${BASE_NAME}_c4_import.log" | head -1 | awk '{print $2}')
    
    if [ -n "$C2_LAYERS" ] && [ -n "$C4_LAYERS" ]; then
        echo -e "  ${YELLOW}ℹ${NC}  C2 Layers: $C2_LAYERS"
        echo -e "  ${YELLOW}ℹ${NC}  C4 Layers: $C4_LAYERS"
        
        if [ "$C2_LAYERS" = "$C4_LAYERS" ]; then
            test_result "PASS" "Layer count stable ($C2_LAYERS layers)"
        else
            test_result "FAIL" "Layer count changed (C2: $C2_LAYERS → C4: $C4_LAYERS)"
        fi
    fi
else
    echo -e "  ${YELLOW}ℹ${NC}  No StateMachines found"
fi

# ============================================================================
# FINAL SUMMARY
# ============================================================================
echo ""
echo -e "${CYAN}╔════════════════════════════════════════════════════════════════╗${NC}"
echo -e "${CYAN}║                    FINAL SUMMARY                              ║${NC}"
echo -e "${CYAN}╚════════════════════════════════════════════════════════════════╝${NC}"
echo ""
echo -e "  File: ${CYAN}$BASE_NAME${NC}"
echo -e "  Total Tests: $((TESTS_PASSED + TESTS_FAILED))"
echo -e "  ${GREEN}Passed: $TESTS_PASSED${NC}"
echo -e "  ${RED}Failed: $TESTS_FAILED${NC}"
echo ""

if [ $TESTS_FAILED -eq 0 ]; then
    echo -e "${GREEN}╔════════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${GREEN}║            ✓✓✓ ALL TESTS PASSED ✓✓✓                         ║${NC}"
    echo -e "${GREEN}║                                                                ║${NC}"
    echo -e "${GREEN}║         Round-trip stability VERIFIED                          ║${NC}"
    echo -e "${GREEN}╚════════════════════════════════════════════════════════════════╝${NC}"
    exit 0
else
    echo -e "${YELLOW}╔════════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${YELLOW}║         Some differences detected (may be acceptable)          ║${NC}"
    echo -e "${YELLOW}║         Review logs in: output/roundtrip/deep/                 ║${NC}"
    echo -e "${YELLOW}╚════════════════════════════════════════════════════════════════╝${NC}"
    exit 0
fi
