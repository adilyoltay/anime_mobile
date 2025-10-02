#!/usr/bin/env python3
"""
RIV Binary Comparison Tool
Compares two RIV files byte-by-byte with detailed chunk-level analysis
"""

import sys
import argparse
from pathlib import Path
from typing import Dict, List, Tuple, Optional
import json

# Import analyzer from same directory
sys.path.insert(0, str(Path(__file__).parent.parent / 'converter'))
from analyze_riv import analyze

class Colors:
    GREEN = '\033[92m'
    RED = '\033[91m'
    YELLOW = '\033[93m'
    BLUE = '\033[94m'
    CYAN = '\033[96m'
    MAGENTA = '\033[95m'
    RESET = '\033[0m'
    BOLD = '\033[1m'
    DIM = '\033[2m'

def hex_dump_diff(data1: bytes, data2: bytes, offset: int, length: int, label1: str, label2: str) -> str:
    """Generate side-by-side hex dump comparison."""
    output = []
    output.append(f"\n{Colors.BOLD}Offset {offset:04x} - {offset+length:04x}{Colors.RESET}")
    output.append(f"{Colors.DIM}{'─' * 80}{Colors.RESET}")
    
    for i in range(0, length, 16):
        pos = offset + i
        chunk_size = min(16, length - i)
        
        # Extract chunks
        chunk1 = data1[pos:pos+chunk_size] if pos < len(data1) else b''
        chunk2 = data2[pos:pos+chunk_size] if pos < len(data2) else b''
        
        # Format hex
        hex1 = ' '.join(f'{b:02x}' for b in chunk1)
        hex2 = ' '.join(f'{b:02x}' for b in chunk2)
        
        # Compare and colorize
        hex1_colored = []
        hex2_colored = []
        for j in range(chunk_size):
            b1 = chunk1[j] if j < len(chunk1) else None
            b2 = chunk2[j] if j < len(chunk2) else None
            
            if b1 != b2:
                if b1 is not None:
                    hex1_colored.append(f'{Colors.RED}{b1:02x}{Colors.RESET}')
                if b2 is not None:
                    hex2_colored.append(f'{Colors.GREEN}{b2:02x}{Colors.RESET}')
            else:
                if b1 is not None:
                    hex1_colored.append(f'{b1:02x}')
                if b2 is not None:
                    hex2_colored.append(f'{b2:02x}')
        
        # Format output
        hex1_str = ' '.join(hex1_colored).ljust(70)  # Account for ANSI codes
        hex2_str = ' '.join(hex2_colored).ljust(70)
        
        output.append(f"{pos:04x} {Colors.BLUE}{label1[:8]:8s}{Colors.RESET} {hex1_str}")
        output.append(f"     {Colors.CYAN}{label2[:8]:8s}{Colors.RESET} {hex2_str}")
        
        if i + 16 < length:
            output.append("")
    
    return '\n'.join(output)

def find_differences(data1: bytes, data2: bytes, max_diff_regions: int = 10) -> List[Tuple[int, int]]:
    """Find regions where files differ."""
    differences = []
    min_len = min(len(data1), len(data2))
    
    in_diff = False
    diff_start = 0
    
    for i in range(min_len):
        if data1[i] != data2[i]:
            if not in_diff:
                diff_start = i
                in_diff = True
        else:
            if in_diff:
                differences.append((diff_start, i))
                in_diff = False
                if len(differences) >= max_diff_regions:
                    break
    
    if in_diff:
        differences.append((diff_start, min_len))
    
    # Add size difference if exists
    if len(data1) != len(data2):
        differences.append((min_len, max(len(data1), len(data2))))
    
    return differences

def compare_chunks(analysis1: Dict, analysis2: Dict) -> Dict:
    """Compare chunk-level structure."""
    comparison = {
        'header_match': True,
        'object_count_match': True,
        'type_sequence_match': True,
        'property_count_match': True,
        'differences': []
    }
    
    # Compare headers
    if analysis1['version'] != analysis2['version']:
        comparison['header_match'] = False
        comparison['differences'].append(
            f"Version mismatch: {analysis1['version']} vs {analysis2['version']}"
        )
    
    # Compare header keys
    keys1 = set(h['key'] for h in analysis1['headerKeys'])
    keys2 = set(h['key'] for h in analysis2['headerKeys'])
    
    if keys1 != keys2:
        missing = keys1 - keys2
        extra = keys2 - keys1
        if missing:
            comparison['differences'].append(f"Missing keys in file2: {missing}")
        if extra:
            comparison['differences'].append(f"Extra keys in file2: {extra}")
    
    # Compare object counts
    count1 = len(analysis1['objects'])
    count2 = len(analysis2['objects'])
    
    if count1 != count2:
        comparison['object_count_match'] = False
        comparison['differences'].append(
            f"Object count: {count1} vs {count2} (diff: {count2-count1:+d})"
        )
    
    # Compare object type sequence
    types1 = [obj['typeKey'] for obj in analysis1['objects']]
    types2 = [obj['typeKey'] for obj in analysis2['objects']]
    
    if types1 != types2:
        comparison['type_sequence_match'] = False
        # Find first mismatch
        for i, (t1, t2) in enumerate(zip(types1, types2)):
            if t1 != t2:
                comparison['differences'].append(
                    f"First type mismatch at object {i}: {t1} vs {t2}"
                )
                break
    
    # Compare total property count
    props1 = sum(len(obj['properties']) for obj in analysis1['objects'])
    props2 = sum(len(obj['properties']) for obj in analysis2['objects'])
    
    if props1 != props2:
        comparison['property_count_match'] = False
        comparison['differences'].append(
            f"Total properties: {props1} vs {props2} (diff: {props2-props1:+d})"
        )
    
    return comparison

def print_comparison_report(file1: Path, file2: Path, 
                           data1: bytes, data2: bytes,
                           analysis1: Dict, analysis2: Dict):
    """Print detailed comparison report."""
    c = Colors
    
    print(f"\n{c.BOLD}{'='*80}{c.RESET}")
    print(f"{c.BOLD}RIV File Comparison Report{c.RESET}")
    print(f"{c.BOLD}{'='*80}{c.RESET}\n")
    
    # File info
    print(f"{c.BLUE}File 1:{c.RESET} {file1}")
    print(f"  Size: {len(data1):,} bytes")
    print(f"  Objects: {len(analysis1['objects'])}")
    
    print(f"\n{c.CYAN}File 2:{c.RESET} {file2}")
    print(f"  Size: {len(data2):,} bytes")
    print(f"  Objects: {len(analysis2['objects'])}")
    
    # Size comparison
    size_diff = len(data2) - len(data1)
    size_pct = (size_diff / len(data1) * 100) if len(data1) > 0 else 0
    
    print(f"\n{c.BOLD}Size Comparison:{c.RESET}")
    if size_diff == 0:
        print(f"  {c.GREEN}✅ Identical size{c.RESET}")
    else:
        color = c.YELLOW if abs(size_pct) < 5 else c.RED
        print(f"  {color}Δ {size_diff:+,} bytes ({size_pct:+.2f}%){c.RESET}")
    
    # Chunk comparison
    print(f"\n{c.BOLD}Chunk Analysis:{c.RESET}")
    chunk_comp = compare_chunks(analysis1, analysis2)
    
    if not chunk_comp['differences']:
        print(f"  {c.GREEN}✅ Structures match perfectly{c.RESET}")
    else:
        print(f"  {c.YELLOW}⚠️  Differences found:{c.RESET}")
        for diff in chunk_comp['differences']:
            print(f"    • {diff}")
    
    # Binary differences
    print(f"\n{c.BOLD}Binary Differences:{c.RESET}")
    diff_regions = find_differences(data1, data2, max_diff_regions=5)
    
    if not diff_regions:
        print(f"  {c.GREEN}✅ Files are byte-for-byte identical{c.RESET}")
    else:
        print(f"  {c.YELLOW}Found {len(diff_regions)} difference region(s){c.RESET}")
        
        for i, (start, end) in enumerate(diff_regions[:3], 1):
            length = min(end - start, 64)  # Limit display
            print(hex_dump_diff(data1, data2, start, length, 
                              file1.name, file2.name))
    
    # Verdict
    print(f"\n{c.BOLD}{'='*80}{c.RESET}")
    if not diff_regions and not chunk_comp['differences']:
        print(f"{c.GREEN}{c.BOLD}✅ FILES ARE IDENTICAL{c.RESET}")
    elif not chunk_comp['differences']:
        print(f"{c.YELLOW}{c.BOLD}⚠️  BINARY DIFFERS BUT STRUCTURE MATCHES{c.RESET}")
    else:
        print(f"{c.RED}{c.BOLD}❌ FILES DIFFER STRUCTURALLY{c.RESET}")
    print(f"{c.BOLD}{'='*80}{c.RESET}\n")

def main():
    parser = argparse.ArgumentParser(
        description='Compare two RIV files in detail',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Compare original vs round-trip
  %(prog)s original.riv roundtrip.riv
  
  # JSON output for automation
  %(prog)s file1.riv file2.riv --json
  
  # Show only summary
  %(prog)s file1.riv file2.riv --summary
        """
    )
    parser.add_argument('file1', type=Path, help='First RIV file')
    parser.add_argument('file2', type=Path, help='Second RIV file')
    parser.add_argument('--json', action='store_true', help='JSON output')
    parser.add_argument('--summary', action='store_true', help='Summary only')
    
    args = parser.parse_args()
    
    if not args.file1.exists():
        print(f"Error: File not found: {args.file1}", file=sys.stderr)
        return 2
    
    if not args.file2.exists():
        print(f"Error: File not found: {args.file2}", file=sys.stderr)
        return 2
    
    # Read files
    data1 = args.file1.read_bytes()
    data2 = args.file2.read_bytes()
    
    # Analyze both
    analysis1 = analyze(args.file1, return_data=True)
    analysis2 = analyze(args.file2, return_data=True)
    
    if args.json:
        # JSON output
        result = {
            'file1': str(args.file1),
            'file2': str(args.file2),
            'size1': len(data1),
            'size2': len(data2),
            'size_diff': len(data2) - len(data1),
            'size_diff_pct': (len(data2) - len(data1)) / len(data1) * 100 if len(data1) > 0 else 0,
            'chunk_comparison': compare_chunks(analysis1, analysis2),
            'binary_identical': data1 == data2
        }
        print(json.dumps(result, indent=2))
    else:
        # Visual output
        print_comparison_report(args.file1, args.file2, data1, data2, analysis1, analysis2)
    
    # Exit code
    if data1 == data2:
        return 0  # Perfect match
    
    chunk_comp = compare_chunks(analysis1, analysis2)
    if not chunk_comp['differences']:
        return 0  # Structurally identical
    
    return 1  # Differences found

if __name__ == '__main__':
    sys.exit(main())
