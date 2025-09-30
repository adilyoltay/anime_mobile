#!/bin/bash

echo "================================================================================
"
echo "           🔍 ANALYZING ALL EXAMPLE RIV FILES"
echo "================================================================================
"

for riv in converter/exampleriv/*.riv; do
    name=$(basename "$riv")
    echo ""
    echo "📄 $name"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    
    # File size
    size=$(ls -lh "$riv" | awk '{print $5}')
    echo "   Size: $size"
    
    # Object count and artboards
    ./build_converter/converter/import_test "$riv" 2>&1 | grep -E "Artboard count:|Objects:" | head -2
    
    # Type breakdown
    echo "   Types:"
    ./build_converter/converter/import_test "$riv" 2>&1 | \
        grep "Object typeKey=" | sort | uniq -c | sort -rn | head -10 | \
        awk '{printf "      %3d × typeKey=%s\n", $1, $3}'
done

echo ""
echo "================================================================================
"

