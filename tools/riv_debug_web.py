#!/usr/bin/env python3
"""
RIV Debug Web UI - Simple test/debug interface for RIV converter
Web-based UI that works on all macOS versions (no Tkinter required)
"""

import http.server
import socketserver
import json
import subprocess
import threading
import urllib.parse
import os
from pathlib import Path
from datetime import datetime

# Repo root
REPO_ROOT = Path(__file__).parent.parent.absolute()
PORT = 8765

class DebugRequestHandler(http.server.SimpleHTTPRequestHandler):
    """Custom request handler for RIV debug operations"""
    
    def do_GET(self):
        """Handle GET requests"""
        if self.path == '/' or self.path == '/index.html':
            self.send_html()
        elif self.path == '/api/check':
            self.send_json(self.check_dependencies())
        elif self.path.startswith('/api/list-rivs'):
            self.send_json(self.list_riv_files())
        elif self.path.startswith('/riv/'):
            self.serve_riv_file()
        else:
            super().do_GET()
    
    def do_POST(self):
        """Handle POST requests"""
        content_length = int(self.headers['Content-Length'])
        post_data = self.rfile.read(content_length)
        data = json.loads(post_data.decode('utf-8'))
        
        if self.path == '/api/extract':
            result = self.run_extract(data)
            self.send_json(result)
        elif self.path == '/api/convert':
            result = self.run_convert(data)
            self.send_json(result)
        elif self.path == '/api/roundtrip':
            result = self.run_roundtrip(data)
            self.send_json(result)
        elif self.path == '/api/analyze':
            result = self.run_analyze(data)
            self.send_json(result)
        elif self.path == '/api/import-test':
            result = self.run_import_test(data)
            self.send_json(result)
        elif self.path == '/api/compare':
            result = self.run_compare(data)
            self.send_json(result)
        elif self.path == '/api/stability':
            result = self.run_stability(data)
            self.send_json(result)
        elif self.path == '/api/json-to-riv':
            result = self.run_json_to_riv(data)
            self.send_json(result)
        else:
            self.send_response(404)
            self.end_headers()
    
    def send_json(self, data):
        """Send JSON response"""
        self.send_response(200)
        self.send_header('Content-type', 'application/json')
        self.end_headers()
        self.wfile.write(json.dumps(data).encode())
    
    def send_html(self):
        """Send main HTML page"""
        html = HTML_TEMPLATE
        self.send_response(200)
        self.send_header('Content-type', 'text/html')
        self.end_headers()
        self.wfile.write(html.encode())
    
    def check_dependencies(self):
        """Check if required tools exist"""
        extractor = REPO_ROOT / "build_converter/converter/universal_extractor"
        converter = REPO_ROOT / "build_converter/converter/rive_convert_cli"
        import_test = REPO_ROOT / "build_converter/converter/import_test"
        
        return {
            'extractor': extractor.exists(),
            'converter': converter.exists(),
            'import_test': import_test.exists(),
            'repo_root': str(REPO_ROOT)
        }
    
    def list_riv_files(self):
        """List available RIV files"""
        examples_dir = REPO_ROOT / "converter/exampleriv"
        output_dir = REPO_ROOT / "output/web_tests"
        files = []
        for source, d in [("output", output_dir), ("examples", examples_dir)]:
            if d.exists():
                for f in d.glob("*.riv"):
                    files.append({
                        'name': f.name,
                        'path': str(f),
                        'size': f.stat().st_size,
                        'source': source
                    })
        return {'files': files}
    
    def serve_riv_file(self):
        """Serve RIV file for player"""
        # Remove '/riv/' prefix and decode URL
        name = urllib.parse.unquote(self.path[5:])
        search_dirs = [REPO_ROOT / "output/web_tests", REPO_ROOT / "converter/exampleriv"]
        riv_path = None
        for d in search_dirs:
            candidate = d / name
            if candidate.exists():
                riv_path = candidate
                break
        if riv_path is None:
            self.send_response(404)
            self.end_headers()
            return
        try:
            self.send_response(200)
            self.send_header('Content-type', 'application/octet-stream')
            self.send_header('Access-Control-Allow-Origin', '*')
            self.end_headers()
            with open(riv_path, 'rb') as f:
                self.wfile.write(f.read())
        except Exception:
            self.send_response(500)
            self.end_headers()
    
    def run_command(self, cmd, cwd=None):
        """Run command and capture output"""
        try:
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                cwd=cwd or REPO_ROOT,
                timeout=60
            )
            return {
                'success': result.returncode == 0,
                'returncode': result.returncode,
                'stdout': result.stdout,
                'stderr': result.stderr
            }
        except subprocess.TimeoutExpired:
            return {
                'success': False,
                'error': 'Command timeout (60s)'
            }
        except Exception as e:
            return {
                'success': False,
                'error': str(e)
            }
    
    def run_extract(self, data):
        """Extract RIV to JSON"""
        input_riv = data.get('input_riv')
        output_dir = Path(data.get('output_dir', REPO_ROOT / "output/web_tests"))
        output_dir.mkdir(parents=True, exist_ok=True)
        
        input_path = Path(input_riv)
        output_json = output_dir / f"{input_path.stem}_extracted.json"
        
        extractor = REPO_ROOT / "build_converter/converter/universal_extractor"
        cmd = [str(extractor), str(input_path), str(output_json)]
        
        result = self.run_command(cmd)
        result['output_file'] = str(output_json)
        return result
    
    def run_convert(self, data):
        """Convert JSON to RIV"""
        input_riv = data.get('input_riv')
        output_dir = Path(data.get('output_dir', REPO_ROOT / "output/web_tests"))
        exact_mode = data.get('exact_mode', True)
        
        input_path = Path(input_riv)
        json_path = output_dir / f"{input_path.stem}_extracted.json"
        output_riv = output_dir / f"{input_path.stem}_roundtrip.riv"
        
        if not json_path.exists():
            return {
                'success': False,
                'error': 'JSON file not found. Run Extract first.'
            }
        
        converter = REPO_ROOT / "build_converter/converter/rive_convert_cli"
        cmd = [str(converter)]
        if exact_mode:
            cmd.append('--exact')
        cmd.extend([str(json_path), str(output_riv)])
        
        result = self.run_command(cmd)
        result['output_file'] = str(output_riv)
        return result
    
    def run_roundtrip(self, data):
        """Full round-trip test"""
        input_riv = data.get('input_riv')
        output_dir = Path(data.get('output_dir', REPO_ROOT / "output/web_tests"))
        exact_mode = data.get('exact_mode', True)
        output_dir.mkdir(parents=True, exist_ok=True)
        
        input_path = Path(input_riv)
        json_path = output_dir / f"{input_path.stem}_extracted.json"
        output_riv = output_dir / f"{input_path.stem}_roundtrip.riv"
        
        extractor = REPO_ROOT / "build_converter/converter/universal_extractor"
        converter = REPO_ROOT / "build_converter/converter/rive_convert_cli"
        import_test = REPO_ROOT / "build_converter/converter/import_test"
        
        steps = []
        
        # Step 1: Extract
        cmd1 = [str(extractor), str(input_path), str(json_path)]
        result1 = self.run_command(cmd1)
        steps.append({
            'name': 'Extract',
            'success': result1['success'],
            'output': result1.get('stdout', '') + result1.get('stderr', '')
        })
        
        if not result1['success']:
            return {'success': False, 'steps': steps}
        
        # Step 2: Convert
        cmd2 = [str(converter)]
        if exact_mode:
            cmd2.append('--exact')
        cmd2.extend([str(json_path), str(output_riv)])
        result2 = self.run_command(cmd2)
        steps.append({
            'name': 'Convert',
            'success': result2['success'],
            'output': result2.get('stdout', '') + result2.get('stderr', '')
        })
        
        if not result2['success']:
            return {'success': False, 'steps': steps}
        
        # Step 3: Import Test
        cmd3 = [str(import_test), str(output_riv)]
        result3 = self.run_command(cmd3)
        import_success = 'SUCCESS' in result3.get('stdout', '')
        steps.append({
            'name': 'Import Test',
            'success': import_success,
            'output': result3.get('stdout', '') + result3.get('stderr', '')
        })
        
        # Step 4: Binary Compare
        cmd4 = ['cmp', str(input_path), str(output_riv)]
        result4 = self.run_command(cmd4)
        byte_identical = result4['returncode'] == 0
        
        orig_size = input_path.stat().st_size
        rt_size = output_riv.stat().st_size
        diff = rt_size - orig_size
        
        steps.append({
            'name': 'Binary Compare',
            'success': byte_identical,
            'output': f"Original: {orig_size:,} bytes\nRound-trip: {rt_size:,} bytes\nDifference: {diff:+,} bytes\n" +
                     ("✅ Files are BYTE-IDENTICAL!" if byte_identical else "⚠️ Files differ")
        })
        
        return {
            'success': result1['success'] and result2['success'] and import_success,
            'byte_identical': byte_identical,
            'steps': steps
        }
    
    def run_analyze(self, data):
        """Analyze RIV structure"""
        input_riv = data.get('input_riv')
        analyzer = REPO_ROOT / "converter/analyze_riv.py"
        
        cmd = ['python3', str(analyzer), str(input_riv)]
        return self.run_command(cmd)
    
    def run_import_test(self, data):
        """Run import test"""
        input_riv = data.get('input_riv')
        import_test = REPO_ROOT / "build_converter/converter/import_test"
        
        cmd = [str(import_test), str(input_riv)]
        return self.run_command(cmd)
    
    def run_compare(self, data):
        """Binary comparison"""
        input_riv = data.get('input_riv')
        output_dir = Path(data.get('output_dir', REPO_ROOT / "output/web_tests"))
        
        input_path = Path(input_riv)
        output_riv = output_dir / f"{input_path.stem}_roundtrip.riv"
        
        if not output_riv.exists():
            return {
                'success': False,
                'error': 'Round-trip file not found. Run Full Round-Trip first.'
            }
        
        cmd = ['cmp', '-l', str(input_path), str(output_riv)]
        return self.run_command(cmd)
    
    def run_stability(self, data):
        """3-cycle stability test"""
        input_riv = data.get('input_riv')
        script = REPO_ROOT / "scripts/simple_stability_test.sh"
        
        if not script.exists():
            return {
                'success': False,
                'error': 'Stability test script not found'
            }
        
        cmd = [str(script), str(input_riv)]
        return self.run_command(cmd)
    
    def run_json_to_riv(self, data):
        """Convert JSON to RIV"""
        json_content = data.get('json_content', '')
        output_name = data.get('output_name', 'custom')
        exact_mode = data.get('exact_mode', False)
        
        if not json_content:
            return {'success': False, 'error': 'No JSON content provided'}
        
        # Save JSON to temp file
        output_dir = REPO_ROOT / "output/web_tests"
        output_dir.mkdir(parents=True, exist_ok=True)
        
        json_file = output_dir / f"{output_name}.json"
        riv_file = output_dir / f"{output_name}.riv"
        
        try:
            with open(json_file, 'w') as f:
                f.write(json_content)
        except Exception as e:
            return {'success': False, 'error': f'Failed to save JSON: {str(e)}'}
        
        # Convert to RIV
        converter = REPO_ROOT / "build_converter/converter/rive_convert_cli"
        if not converter.exists():
            return {'success': False, 'error': 'Converter not built'}
        
        cmd = [str(converter)]
        if exact_mode:
            cmd.append('--exact')
        cmd.extend([str(json_file), str(riv_file)])
        
        result = self.run_command(cmd)
        result['json_file'] = str(json_file)
        result['riv_file'] = str(riv_file)
        result['riv_name'] = riv_file.name
        
        return result

# HTML Template
HTML_TEMPLATE = """<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>RIV Debug Tool</title>
    <script src="https://unpkg.com/@rive-app/canvas@2.7.0"></script>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
            background: #f5f5f7;
            padding: 20px;
        }
        .container { max-width: 1200px; margin: 0 auto; }
        h1 {
            color: #1d1d1f;
            margin-bottom: 10px;
            font-size: 32px;
        }
        .subtitle {
            color: #86868b;
            margin-bottom: 30px;
            font-size: 14px;
        }
        .card {
            background: white;
            border-radius: 12px;
            padding: 20px;
            margin-bottom: 20px;
            box-shadow: 0 2px 8px rgba(0,0,0,0.1);
        }
        .card-title {
            font-size: 18px;
            font-weight: 600;
            margin-bottom: 15px;
            color: #1d1d1f;
        }
        .form-group {
            margin-bottom: 15px;
        }
        label {
            display: block;
            margin-bottom: 5px;
            color: #1d1d1f;
            font-size: 14px;
            font-weight: 500;
        }
        input[type="text"], select {
            width: 100%;
            padding: 10px;
            border: 1px solid #d2d2d7;
            border-radius: 8px;
            font-size: 14px;
        }
        input[type="checkbox"] {
            margin-right: 8px;
        }
        .button-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(150px, 1fr));
            gap: 10px;
            margin-top: 15px;
        }
        button {
            padding: 12px 20px;
            border: none;
            border-radius: 8px;
            font-size: 14px;
            font-weight: 600;
            cursor: pointer;
            transition: all 0.2s;
        }
        button:hover { transform: translateY(-1px); }
        button:active { transform: translateY(0); }
        .btn-primary {
            background: #0071e3;
            color: white;
        }
        .btn-primary:hover { background: #0077ED; }
        .btn-secondary {
            background: #f5f5f7;
            color: #1d1d1f;
        }
        .btn-secondary:hover { background: #e8e8ed; }
        .btn-success { background: #34c759; color: white; }
        .btn-danger { background: #ff3b30; color: white; }
        .btn-warning { background: #ff9500; color: white; }
        .console {
            background: #1d1d1f;
            color: #f5f5f7;
            padding: 15px;
            border-radius: 8px;
            font-family: 'Monaco', 'Courier New', monospace;
            font-size: 12px;
            height: 400px;
            overflow-y: auto;
            white-space: pre-wrap;
            word-wrap: break-word;
        }
        .console .header { color: #0a84ff; font-weight: bold; }
        .console .success { color: #30d158; }
        .console .error { color: #ff453a; }
        .console .warning { color: #ff9f0a; }
        .console .info { color: #98989d; }
        .status-bar {
            position: fixed;
            bottom: 0;
            left: 0;
            right: 0;
            background: white;
            padding: 12px 20px;
            box-shadow: 0 -2px 8px rgba(0,0,0,0.1);
            display: flex;
            justify-content: space-between;
            align-items: center;
        }
        .status-text {
            font-size: 14px;
            color: #1d1d1f;
        }
        .dependency-status {
            display: flex;
            gap: 10px;
            margin-bottom: 15px;
        }
        .badge {
            padding: 4px 8px;
            border-radius: 4px;
            font-size: 12px;
            font-weight: 600;
        }
        .badge-success { background: #d1f4e0; color: #0f6130; }
        .badge-error { background: #ffe5e5; color: #c41e3a; }
        .spinner {
            border: 2px solid #f3f3f3;
            border-top: 2px solid #0071e3;
            border-radius: 50%;
            width: 16px;
            height: 16px;
            animation: spin 1s linear infinite;
            display: inline-block;
            margin-left: 10px;
        }
        @keyframes spin {
            0% { transform: rotate(0deg); }
            100% { transform: rotate(360deg); }
        }
        #riveCanvas {
            width: 100%;
            max-width: 100%;
            height: 600px;
            background: #f0f0f0;
            border-radius: 8px;
            display: block;
            border: 1px solid #d2d2d7;
            image-rendering: -webkit-optimize-contrast;
            image-rendering: crisp-edges;
        }
        .player-controls {
            display: flex;
            gap: 10px;
            margin-top: 15px;
            align-items: center;
        }
        .artboard-select {
            flex: 1;
            padding: 8px;
            border: 1px solid #d2d2d7;
            border-radius: 6px;
        }
        /* JSON Editor */
        .json-editor {
            width: 100%;
            min-height: 220px;
            background: #0f0f12;
            color: #eaeaea;
            border: 1px solid #2a2a2e;
            border-radius: 8px;
            font-family: 'Monaco', 'Courier New', monospace;
            font-size: 12px;
            padding: 12px;
            outline: none;
            resize: vertical;
            line-height: 1.4;
        }
        .inline-row {
            display: grid;
            grid-template-columns: 1fr auto;
            gap: 10px;
            align-items: center;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>🎨 RIV Debug Tool</h1>
        <p class="subtitle">Web-based test and debug interface for RIV converter</p>
        
        <div class="card">
            <div class="card-title">📦 Dependencies</div>
            <div id="dependencies" class="dependency-status">
                <div class="badge badge-error">Checking...</div>
            </div>
        </div>
        
        <div class="card">
            <div class="card-title">📁 File Selection</div>
            <div class="form-group">
                <label>Input RIV File</label>
                <select id="inputRiv" onchange="loadRivePlayer()">
                    <option value="">Loading...</option>
                </select>
            </div>
            <div class="form-group">
                <label>Output Directory</label>
                <input type="text" id="outputDir" value="output/web_tests" placeholder="output/web_tests">
            </div>
            <div class="form-group">
                <label>
                    <input type="checkbox" id="exactMode" checked>
                    Use --exact mode (byte-perfect reconstruction)
                </label>
            </div>
        </div>
        

        <div class="card">
            <div class="card-title">🧾 JSON → RIV</div>
            <div class="form-group inline-row">
                <div>
                    <label>Output Name</label>
                    <input type="text" id="jsonOutputName" value="custom_bpm" placeholder="custom_bpm">
                </div>
                <div>
                    <label>&nbsp;</label>
                    <button class="btn-primary" onclick="runJsonToRiv()">📥 Convert JSON → RIV</button>
                </div>
            </div>
            <div class="form-group">
                <label>Select JSON File</label>
                <input type="file" id="jsonFile" accept=".json">
            </div>
            <div class="form-group">
                <label>JSON Content</label>
                <textarea id="jsonEditor" class="json-editor" placeholder='{"format":"universal","version":"1.0","artboards":[...]}'></textarea>
            </div>
            <div class="button-grid">
                <button class="btn-secondary" onclick="loadSampleJson()">📄 Load Sample</button>
                <button class="btn-secondary" onclick="beautifyJson()">🧹 Format JSON</button>
                <button class="btn-warning" onclick="clearJsonEditor()">♻️ Clear</button>
            </div>
        </div>

        
        <div class="card">
            <div class="card-title">🎬 Rive Viewer</div>
            <canvas id="riveCanvas"></canvas>
            <div class="player-controls">
                <button class="btn-primary" id="playPauseBtn" onclick="togglePlayPause()">▶️ Play</button>
                <button class="btn-secondary" onclick="resetAnimation()">🔄 Reset</button>
                <select id="stateMachineSelect" class="artboard-select" onchange="changeStateMachine()">
                    <option value="">Select animation/state machine...</option>
                </select>
            </div>
        </div>

        <div class="card">
            <div class="card-title">⚡ Actions</div>
            <div class="button-grid">
                <button class="btn-primary" onclick="runExtract()">📤 Extract</button>
                <button class="btn-primary" onclick="runConvert()">📥 Convert</button>
                <button class="btn-success" onclick="runRoundtrip()">🔄 Full Round-Trip</button>
                <button class="btn-secondary" onclick="runAnalyze()">🔍 Analyze RIV</button>
                <button class="btn-secondary" onclick="runImportTest()">✅ Import Test</button>
                <button class="btn-secondary" onclick="runCompare()">📊 Binary Compare</button>
                <button class="btn-warning" onclick="runStability()">🔁 3-Cycle Stability</button>
                <button class="btn-secondary" onclick="clearConsole()">🧹 Clear Console</button>
                <button class="btn-secondary" onclick="copyConsole()">📋 Copy Console</button>
            </div>
        </div>
        
        <div class="card">
            <div class="card-title">💻 Debug Console</div>
            <div id="console" class="console"></div>
        </div>
    </div>
    
    <div class="status-bar">
        <span id="status" class="status-text">Ready</span>
        <button class="btn-secondary" onclick="window.open('http://localhost:' + window.location.port + '/output/web_tests', '_blank')">📂 Open Output</button>
    </div>
    
    <script>
        let consoleEl = document.getElementById('console');
        
        function log(message, type = '') {
            const span = document.createElement('span');
            span.className = type;
            span.textContent = message + '\\n';
            consoleEl.appendChild(span);
            consoleEl.scrollTop = consoleEl.scrollHeight;
        }
        
        function logHeader(message) {
            log('\\n' + '='.repeat(60), 'header');
            log('[' + new Date().toLocaleTimeString() + '] ' + message, 'header');
            log('='.repeat(60), 'header');
        }
        
        function clearConsole() {
            consoleEl.innerHTML = '';
            setStatus('Console cleared');
        }
        
        function copyConsole() {
            const text = consoleEl.innerText;
            navigator.clipboard.writeText(text);
            setStatus('✅ Copied to clipboard');
        }
        
        function setStatus(text, spinner = false) {
            document.getElementById('status').innerHTML = text + (spinner ? '<span class="spinner"></span>' : '');
        }
        
        async function apiCall(endpoint, data = null) {
            const options = data ? {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(data)
            } : {};
            
            const response = await fetch(endpoint, options);
            return await response.json();
        }
        
        async function checkDependencies() {
            const result = await apiCall('/api/check');
            const deps = document.getElementById('dependencies');
            deps.innerHTML = '';
            
            const tools = [
                { name: 'Extractor', key: 'extractor' },
                { name: 'Converter', key: 'converter' },
                { name: 'Import Test', key: 'import_test' }
            ];
            
            tools.forEach(tool => {
                const badge = document.createElement('div');
                badge.className = 'badge ' + (result[tool.key] ? 'badge-success' : 'badge-error');
                badge.textContent = (result[tool.key] ? '✅ ' : '❌ ') + tool.name;
                deps.appendChild(badge);
            });
            
            if (!result.extractor || !result.converter || !result.import_test) {
                log('⚠️  Some dependencies missing. Build first:', 'warning');
                log('  cmake -S . -B build_converter', 'info');
                log('  cmake --build build_converter', 'info');
            }
        }
        
        async function loadRivFiles() {
            const result = await apiCall('/api/list-rivs');
            const select = document.getElementById('inputRiv');
            select.innerHTML = '';
            
            if (result.files.length === 0) {
                select.innerHTML = '<option value="">No RIV files found</option>';
                return;
            }
            
            result.files.forEach(file => {
                const option = document.createElement('option');
                option.value = file.path;
                option.textContent = file.name + ' (' + (file.size / 1024).toFixed(1) + ' KB)';
                select.appendChild(option);
            });
        }
        
        function getData() {
            return {
                input_riv: document.getElementById('inputRiv').value,
                output_dir: document.getElementById('outputDir').value,
                exact_mode: document.getElementById('exactMode').checked
            };
        }
        
        async function runExtract() {
            logHeader('Extract RIV → JSON');
            setStatus('Extracting...', true);
            
            const result = await apiCall('/api/extract', getData());
            
            if (result.success) {
                log(result.stdout || '', 'success');
                log('✅ Extract completed', 'success');
                log('Output: ' + result.output_file, 'info');
                setStatus('✅ Extract completed');
            } else {
                log(result.stderr || result.error || 'Failed', 'error');
                setStatus('❌ Extract failed');
            }
        }
        
        async function runConvert() {
            logHeader('Convert JSON → RIV');
            setStatus('Converting...', true);
            
            const result = await apiCall('/api/convert', getData());
            
            if (result.success) {
                log(result.stdout || '', 'success');
                log('✅ Convert completed', 'success');
                log('Output: ' + result.output_file, 'info');
                setStatus('✅ Convert completed');
            } else {
                log(result.stderr || result.error || 'Failed', 'error');
                setStatus('❌ Convert failed');
            }
        }
        
        async function runRoundtrip() {
            logHeader('Full Round-Trip Test');
            setStatus('Running round-trip...', true);
            
            const result = await apiCall('/api/roundtrip', getData());
            
            result.steps.forEach((step, i) => {
                log('\\n[' + (i + 1) + '/4] ' + step.name + '...', 'header');
                log(step.output, step.success ? 'success' : 'error');
                log(step.success ? '✅ ' + step.name + ' completed' : '❌ ' + step.name + ' failed', 
                    step.success ? 'success' : 'error');
            });
            
            if (result.success && result.byte_identical) {
                log('\\n🎉 Round-trip successful - files are BYTE-IDENTICAL!', 'success');
                setStatus('✅ Round-trip successful');
            } else if (result.success) {
                log('\\n⚠️  Round-trip completed but files differ', 'warning');
                setStatus('⚠️  Files differ');
            } else {
                log('\\n❌ Round-trip failed', 'error');
                setStatus('❌ Round-trip failed');
            }
        }
        
        async function runAnalyze() {
            logHeader('Analyze RIV Structure');
            setStatus('Analyzing...', true);
            
            const result = await apiCall('/api/analyze', getData());
            
            log(result.stdout || '', 'info');
            if (result.stderr) log(result.stderr, 'error');
            setStatus(result.success ? '✅ Analysis complete' : '❌ Analysis failed');
        }
        
        async function runImportTest() {
            logHeader('Import Test');
            setStatus('Testing import...', true);
            
            const result = await apiCall('/api/import-test', getData());
            
            log(result.stdout || '', result.success ? 'info' : 'error');
            if (result.stderr) log(result.stderr, 'error');
            setStatus(result.success ? '✅ Import test passed' : '❌ Import test failed');
        }
        
        async function runCompare() {
            logHeader('Binary Comparison');
            setStatus('Comparing...', true);
            
            const result = await apiCall('/api/compare', getData());
            
            if (result.success) {
                log('✅ Files are identical', 'success');
                setStatus('✅ Files identical');
            } else {
                log(result.stdout || result.error || 'Files differ', 'warning');
                setStatus('⚠️  Files differ');
            }
        }
        
        async function runStability() {
            logHeader('3-Cycle Stability Test');
            setStatus('Running stability test...', true);
            
            const result = await apiCall('/api/stability', getData());
            
            log(result.stdout || '', result.success ? 'info' : 'error');
            if (result.stderr) log(result.stderr, 'error');
            setStatus(result.success ? '✅ Stability test complete' : '❌ Stability test failed');
        }

        // JSON → RIV
        function loadSampleJson() {
            const sample = {
                format: 'universal',
                version: '1.0',
                artboards: [{
                    name: 'Sample', width: 200, height: 200,
                    objects: [
                        { typeKey: 1, localId: 0, properties: { name: 'Sample', width: 200, height: 200 } },
                        { typeKey: 7, localId: 1, parentId: 0, properties: { x: 50, y: 50, width: 100, height: 100 } },
                        { typeKey: 20, localId: 2, parentId: 1, properties: {} },
                        { typeKey: 18, localId: 3, parentId: 2, properties: { color: '#00A8E8' } }
                    ]
                }]
            };
            document.getElementById('jsonEditor').value = JSON.stringify(sample, null, 2);
            setStatus('Sample JSON loaded');
        }

        function beautifyJson() {
            const el = document.getElementById('jsonEditor');
            try {
                const parsed = JSON.parse(el.value);
                el.value = JSON.stringify(parsed, null, 2);
                setStatus('JSON formatted');
            } catch (e) {
                setStatus('❌ Invalid JSON: ' + e.message);
                log('Invalid JSON: ' + e.message, 'error');
            }
        }

        function clearJsonEditor() {
            document.getElementById('jsonEditor').value = '';
            setStatus('Editor cleared');
        }

        async function runJsonToRiv() {
            logHeader('JSON → RIV Conversion');
            setStatus('Converting JSON...', true);

            const fileInput = document.getElementById('jsonFile');
            let jsonText = '';
            let outputName = document.getElementById('jsonOutputName').value.trim();

            if (fileInput.files && fileInput.files[0]) {
                const file = fileInput.files[0];
                try {
                    jsonText = await file.text();
                    log('📄 Using file: ' + file.name, 'info');
                    if (!outputName) {
                        outputName = file.name.replace(/\.json$/i, '') || 'custom';
                        document.getElementById('jsonOutputName').value = outputName;
                    }
                } catch (e) {
                    setStatus('❌ Failed to read file');
                    log('Failed to read selected file: ' + e.message, 'error');
                    return;
                }
            } else {
                jsonText = document.getElementById('jsonEditor').value;
                if (!jsonText.trim()) {
                    setStatus('❌ No JSON content');
                    log('Select a JSON file or paste JSON first.', 'error');
                    return;
                }
                if (!outputName) outputName = 'custom';
            }

            // Parse and auto-detect exact mode
            let parsed = null;
            let isExactJson = false;
            try {
                parsed = JSON.parse(jsonText);
                isExactJson = parsed.__riv_exact__ === true;
            } catch (e) {
                log('JSON parse warning (converter will also validate): ' + e.message, 'warning');
            }
            const userExactPref = document.getElementById('exactMode').checked;
            const exact = userExactPref && isExactJson; // Only enable --exact if JSON declares __riv_exact__
            if (userExactPref && !isExactJson) {
                log('⚠️ --exact requested but JSON is not exact. Running in normal mode.', 'warning');
            }

            const result = await apiCall('/api/json-to-riv', {
                json_content: jsonText,
                output_name: outputName,
                exact_mode: exact
            });

            if (result.success) {
                log(result.stdout || '', 'success');
                log('✅ JSON converted to RIV', 'success');
                log('Output: ' + result.riv_file, 'info');
                setStatus('✅ JSON converted');
                // Refresh file list and auto-load viewer
                await loadRivFiles();
                const select = document.getElementById('inputRiv');
                // Prefer absolute path match
                select.value = result.riv_file;
                if (select.value !== result.riv_file) {
                    // Fallback: select by filename suffix
                    const name = result.riv_name;
                    for (const opt of select.options) {
                        if (opt.value.endsWith('/' + name)) { select.value = opt.value; break; }
                    }
                }
                loadRivePlayer();
            } else {
                log(result.stderr || result.error || 'Conversion failed', 'error');
                setStatus('❌ JSON → RIV failed');
            }
        }
        
        // Rive Player Variables
        let riveInstance = null;
        let isPlaying = false;
        let currentFile = null;
        
        function loadRivePlayer() {
            const select = document.getElementById('inputRiv');
            const filename = select.value;
            
            if (!filename) return;
            
            // Extract filename from path
            const rivName = filename.split('/').pop();
            const rivUrl = '/riv/' + rivName;
            
            // Clean up previous instance
            if (riveInstance) {
                riveInstance.cleanup();
                riveInstance = null;
            }
            
            try {
                log('🎬 Loading Rive file: ' + rivName, 'info');
                
                // First, load to inspect available content
                const tempInstance = new rive.Rive({
                    src: rivUrl,
                    canvas: document.getElementById('riveCanvas'),
                    autoplay: false,
                    layout: new rive.Layout({
                        fit: rive.Fit.Contain,
                        alignment: rive.Alignment.Center
                    }),
                    onLoad: () => {
                        const stateMachineNames = tempInstance.stateMachineNames || [];
                        const animationNames = tempInstance.animationNames || [];
                        
                        // Clean up temp instance
                        tempInstance.cleanup();
                        
                        // Setup high-DPI canvas
                        const canvas = document.getElementById('riveCanvas');
                        const ctx = canvas.getContext('2d');
                        const devicePixelRatio = window.devicePixelRatio || 1;
                        
                        // Set actual size in memory (scaled up for high DPI)
                        canvas.width = canvas.offsetWidth * devicePixelRatio;
                        canvas.height = canvas.offsetHeight * devicePixelRatio;
                        
                        // Scale the canvas back down using CSS
                        canvas.style.width = canvas.offsetWidth + 'px';
                        canvas.style.height = canvas.offsetHeight + 'px';
                        
                        // Scale the drawing context so everything draws at the correct size
                        ctx.scale(devicePixelRatio, devicePixelRatio);
                        
                        // Now load with proper defaults
                        const config = {
                            src: rivUrl,
                            canvas: canvas,
                            autoplay: true,
                            layout: new rive.Layout({
                                fit: rive.Fit.Contain,
                                alignment: rive.Alignment.Center
                            }),
                            onLoad: () => {
                                isPlaying = true;
                                document.getElementById('playPauseBtn').textContent = '⏸️ Pause';
                                currentFile = riveInstance;
                                
                                // Populate dropdown
                                const smSelect = document.getElementById('stateMachineSelect');
                                smSelect.innerHTML = '<option value="">Current: ' + 
                                    (stateMachineNames.length > 0 ? stateMachineNames[0] : 
                                     animationNames.length > 0 ? animationNames[0] : 'Default') + 
                                    '</option>';
                                
                                stateMachineNames.forEach(name => {
                                    const option = document.createElement('option');
                                    option.value = 'sm:' + name;
                                    option.textContent = '🎮 ' + name;
                                    smSelect.appendChild(option);
                                });
                                
                                animationNames.forEach(name => {
                                    const option = document.createElement('option');
                                    option.value = 'anim:' + name;
                                    option.textContent = '🎬 ' + name;
                                    smSelect.appendChild(option);
                                });
                                
                                log('✅ Rive file loaded and playing', 'success');
                                if (stateMachineNames.length > 0) {
                                    log('State Machines: ' + stateMachineNames.join(', '), 'info');
                                }
                                if (animationNames.length > 0) {
                                    log('Animations: ' + animationNames.join(', '), 'info');
                                }
                            },
                            onLoadError: (err) => {
                                log('❌ Failed to load Rive file: ' + err, 'error');
                            }
                        };
                        
                        // Load first state machine or first animation by default
                        if (stateMachineNames.length > 0) {
                            config.stateMachines = [stateMachineNames[0]];
                            log('▶️ Auto-loading state machine: ' + stateMachineNames[0], 'info');
                        } else if (animationNames.length > 0) {
                            config.animations = [animationNames[0]];
                            log('▶️ Auto-loading animation: ' + animationNames[0], 'info');
                        }
                        
                        riveInstance = new rive.Rive(config);
                    },
                    onLoadError: (err) => {
                        log('❌ Failed to load Rive file: ' + err, 'error');
                    }
                });
            } catch (err) {
                log('❌ Error loading Rive: ' + err.message, 'error');
            }
        }
        
        function togglePlayPause() {
            if (!riveInstance) return;
            
            if (isPlaying) {
                riveInstance.pause();
                document.getElementById('playPauseBtn').textContent = '▶️ Play';
                isPlaying = false;
                log('⏸️ Paused', 'info');
            } else {
                riveInstance.play();
                document.getElementById('playPauseBtn').textContent = '⏸️ Pause';
                isPlaying = true;
                log('▶️ Playing', 'info');
            }
        }
        
        function resetAnimation() {
            if (!riveInstance) return;
            riveInstance.reset();
            log('🔄 Animation reset', 'info');
        }
        
        function changeStateMachine() {
            const select = document.getElementById('stateMachineSelect');
            const value = select.value;
            
            if (!value || !riveInstance) return;
            
            const [type, name] = value.split(':');
            
            if (!name) {
                // Reset to default
                loadRivePlayer();
                return;
            }
            
            try {
                // Cleanup and reload with specific state machine or animation
                const filename = document.getElementById('inputRiv').value;
                const rivName = filename.split('/').pop();
                const rivUrl = '/riv/' + rivName;
                
                if (riveInstance) {
                    riveInstance.cleanup();
                }
                
                const config = {
                    src: rivUrl,
                    canvas: document.getElementById('riveCanvas'),
                    autoplay: true,
                    layout: new rive.Layout({
                        fit: rive.Fit.Contain,
                        alignment: rive.Alignment.Center
                    }),
                    onLoad: () => {
                        isPlaying = true;
                        document.getElementById('playPauseBtn').textContent = '⏸️ Pause';
                        log('✅ Switched to: ' + name, 'success');
                    }
                };
                
                if (type === 'sm') {
                    config.stateMachines = [name];
                    log('🎮 Loading state machine: ' + name, 'info');
                } else if (type === 'anim') {
                    config.animations = [name];
                    log('🎬 Loading animation: ' + name, 'info');
                }
                
                riveInstance = new rive.Rive(config);
                
            } catch (err) {
                log('❌ Failed to change: ' + err.message, 'error');
            }
        }
        
        // Initialize
        checkDependencies();
        loadRivFiles();
        log('🚀 RIV Debug Tool loaded', 'success');
        log('Select a RIV file to preview in the viewer, or paste JSON and convert.', 'info');
    </script>
</body>
</html>
"""

def main():
    print(f"🚀 Starting RIV Debug Web UI on http://localhost:{PORT}")
    print(f"📁 Repo root: {REPO_ROOT}")
    print(f"\n👉 Open http://localhost:{PORT} in your browser")
    print(f"Press Ctrl+C to stop\n")
    
    with socketserver.TCPServer(("", PORT), DebugRequestHandler) as httpd:
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            print("\n\n👋 Shutting down...")

if __name__ == "__main__":
    main()
