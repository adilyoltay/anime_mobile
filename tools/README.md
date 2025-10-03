# RIV Debug Tools

Simple test and debug interfaces for RIV converter with CLI integration.

![Platform](https://img.shields.io/badge/Platform-macOS-blue)
![Python](https://img.shields.io/badge/Python-3.8+-green)

## Available Tools

### 🌐 Web UI (Recommended)
**`riv_debug_web.py`** - Web-based interface that works on all macOS versions
- ✅ No Tkinter dependency issues
- 🚀 Runs in your browser
- 📱 Works on any macOS version

### 🖥️ Desktop UI (Legacy)
**`riv_debug_gui.py`** - Tkinter-based desktop app
- ⚠️ Requires macOS 13+ (Tkinter compatibility)
- May crash on older macOS versions

---

## Features

### 🎯 Core Operations
- **Extract**: RIV → JSON conversion
- **Convert**: JSON → RIV with `--exact` mode support
- **Full Round-Trip**: Extract → Convert → Compare → Import Test
- **Analyze**: Detailed RIV structure analysis
- **Import Test**: Validate RIV file loading
- **Binary Compare**: Byte-by-byte comparison
- **3-Cycle Stability**: Multi-cycle round-trip validation

### 🔧 Debug Features
- **Real-time Console**: Live command output with syntax highlighting
- **Copy to Clipboard**: Quick copy of debug information
- **Save to File**: Export debug logs with timestamps
- **Open Output**: Quick access to output directory in Finder
- **Dependency Check**: Automatic validation of required tools

### 🎨 UI Features
- Native macOS look and feel
- Color-coded console output:
  - 🔵 **Headers**: Blue, bold
  - 🟢 **Success**: Green
  - 🔴 **Errors**: Red
  - 🟠 **Warnings**: Orange
  - ⚫ **Info**: Gray
- Thread-safe operations (non-blocking UI)
- File pickers for easy file selection

## Requirements

- **Python 3.8+** (included with macOS)
- **Tkinter** (included with Python on macOS)
- **Built converter tools**:
  - `build_converter/converter/universal_extractor`
  - `build_converter/converter/rive_convert_cli`
  - `build_converter/converter/import_test`

## Quick Start

### 🌐 Web UI (Recommended)

```bash
# 1. Build converter tools (if not already built)
cmake -S . -B build_converter
cmake --build build_converter --target rive_convert_cli import_test

# 2. Start web server
python3 tools/riv_debug_web.py

# 3. Open in browser
# Navigate to: http://localhost:8765
```

### 🖥️ Desktop UI (Legacy - may crash on older macOS)

```bash
# Only use if you have macOS 13+
python3 tools/riv_debug_gui.py
```

## Installation

### 1. Build the Converter Tools

```bash
# From repo root
cmake -S . -B build_converter
cmake --build build_converter --target rive_convert_cli import_test
```

### 2. Launch Your Preferred Interface

#### Web UI (Recommended)
```bash
python3 tools/riv_debug_web.py
# Open http://localhost:8765 in your browser
```

#### Desktop UI (Legacy)
```bash
python3 tools/riv_debug_gui.py
```

## Usage

### Basic Workflow

1. **Select Input RIV File**
   - Click "Browse..." next to "Input RIV"
   - Select a `.riv` file (e.g., `converter/exampleriv/bee_baby.riv`)

2. **Select Output Directory**
   - Click "Browse..." next to "Output Dir"
   - Or use default: `output/gui_tests/`

3. **Choose Operation**
   - **Extract**: Convert RIV to JSON
   - **Convert**: Convert JSON back to RIV
   - **Full Round-Trip**: Complete pipeline with validation

4. **View Results**
   - Console shows real-time output
   - Status bar shows operation status
   - Copy or save console output for debugging

### Operation Details

#### Extract
- Converts RIV binary to human-readable JSON
- Preserves all properties with type information
- Output: `{filename}_extracted.json`

#### Convert
- Converts JSON back to RIV binary
- Supports `--exact` mode for byte-perfect reconstruction
- Output: `{filename}_roundtrip.riv`

#### Full Round-Trip
Runs complete validation pipeline:
1. Extract RIV → JSON
2. Convert JSON → RIV
3. Import test (validate loading)
4. Binary compare (byte-by-byte diff)

Shows:
- File sizes (original vs round-trip)
- Byte difference
- Import status
- Binary match result

#### Analyze RIV
- Shows header information
- Lists all objects with types
- Displays property keys
- Shows ToC (Table of Contents)

#### Import Test
- Tests file loading in Rive runtime
- Shows object count
- Lists artboards and state machines
- Detects NULL objects

#### 3-Cycle Stability
- Extracts → Converts 3 times
- Compares SHA-256 hashes
- Validates convergence
- Output: Stability report

### Console Features

#### Copy to Clipboard
- Click "Copy Console to Clipboard"
- Paste anywhere (Command+V)
- Great for sharing debug info

#### Save to File
- Click "Save Console to File"
- Choose location and filename
- Default: `debug_log_YYYYMMDD_HHMMSS.txt`

#### Clear Console
- Click "Clear Console"
- Clears all output
- Keeps dependency check results

## Examples

### Example 1: Test Exact Round-Trip

```
1. Input RIV: converter/exampleriv/bee_baby.riv
2. Output Dir: output/gui_tests
3. Check "Use --exact mode"
4. Click "Full Round-Trip"

Expected Output:
✅ Extract completed
✅ Convert completed
✅ Import test passed
✅ Files are BYTE-IDENTICAL!
```

### Example 2: Analyze Structure

```
1. Input RIV: converter/exampleriv/test.riv
2. Click "Analyze RIV"

Expected Output:
- Version: 7.0
- File ID: ...
- Header keys: 22
- Objects: 827
- ToC listing
```

### Example 3: Stability Test

```
1. Input RIV: converter/exampleriv/bee_baby.riv
2. Click "3-Cycle Stability"

Expected Output:
- C2, C4, C6 file sizes
- SHA-256 hashes (should be identical)
- Verdict: FULLY STABLE
```

## Troubleshooting

### Missing Dependencies

If you see errors like:
```
❌ Missing: universal_extractor
❌ Missing: rive_convert_cli
```

**Solution**: Build the converter tools
```bash
cmake -S . -B build_converter
cmake --build build_converter --target rive_convert_cli import_test
```

### JSON File Not Found

When clicking "Convert" without running "Extract" first:
```
❌ JSON file not found. Run Extract first.
```

**Solution**: Run "Extract" operation first, or use "Full Round-Trip"

### Permission Denied

If script is not executable:
```bash
chmod +x tools/riv_debug_gui.py
```

### Python Not Found

macOS should have Python 3 pre-installed. Verify:
```bash
python3 --version
```

If missing, install from [python.org](https://www.python.org/downloads/)

## Tips

1. **Use Full Round-Trip** for comprehensive testing
2. **Keep console output** - copy before running new operations
3. **Check status bar** for quick operation results
4. **Open output dir** to see generated files
5. **Use --exact mode** for byte-perfect reconstruction

## Keyboard Shortcuts

- `Command+C`: Copy (when console is focused)
- `Command+A`: Select all (in console)
- `Command+Q`: Quit application

## Architecture

```
┌─────────────────────────────────────┐
│      RIV Debug GUI (Tkinter)        │
├─────────────────────────────────────┤
│  File Selection | Options | Actions │
├─────────────────────────────────────┤
│         Debug Console               │
│  (ScrolledText with color tags)     │
├─────────────────────────────────────┤
│       Copy | Save | Status          │
└─────────────────────────────────────┘
           │
           ├─> universal_extractor (C++)
           ├─> rive_convert_cli (C++)
           ├─> import_test (C++)
           ├─> analyze_riv.py (Python)
           └─> Shell scripts
```

## Development

### Adding New Operations

1. Add button in `setup_ui()`:
```python
ttk.Button(actions_frame, text="My Op", 
          command=self.run_my_op).grid(...)
```

2. Implement handler:
```python
def run_my_op(self):
    if not self.validate_input():
        return
    cmd = ["/path/to/tool", "arg1", "arg2"]
    self.run_command(cmd, "My Operation")
```

### Console Logging

```python
self.log_header("Section Title")     # Blue, bold
self.log_success("✅ Success")       # Green
self.log_error("❌ Error")           # Red
self.log_warning("⚠️  Warning")      # Orange
self.log_info("Info message")        # Gray
self.log_output("Raw output")        # Default
```

## License

Part of anime_mobile project - see main LICENSE file.

## Support

For issues or feature requests, see main repo documentation.
