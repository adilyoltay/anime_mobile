#!/bin/bash
# PR-VALIDATION: Master validation script
# Runs all validation tests and generates report

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

echo -e "${CYAN}╔════════════════════════════════════════════════════════════════╗${NC}"
echo -e "${CYAN}║                                                                ║${NC}"
echo -e "${CYAN}║          PR-VALIDATION: COMPREHENSIVE TEST SUITE               ║${NC}"
echo -e "${CYAN}║              Orphan Paint Auto-Fix Validation                  ║${NC}"
echo -e "${CYAN}║                                                                ║${NC}"
echo -e "${CYAN}╚════════════════════════════════════════════════════════════════╝${NC}"
echo ""

cd "$REPO_ROOT"

# Build check
echo -e "${BLUE}═══════════════════════════════════════════════════════════════${NC}"
echo -e "${BLUE}                       BUILD CHECK                              ${NC}"
echo -e "${BLUE}═══════════════════════════════════════════════════════════════${NC}"
echo ""

if [[ ! -f "build_converter/converter/rive_convert_cli" ]]; then
    echo -e "${RED}✗ Build not found. Running cmake build...${NC}"
    cmake --build build_converter --target rive_convert_cli import_test
fi

if [[ ! -x "build_converter/converter/rive_convert_cli" ]]; then
    echo -e "${RED}✗ FAILED: rive_convert_cli not executable${NC}"
    exit 1
fi

if [[ ! -x "build_converter/converter/import_test" ]]; then
    echo -e "${RED}✗ FAILED: import_test not executable${NC}"
    exit 1
fi

echo -e "${GREEN}✓ Build check passed${NC}"
echo ""

# Make scripts executable
chmod +x "$SCRIPT_DIR/validate_orphan_fix.sh"
chmod +x "$SCRIPT_DIR/validate_roundtrip.sh"

# Test Suite 1: Orphan Fix Validation
echo -e "${BLUE}═══════════════════════════════════════════════════════════════${NC}"
echo -e "${BLUE}          TEST SUITE 1: ORPHAN FIX VALIDATION                   ${NC}"
echo -e "${BLUE}═══════════════════════════════════════════════════════════════${NC}"
echo ""

SUITE1_RESULT=0
if "$SCRIPT_DIR/validate_orphan_fix.sh"; then
    echo -e "${GREEN}✓ Test Suite 1 PASSED${NC}"
else
    echo -e "${RED}✗ Test Suite 1 FAILED${NC}"
    SUITE1_RESULT=1
fi
echo ""

# Test Suite 2: Round-Trip Validation
echo -e "${BLUE}═══════════════════════════════════════════════════════════════${NC}"
echo -e "${BLUE}         TEST SUITE 2: ROUND-TRIP VALIDATION                    ${NC}"
echo -e "${BLUE}═══════════════════════════════════════════════════════════════${NC}"
echo ""

SUITE2_RESULT=0
if "$SCRIPT_DIR/validate_roundtrip.sh"; then
    echo -e "${GREEN}✓ Test Suite 2 PASSED${NC}"
else
    echo -e "${RED}✗ Test Suite 2 FAILED${NC}"
    SUITE2_RESULT=1
fi
echo ""

# Generate Report
REPORT_FILE="$REPO_ROOT/output/VALIDATION_REPORT.md"
mkdir -p "$(dirname "$REPORT_FILE")"

cat > "$REPORT_FILE" << EOF
# PR-VALIDATION: Test Report

**Date:** $(date "+%Y-%m-%d %H:%M:%S")  
**Branch:** $(git rev-parse --abbrev-ref HEAD)  
**Commit:** $(git rev-parse --short HEAD)

---

## Test Results Summary

### Test Suite 1: Orphan Fix Validation
**Status:** $([ $SUITE1_RESULT -eq 0 ] && echo "✅ PASSED" || echo "❌ FAILED")

Tests:
- Orphan paint detection
- Synthetic Shape reuse
- Gradient hierarchy preservation
- Gradient with parametric path
- Dash/DashPath support

### Test Suite 2: Round-Trip Validation
**Status:** $([ $SUITE2_RESULT -eq 0 ] && echo "✅ PASSED" || echo "❌ FAILED")

Tests:
- JSON → RIV → JSON → RIV stability
- Object count preservation
- Orphan fix idempotency
- Import test validation

---

## Overall Status

$([ $SUITE1_RESULT -eq 0 ] && [ $SUITE2_RESULT -eq 0 ] && echo "✅ **ALL TESTS PASSED**" || echo "❌ **SOME TESTS FAILED**")

---

## Test Artifacts

- Validation outputs: \`output/validation/\`
- Round-trip outputs: \`output/roundtrip/\`
- Full logs: Check individual test directories

---

## Next Steps

$(if [ $SUITE1_RESULT -eq 0 ] && [ $SUITE2_RESULT -eq 0 ]; then
    echo "✅ Ready to merge PR-ORPHAN-FIX"
    echo "✅ Ready for production deployment"
else
    echo "❌ Fix failing tests before merge"
    echo "❌ Review test artifacts for details"
fi)
EOF

# Final Report
echo -e "${CYAN}════════════════════════════════════════════════════════════════${NC}"
echo -e "${CYAN}                      FINAL REPORT                              ${NC}"
echo -e "${CYAN}════════════════════════════════════════════════════════════════${NC}"
echo ""
echo -e "Test Suite 1 (Orphan Fix):  $([ $SUITE1_RESULT -eq 0 ] && echo -e "${GREEN}PASSED${NC}" || echo -e "${RED}FAILED${NC}")"
echo -e "Test Suite 2 (Round-Trip):  $([ $SUITE2_RESULT -eq 0 ] && echo -e "${GREEN}PASSED${NC}" || echo -e "${RED}FAILED${NC}")"
echo ""
echo -e "Full report: ${BLUE}$REPORT_FILE${NC}"
echo ""

if [ $SUITE1_RESULT -eq 0 ] && [ $SUITE2_RESULT -eq 0 ]; then
    echo -e "${GREEN}╔════════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${GREEN}║                                                                ║${NC}"
    echo -e "${GREEN}║              ✓✓✓ ALL VALIDATION PASSED ✓✓✓                    ║${NC}"
    echo -e "${GREEN}║                                                                ║${NC}"
    echo -e "${GREEN}║            Ready for Production Deployment                     ║${NC}"
    echo -e "${GREEN}║                                                                ║${NC}"
    echo -e "${GREEN}╚════════════════════════════════════════════════════════════════╝${NC}"
    exit 0
else
    echo -e "${RED}╔════════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${RED}║                                                                ║${NC}"
    echo -e "${RED}║              ✗✗✗ VALIDATION FAILED ✗✗✗                        ║${NC}"
    echo -e "${RED}║                                                                ║${NC}"
    echo -e "${RED}║            Review Test Artifacts                               ║${NC}"
    echo -e "${RED}║                                                                ║${NC}"
    echo -e "${RED}╚════════════════════════════════════════════════════════════════╝${NC}"
    exit 1
fi
