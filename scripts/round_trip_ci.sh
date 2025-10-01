#!/bin/bash
# Rive Converter Round-trip CI Test Suite
# Tests validator → convert → import pipeline for multiple object counts

set -e  # Exit on first error

echo "======================================"
echo "Rive Converter Round-trip CI Tests"
echo "======================================"
echo ""

# Configuration
BUILD_DIR="build_converter"
VALIDATOR="${BUILD_DIR}/converter/json_validator"
CONVERTER="${BUILD_DIR}/converter/rive_convert_cli"
IMPORT_TEST="${BUILD_DIR}/converter/import_test"
EXTRACTOR="${BUILD_DIR}/converter/universal_extractor"

# Check if tools exist
if [ ! -f "$VALIDATOR" ]; then
    echo "❌ ERROR: json_validator not found at $VALIDATOR"
    echo "Run: cmake --build build_converter --target json_validator"
    exit 2
fi

if [ ! -f "$CONVERTER" ]; then
    echo "❌ ERROR: rive_convert_cli not found at $CONVERTER"
    echo "Run: cmake --build build_converter --target rive_convert_cli"
    exit 2
fi

if [ ! -f "$IMPORT_TEST" ]; then
    echo "❌ ERROR: import_test not found at $IMPORT_TEST"
    echo "Run: cmake --build build_converter --target import_test"
    exit 2
fi

# Test files (pre-generated subsets)
TESTS=(
    "output/tests/test_189_no_trim.json:189 objects"
    "output/tests/test_190_no_trim.json:190 objects (previous threshold)"
    "output/tests/test_273_no_trim.json:273 objects"
    "output/tests/bee_baby_NO_TRIMPATH.json:Full bee_baby (1142 objects)"
)

PASSED=0
FAILED=0

for test_entry in "${TESTS[@]}"; do
    IFS=: read -r test_file test_desc <<< "$test_entry"
    
    if [ ! -f "$test_file" ]; then
        echo "⚠️  SKIP: $test_desc - file not found: $test_file"
        continue
    fi
    
    echo "----------------------------------------"
    echo "Testing: $test_desc"
    echo "File: $test_file"
    echo ""
    
    # Step 1: Validate JSON
    echo "  [1/3] Validating JSON..."
    if ! "$VALIDATOR" "$test_file" > /dev/null 2>&1; then
        echo "  ❌ FAILED: JSON validation failed"
        echo ""
        "$VALIDATOR" "$test_file"
        FAILED=$((FAILED + 1))
        continue
    fi
    echo "  ✅ JSON validation passed"
    
    # Step 2: Convert to RIV
    echo "  [2/3] Converting to RIV..."
    riv_file="${test_file%.json}.riv"
    if ! "$CONVERTER" "$test_file" "$riv_file" > /dev/null 2>&1; then
        echo "  ❌ FAILED: Conversion failed"
        echo ""
        "$CONVERTER" "$test_file" "$riv_file"
        FAILED=$((FAILED + 1))
        continue
    fi
    echo "  ✅ Conversion successful"
    
    # Step 3: Import test
    echo "  [3/3] Testing import..."
    if ! "$IMPORT_TEST" "$riv_file" > /dev/null 2>&1; then
        echo "  ❌ FAILED: Import test failed"
        echo ""
        "$IMPORT_TEST" "$riv_file"
        FAILED=$((FAILED + 1))
        continue
    fi
    echo "  ✅ Import test passed"
    
    echo ""
    echo "✅ $test_desc: ALL TESTS PASSED"
    PASSED=$((PASSED + 1))
done

echo ""
echo "======================================"
echo "Test Summary"
echo "======================================"
echo "Passed: $PASSED"
echo "Failed: $FAILED"
echo ""

if [ $FAILED -eq 0 ]; then
    echo "✅ ALL TESTS PASSED"
    exit 0
else
    echo "❌ SOME TESTS FAILED"
    exit 1
fi
