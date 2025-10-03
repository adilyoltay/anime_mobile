#!/usr/bin/env python3
"""
RIV Debug GUI - Simple test/debug interface for RIV converter
Supports extract, convert, compare, and analyze operations
"""

import tkinter as tk
from tkinter import ttk, filedialog, messagebox, scrolledtext
import subprocess
import threading
import os
import sys
from pathlib import Path
from datetime import datetime

class RivDebugGUI:
    def __init__(self, root):
        self.root = root
        self.root.title("RIV Debug Tool")
        self.root.geometry("1000x700")
        
        # Set repo root
        self.repo_root = Path(__file__).parent.parent.absolute()
        self.extractor = self.repo_root / "build_converter/converter/universal_extractor"
        self.converter = self.repo_root / "build_converter/converter/rive_convert_cli"
        self.import_test = self.repo_root / "build_converter/converter/import_test"
        self.analyzer = self.repo_root / "converter/analyze_riv.py"
        
        # Variables
        self.input_riv = tk.StringVar()
        self.output_dir = tk.StringVar(value=str(self.repo_root / "output/gui_tests"))
        self.exact_mode = tk.BooleanVar(value=True)
        
        self.setup_ui()
        self.check_dependencies()
        
    def setup_ui(self):
        """Setup the UI layout"""
        # Main container with padding
        main_frame = ttk.Frame(self.root, padding="10")
        main_frame.grid(row=0, column=0, sticky=(tk.W, tk.E, tk.N, tk.S))
        
        # Configure grid weights
        self.root.columnconfigure(0, weight=1)
        self.root.rowconfigure(0, weight=1)
        main_frame.columnconfigure(1, weight=1)
        main_frame.rowconfigure(4, weight=1)
        
        # === File Selection Section ===
        file_frame = ttk.LabelFrame(main_frame, text="File Selection", padding="10")
        file_frame.grid(row=0, column=0, columnspan=2, sticky=(tk.W, tk.E), pady=(0, 10))
        file_frame.columnconfigure(1, weight=1)
        
        # Input RIV
        ttk.Label(file_frame, text="Input RIV:").grid(row=0, column=0, sticky=tk.W, pady=5)
        ttk.Entry(file_frame, textvariable=self.input_riv).grid(row=0, column=1, sticky=(tk.W, tk.E), padx=5)
        ttk.Button(file_frame, text="Browse...", command=self.browse_input).grid(row=0, column=2)
        
        # Output Directory
        ttk.Label(file_frame, text="Output Dir:").grid(row=1, column=0, sticky=tk.W, pady=5)
        ttk.Entry(file_frame, textvariable=self.output_dir).grid(row=1, column=1, sticky=(tk.W, tk.E), padx=5)
        ttk.Button(file_frame, text="Browse...", command=self.browse_output).grid(row=1, column=2)
        
        # === Options Section ===
        options_frame = ttk.LabelFrame(main_frame, text="Options", padding="10")
        options_frame.grid(row=1, column=0, columnspan=2, sticky=(tk.W, tk.E), pady=(0, 10))
        
        ttk.Checkbutton(options_frame, text="Use --exact mode (byte-perfect reconstruction)", 
                       variable=self.exact_mode).grid(row=0, column=0, sticky=tk.W)
        
        # === Actions Section ===
        actions_frame = ttk.LabelFrame(main_frame, text="Actions", padding="10")
        actions_frame.grid(row=2, column=0, columnspan=2, sticky=(tk.W, tk.E), pady=(0, 10))
        
        btn_width = 15
        
        # Row 1
        ttk.Button(actions_frame, text="Extract", width=btn_width, 
                  command=self.run_extract).grid(row=0, column=0, padx=5, pady=5)
        ttk.Button(actions_frame, text="Convert", width=btn_width,
                  command=self.run_convert).grid(row=0, column=1, padx=5, pady=5)
        ttk.Button(actions_frame, text="Full Round-Trip", width=btn_width,
                  command=self.run_roundtrip).grid(row=0, column=2, padx=5, pady=5)
        
        # Row 2
        ttk.Button(actions_frame, text="Analyze RIV", width=btn_width,
                  command=self.run_analyze).grid(row=1, column=0, padx=5, pady=5)
        ttk.Button(actions_frame, text="Import Test", width=btn_width,
                  command=self.run_import_test).grid(row=1, column=1, padx=5, pady=5)
        ttk.Button(actions_frame, text="Binary Compare", width=btn_width,
                  command=self.run_compare).grid(row=1, column=2, padx=5, pady=5)
        
        # Row 3
        ttk.Button(actions_frame, text="3-Cycle Stability", width=btn_width,
                  command=self.run_stability).grid(row=2, column=0, padx=5, pady=5)
        ttk.Button(actions_frame, text="Clear Console", width=btn_width,
                  command=self.clear_console).grid(row=2, column=1, padx=5, pady=5)
        ttk.Button(actions_frame, text="Open Output Dir", width=btn_width,
                  command=self.open_output_dir).grid(row=2, column=2, padx=5, pady=5)
        
        # === Console Section ===
        console_frame = ttk.LabelFrame(main_frame, text="Debug Console", padding="10")
        console_frame.grid(row=4, column=0, columnspan=2, sticky=(tk.W, tk.E, tk.N, tk.S), pady=(0, 10))
        console_frame.columnconfigure(0, weight=1)
        console_frame.rowconfigure(0, weight=1)
        
        # Console text widget
        self.console = scrolledtext.ScrolledText(console_frame, wrap=tk.WORD, 
                                                 height=20, font=("Monaco", 11))
        self.console.grid(row=0, column=0, sticky=(tk.W, tk.E, tk.N, tk.S))
        
        # Configure console tags for colors
        self.console.tag_config("header", foreground="#0066CC", font=("Monaco", 11, "bold"))
        self.console.tag_config("success", foreground="#008800")
        self.console.tag_config("error", foreground="#CC0000")
        self.console.tag_config("warning", foreground="#FF8800")
        self.console.tag_config("info", foreground="#666666")
        
        # === Bottom Buttons ===
        bottom_frame = ttk.Frame(main_frame)
        bottom_frame.grid(row=5, column=0, columnspan=2, sticky=(tk.W, tk.E))
        
        ttk.Button(bottom_frame, text="Copy Console to Clipboard", 
                  command=self.copy_to_clipboard).pack(side=tk.LEFT, padx=5)
        ttk.Button(bottom_frame, text="Save Console to File",
                  command=self.save_console).pack(side=tk.LEFT, padx=5)
        
        # Status bar
        self.status_var = tk.StringVar(value="Ready")
        status_bar = ttk.Label(self.root, textvariable=self.status_var, relief=tk.SUNKEN)
        status_bar.grid(row=1, column=0, sticky=(tk.W, tk.E))
        
    def check_dependencies(self):
        """Check if required binaries exist"""
        self.log_header("Checking Dependencies")
        
        missing = []
        if not self.extractor.exists():
            missing.append(str(self.extractor))
            self.log_error(f"❌ Missing: {self.extractor.name}")
        else:
            self.log_success(f"✅ Found: {self.extractor.name}")
            
        if not self.converter.exists():
            missing.append(str(self.converter))
            self.log_error(f"❌ Missing: {self.converter.name}")
        else:
            self.log_success(f"✅ Found: {self.converter.name}")
            
        if not self.import_test.exists():
            missing.append(str(self.import_test))
            self.log_error(f"❌ Missing: {self.import_test.name}")
        else:
            self.log_success(f"✅ Found: {self.import_test.name}")
            
        if missing:
            self.log_warning("\n⚠️  Some tools are missing. Please build first:")
            self.log_info("  cmake -S . -B build_converter")
            self.log_info("  cmake --build build_converter --target rive_convert_cli import_test")
        else:
            self.log_success("\n✅ All dependencies found!\n")
    
    def browse_input(self):
        """Browse for input RIV file"""
        filename = filedialog.askopenfilename(
            title="Select Input RIV File",
            filetypes=[("RIV Files", "*.riv"), ("All Files", "*.*")],
            initialdir=self.repo_root / "converter/exampleriv"
        )
        if filename:
            self.input_riv.set(filename)
            self.log_info(f"Selected input: {Path(filename).name}")
    
    def browse_output(self):
        """Browse for output directory"""
        dirname = filedialog.askdirectory(
            title="Select Output Directory",
            initialdir=self.output_dir.get()
        )
        if dirname:
            self.output_dir.set(dirname)
            self.log_info(f"Output directory: {dirname}")
    
    def clear_console(self):
        """Clear the console"""
        self.console.delete(1.0, tk.END)
        self.status_var.set("Console cleared")
    
    def copy_to_clipboard(self):
        """Copy console content to clipboard"""
        content = self.console.get(1.0, tk.END)
        self.root.clipboard_clear()
        self.root.clipboard_append(content)
        self.status_var.set("Console copied to clipboard")
        self.log_success("✅ Copied to clipboard")
    
    def save_console(self):
        """Save console content to file"""
        filename = filedialog.asksaveasfilename(
            title="Save Console Output",
            defaultextension=".txt",
            filetypes=[("Text Files", "*.txt"), ("All Files", "*.*")],
            initialfile=f"debug_log_{datetime.now().strftime('%Y%m%d_%H%M%S')}.txt"
        )
        if filename:
            content = self.console.get(1.0, tk.END)
            Path(filename).write_text(content)
            self.log_success(f"✅ Saved to: {filename}")
    
    def open_output_dir(self):
        """Open output directory in Finder"""
        output_path = Path(self.output_dir.get())
        output_path.mkdir(parents=True, exist_ok=True)
        subprocess.run(["open", str(output_path)])
        self.log_info(f"Opened: {output_path}")
    
    def log_header(self, message):
        """Log header message"""
        timestamp = datetime.now().strftime("%H:%M:%S")
        self.console.insert(tk.END, f"\n{'='*60}\n", "header")
        self.console.insert(tk.END, f"[{timestamp}] {message}\n", "header")
        self.console.insert(tk.END, f"{'='*60}\n", "header")
        self.console.see(tk.END)
        self.root.update()
    
    def log_success(self, message):
        """Log success message"""
        self.console.insert(tk.END, f"{message}\n", "success")
        self.console.see(tk.END)
        self.root.update()
    
    def log_error(self, message):
        """Log error message"""
        self.console.insert(tk.END, f"{message}\n", "error")
        self.console.see(tk.END)
        self.root.update()
    
    def log_warning(self, message):
        """Log warning message"""
        self.console.insert(tk.END, f"{message}\n", "warning")
        self.console.see(tk.END)
        self.root.update()
    
    def log_info(self, message):
        """Log info message"""
        self.console.insert(tk.END, f"{message}\n", "info")
        self.console.see(tk.END)
        self.root.update()
    
    def log_output(self, text):
        """Log command output"""
        self.console.insert(tk.END, f"{text}\n")
        self.console.see(tk.END)
        self.root.update()
    
    def validate_input(self):
        """Validate input file exists"""
        input_path = Path(self.input_riv.get())
        if not input_path.exists():
            messagebox.showerror("Error", "Please select a valid input RIV file")
            return False
        return True
    
    def run_command(self, cmd, title):
        """Run a command and show output"""
        def execute():
            self.log_header(title)
            self.log_info(f"Command: {' '.join(str(c) for c in cmd)}")
            self.log_info("")
            
            try:
                # Create output directory
                Path(self.output_dir.get()).mkdir(parents=True, exist_ok=True)
                
                # Run command
                result = subprocess.run(
                    cmd,
                    capture_output=True,
                    text=True,
                    cwd=self.repo_root
                )
                
                # Show output
                if result.stdout:
                    self.log_output(result.stdout)
                if result.stderr:
                    self.log_error(result.stderr)
                
                # Show result
                if result.returncode == 0:
                    self.log_success(f"\n✅ Success (exit code: 0)")
                    self.status_var.set(f"✅ {title} completed")
                else:
                    self.log_error(f"\n❌ Failed (exit code: {result.returncode})")
                    self.status_var.set(f"❌ {title} failed")
                    
            except Exception as e:
                self.log_error(f"❌ Exception: {str(e)}")
                self.status_var.set(f"❌ Error: {str(e)}")
        
        # Run in thread to avoid blocking UI
        threading.Thread(target=execute, daemon=True).start()
    
    def run_extract(self):
        """Extract RIV to JSON"""
        if not self.validate_input():
            return
        
        input_path = Path(self.input_riv.get())
        output_path = Path(self.output_dir.get()) / f"{input_path.stem}_extracted.json"
        
        cmd = [str(self.extractor), str(input_path), str(output_path)]
        self.run_command(cmd, "Extract RIV → JSON")
    
    def run_convert(self):
        """Convert JSON to RIV"""
        if not self.validate_input():
            return
        
        input_path = Path(self.input_riv.get())
        json_path = Path(self.output_dir.get()) / f"{input_path.stem}_extracted.json"
        output_path = Path(self.output_dir.get()) / f"{input_path.stem}_roundtrip.riv"
        
        if not json_path.exists():
            self.log_error("❌ JSON file not found. Run Extract first.")
            return
        
        cmd = [str(self.converter)]
        if self.exact_mode.get():
            cmd.append("--exact")
        cmd.extend([str(json_path), str(output_path)])
        
        self.run_command(cmd, "Convert JSON → RIV")
    
    def run_roundtrip(self):
        """Full round-trip: Extract → Convert → Compare"""
        if not self.validate_input():
            return
        
        input_path = Path(self.input_riv.get())
        json_path = Path(self.output_dir.get()) / f"{input_path.stem}_extracted.json"
        output_path = Path(self.output_dir.get()) / f"{input_path.stem}_roundtrip.riv"
        
        def execute():
            # Step 1: Extract
            self.log_header("Step 1/4: Extract")
            cmd1 = [str(self.extractor), str(input_path), str(json_path)]
            self.log_info(f"Command: {' '.join(str(c) for c in cmd1)}")
            result1 = subprocess.run(cmd1, capture_output=True, text=True, cwd=self.repo_root)
            
            if result1.stdout:
                self.log_output(result1.stdout)
            if result1.returncode != 0:
                self.log_error(f"❌ Extract failed: {result1.stderr}")
                return
            self.log_success("✅ Extract completed")
            
            # Step 2: Convert
            self.log_header("Step 2/4: Convert")
            cmd2 = [str(self.converter)]
            if self.exact_mode.get():
                cmd2.append("--exact")
            cmd2.extend([str(json_path), str(output_path)])
            self.log_info(f"Command: {' '.join(str(c) for c in cmd2)}")
            result2 = subprocess.run(cmd2, capture_output=True, text=True, cwd=self.repo_root)
            
            if result2.stdout:
                self.log_output(result2.stdout)
            if result2.returncode != 0:
                self.log_error(f"❌ Convert failed: {result2.stderr}")
                return
            self.log_success("✅ Convert completed")
            
            # Step 3: Import Test
            self.log_header("Step 3/4: Import Test")
            cmd3 = [str(self.import_test), str(output_path)]
            self.log_info(f"Command: {' '.join(str(c) for c in cmd3)}")
            result3 = subprocess.run(cmd3, capture_output=True, text=True, cwd=self.repo_root)
            
            if result3.stdout:
                self.log_output(result3.stdout)
            if "SUCCESS" in result3.stdout:
                self.log_success("✅ Import test passed")
            else:
                self.log_error("❌ Import test failed")
            
            # Step 4: Binary Compare
            self.log_header("Step 4/4: Binary Compare")
            cmd4 = ["cmp", str(input_path), str(output_path)]
            self.log_info(f"Command: {' '.join(cmd4)}")
            result4 = subprocess.run(cmd4, capture_output=True, text=True)
            
            if result4.returncode == 0:
                self.log_success("✅ Files are BYTE-IDENTICAL!")
                
                # Show file sizes
                orig_size = input_path.stat().st_size
                rt_size = output_path.stat().st_size
                self.log_info(f"\nOriginal:   {orig_size:,} bytes")
                self.log_info(f"Round-trip: {rt_size:,} bytes")
                self.log_info(f"Difference: {rt_size - orig_size:+,} bytes")
                
                self.status_var.set("✅ Round-trip successful - files match!")
            else:
                self.log_warning("⚠️  Files differ")
                if result4.stdout:
                    self.log_output(result4.stdout)
                self.status_var.set("⚠️  Round-trip completed but files differ")
        
        threading.Thread(target=execute, daemon=True).start()
    
    def run_analyze(self):
        """Analyze RIV file structure"""
        if not self.validate_input():
            return
        
        input_path = Path(self.input_riv.get())
        cmd = ["python3", str(self.analyzer), str(input_path)]
        self.run_command(cmd, "Analyze RIV Structure")
    
    def run_import_test(self):
        """Run import test on RIV"""
        if not self.validate_input():
            return
        
        input_path = Path(self.input_riv.get())
        cmd = [str(self.import_test), str(input_path)]
        self.run_command(cmd, "Import Test")
    
    def run_compare(self):
        """Compare original and round-trip RIV"""
        if not self.validate_input():
            return
        
        input_path = Path(self.input_riv.get())
        output_path = Path(self.output_dir.get()) / f"{input_path.stem}_roundtrip.riv"
        
        if not output_path.exists():
            self.log_error("❌ Round-trip file not found. Run Full Round-Trip first.")
            return
        
        cmd = ["cmp", "-l", str(input_path), str(output_path)]
        self.run_command(cmd, "Binary Comparison")
    
    def run_stability(self):
        """Run 3-cycle stability test"""
        if not self.validate_input():
            return
        
        input_path = Path(self.input_riv.get())
        script = self.repo_root / "scripts/simple_stability_test.sh"
        
        if not script.exists():
            self.log_error(f"❌ Script not found: {script}")
            return
        
        cmd = [str(script), str(input_path)]
        self.run_command(cmd, "3-Cycle Stability Test")

def main():
    root = tk.Tk()
    app = RivDebugGUI(root)
    root.mainloop()

if __name__ == "__main__":
    main()
