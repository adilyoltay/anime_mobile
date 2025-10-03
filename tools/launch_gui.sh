#!/bin/bash
# Quick launcher for RIV Debug GUI

cd "$(dirname "$0")/.."

# Check if build exists
if [ ! -f "build_converter/converter/rive_convert_cli" ]; then
    echo "⚠️  Build not found. Building converter tools..."
    cmake -S . -B build_converter
    cmake --build build_converter --target rive_convert_cli import_test
fi

# Launch GUI
echo "🚀 Launching RIV Debug GUI..."
python3 tools/riv_debug_gui.py
