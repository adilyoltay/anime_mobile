#!/usr/bin/env python3
"""
Round-trip Growth Tracker
Tracks size growth and regression metrics for round-trip conversions
"""

import sys
import json
from pathlib import Path
from typing import Dict, Tuple
import subprocess

# Growth thresholds
GROWTH_WARN_THRESHOLD = 0.05  # 5%
GROWTH_FAIL_THRESHOLD = 0.10  # 10%

class Colors:
    GREEN = '\033[92m'
    RED = '\033[91m'
    YELLOW = '\033[93m'
    BLUE = '\033[94m'
    CYAN = '\033[96m'
    RESET = '\033[0m'
    BOLD = '\033[1m'

def get_riv_info(riv_path: Path) -> Dict:
    """Get detailed RIV file information using analyzer."""
    try:
        # Use the analyzer to get object count
        result = subprocess.run(
            ['python3', 'converter/analyze_riv.py', str(riv_path), '--json'],
            capture_output=True,
            text=True,
            check=True
        )
        data = json.loads(result.stdout)
        
        return {
            'size': riv_path.stat().st_size,
            'objects': len(data.get('objects', [])),
            'version': data.get('version', '0.0'),
            'headerKeys': len(data.get('headerKeys', []))
        }
    except Exception as e:
        # Fallback to just size
        return {
            'size': riv_path.stat().st_size,
            'objects': 0,
            'version': 'unknown',
            'headerKeys': 0,
            'error': str(e)
        }

def calculate_metrics(original: Dict, roundtrip: Dict) -> Dict:
    """Calculate growth metrics."""
    metrics = {}
    
    # Size metrics
    size_diff = roundtrip['size'] - original['size']
    size_growth = (size_diff / original['size']) if original['size'] > 0 else 0
    
    metrics['size_original'] = original['size']
    metrics['size_roundtrip'] = roundtrip['size']
    metrics['size_diff'] = size_diff
    metrics['size_growth_pct'] = size_growth * 100
    
    # Object count metrics
    obj_diff = roundtrip['objects'] - original['objects']
    obj_growth = (obj_diff / original['objects']) if original['objects'] > 0 else 0
    
    metrics['objects_original'] = original['objects']
    metrics['objects_roundtrip'] = roundtrip['objects']
    metrics['objects_diff'] = obj_diff
    metrics['objects_growth_pct'] = obj_growth * 100
    
    # Header keys
    metrics['header_keys_original'] = original['headerKeys']
    metrics['header_keys_roundtrip'] = roundtrip['headerKeys']
    metrics['header_keys_diff'] = roundtrip['headerKeys'] - original['headerKeys']
    
    # Pass/Fail determination
    if size_growth > GROWTH_FAIL_THRESHOLD:
        metrics['status'] = 'FAIL'
        metrics['message'] = f'Size growth {size_growth*100:.1f}% exceeds fail threshold ({GROWTH_FAIL_THRESHOLD*100}%)'
    elif size_growth > GROWTH_WARN_THRESHOLD:
        metrics['status'] = 'WARN'
        metrics['message'] = f'Size growth {size_growth*100:.1f}% exceeds warning threshold ({GROWTH_WARN_THRESHOLD*100}%)'
    else:
        metrics['status'] = 'PASS'
        metrics['message'] = f'Size growth {size_growth*100:.1f}% within acceptable range'
    
    return metrics

def print_report(original_path: Path, roundtrip_path: Path, metrics: Dict):
    """Print formatted growth report."""
    c = Colors
    
    print(f"\n{c.BOLD}{'='*70}{c.RESET}")
    print(f"{c.BOLD}Round-trip Growth Analysis{c.RESET}")
    print(f"{c.BOLD}{'='*70}{c.RESET}\n")
    
    # Files
    print(f"{c.BLUE}Original:{c.RESET}   {original_path}")
    print(f"{c.CYAN}Round-trip:{c.RESET} {roundtrip_path}\n")
    
    # Size metrics
    print(f"{c.BOLD}File Size:{c.RESET}")
    size_orig = metrics['size_original']
    size_rt = metrics['size_roundtrip']
    size_diff = metrics['size_diff']
    size_growth = metrics['size_growth_pct']
    
    # Color code based on growth
    if size_growth > GROWTH_FAIL_THRESHOLD * 100:
        growth_color = c.RED
        status_symbol = "❌"
    elif size_growth > GROWTH_WARN_THRESHOLD * 100:
        growth_color = c.YELLOW
        status_symbol = "⚠️"
    else:
        growth_color = c.GREEN
        status_symbol = "✅"
    
    print(f"  Original:   {size_orig:,} bytes")
    print(f"  Round-trip: {size_rt:,} bytes")
    print(f"  Difference: {growth_color}{size_diff:+,} bytes ({size_growth:+.2f}%){c.RESET}")
    print(f"  Status:     {status_symbol} {metrics['status']}")
    
    # Object count
    print(f"\n{c.BOLD}Object Count:{c.RESET}")
    print(f"  Original:   {metrics['objects_original']:,}")
    print(f"  Round-trip: {metrics['objects_roundtrip']:,}")
    obj_diff = metrics['objects_diff']
    obj_growth = metrics['objects_growth_pct']
    
    if obj_diff == 0:
        print(f"  Difference: {c.GREEN}No change{c.RESET}")
    else:
        diff_color = c.YELLOW if abs(obj_growth) > 5 else c.CYAN
        print(f"  Difference: {diff_color}{obj_diff:+,} objects ({obj_growth:+.2f}%){c.RESET}")
    
    # Header keys
    print(f"\n{c.BOLD}Header Keys:{c.RESET}")
    print(f"  Original:   {metrics['header_keys_original']}")
    print(f"  Round-trip: {metrics['header_keys_roundtrip']}")
    hk_diff = metrics['header_keys_diff']
    if hk_diff != 0:
        print(f"  Difference: {c.YELLOW}{hk_diff:+d} keys{c.RESET}")
    else:
        print(f"  Difference: {c.GREEN}No change{c.RESET}")
    
    # Thresholds
    print(f"\n{c.BOLD}Growth Thresholds:{c.RESET}")
    print(f"  Warning:  > {GROWTH_WARN_THRESHOLD*100:.0f}%")
    print(f"  Fail:     > {GROWTH_FAIL_THRESHOLD*100:.0f}%")
    
    # Verdict
    print(f"\n{c.BOLD}{'='*70}{c.RESET}")
    status = metrics['status']
    message = metrics['message']
    
    if status == 'PASS':
        print(f"{c.GREEN}{c.BOLD}✅ {status}: {message}{c.RESET}")
    elif status == 'WARN':
        print(f"{c.YELLOW}{c.BOLD}⚠️  {status}: {message}{c.RESET}")
    else:
        print(f"{c.RED}{c.BOLD}❌ {status}: {message}{c.RESET}")
    
    print(f"{c.BOLD}{'='*70}{c.RESET}\n")

def main():
    if len(sys.argv) < 3:
        print("Usage: track_roundtrip_growth.py <original.riv> <roundtrip.riv> [--json]")
        print()
        print("Tracks size growth and metrics for round-trip conversions")
        print()
        print("Options:")
        print("  --json    Output in JSON format")
        print()
        print("Exit codes:")
        print("  0 - Growth within acceptable range (<5%)")
        print("  1 - Growth warning (5-10%)")
        print("  2 - Growth failure (>10%) or error")
        return 2
    
    original_path = Path(sys.argv[1])
    roundtrip_path = Path(sys.argv[2])
    json_output = '--json' in sys.argv
    
    if not original_path.exists():
        print(f"Error: Original file not found: {original_path}", file=sys.stderr)
        return 2
    
    if not roundtrip_path.exists():
        print(f"Error: Round-trip file not found: {roundtrip_path}", file=sys.stderr)
        return 2
    
    # Get file info
    original_info = get_riv_info(original_path)
    roundtrip_info = get_riv_info(roundtrip_path)
    
    # Calculate metrics
    metrics = calculate_metrics(original_info, roundtrip_info)
    
    if json_output:
        # JSON output for automation
        output = {
            'original': str(original_path),
            'roundtrip': str(roundtrip_path),
            'metrics': metrics
        }
        print(json.dumps(output, indent=2))
    else:
        # Human-readable output
        print_report(original_path, roundtrip_path, metrics)
    
    # Exit code based on status
    if metrics['status'] == 'PASS':
        return 0
    elif metrics['status'] == 'WARN':
        return 1
    else:
        return 2

if __name__ == '__main__':
    sys.exit(main())
