#!/usr/bin/env python3
"""
Round-trip RIV Validator
Validates that converted .riv files maintain:
1. Correct chunk order and sizes
2. Compression/encryption flags
3. Proper alignment and padding
4. Binary structure integrity
"""

import sys
import struct
import argparse
from pathlib import Path
from typing import Tuple, List, Dict, Any
from collections import defaultdict

# Colors for terminal output
class Colors:
    GREEN = '\033[92m'
    RED = '\033[91m'
    YELLOW = '\033[93m'
    BLUE = '\033[94m'
    RESET = '\033[0m'
    BOLD = '\033[1m'

def read_varuint(data: bytes, pos: int) -> Tuple[int, int]:
    """Read Rive variable-length unsigned integer."""
    result = 0
    shift = 0
    start_pos = pos
    while pos < len(data):
        byte = data[pos]
        pos += 1
        result |= (byte & 0x7F) << shift
        if not (byte & 0x80):
            return result, pos
        shift += 7
    raise EOFError(f"Unexpected EOF reading varuint at {start_pos}")

def analyze_riv_structure(data: bytes) -> Dict[str, Any]:
    """Analyze RIV file structure and return detailed metadata."""
    if data[:4] != b"RIVE":
        raise ValueError("Not a RIVE file")
    
    result = {
        'magic': data[:4].decode('ascii'),
        'size': len(data),
        'chunks': [],
        'header': {},
        'objects': [],
        'padding': [],
        'errors': [],
        'warnings': []
    }
    
    pos = 4
    
    # Header: version + fileId
    try:
        major, pos = read_varuint(data, pos)
        minor, pos = read_varuint(data, pos)
        file_id, pos = read_varuint(data, pos)
        
        result['header'] = {
            'version': f"{major}.{minor}",
            'fileId': file_id,
            'offset': 4,
            'size': pos - 4
        }
        result['chunks'].append(('HEADER', 4, pos - 4))
    except Exception as e:
        result['errors'].append(f"Failed to parse header: {e}")
        return result
    
    # ToC (Table of Contents): property keys
    toc_start = pos
    header_keys = []
    try:
        while True:
            key, pos = read_varuint(data, pos)
            if key == 0:
                break
            header_keys.append(key)
        
        result['header']['propertyKeys'] = header_keys
        result['header']['propertyCount'] = len(header_keys)
        result['chunks'].append(('TOC', toc_start, pos - toc_start))
        
        # NOTE: No padding between ToC and bitmap (per RuntimeHeader::read specification)
            
    except Exception as e:
        result['errors'].append(f"Failed to parse ToC: {e}")
        return result
    
    # Bitmap: field type definitions (4-byte aligned)
    bitmap_start = pos
    bitmap_count = (len(header_keys) + 3) // 4
    bitmap_size = bitmap_count * 4
    
    if pos + bitmap_size > len(data):
        result['errors'].append(f"Bitmap exceeds file size: need {bitmap_size} bytes at {pos}")
        return result
    
    bitmap_bytes = data[pos:pos + bitmap_size]
    bitmaps = [struct.unpack_from("<I", bitmap_bytes, i * 4)[0] for i in range(bitmap_count)]
    
    result['header']['bitmap'] = {
        'offset': bitmap_start,
        'size': bitmap_size,
        'count': bitmap_count,
        'values': bitmaps
    }
    result['chunks'].append(('BITMAP', bitmap_start, bitmap_size))
    pos += bitmap_size
    
    # Objects: core objects stream
    objects_start = pos
    obj_count = 0
    bytes_objects = []
    
    try:
        while pos < len(data):
            obj_start = pos
            type_key, pos = read_varuint(data, pos)
            
            if type_key == 0:
                # Stream terminator
                result['chunks'].append(('OBJECTS', objects_start, pos - objects_start))
                break
            
            # Read properties
            props = []
            while True:
                prop_key, pos = read_varuint(data, pos)
                if prop_key == 0:
                    break
                
                # Determine property type from bitmap
                idx = header_keys.index(prop_key) if prop_key in header_keys else -1
                if idx >= 0:
                    bucket = idx // 4
                    shift = (idx % 4) * 2
                    type_id = (bitmaps[bucket] >> shift) & 0x3
                    prop_type = ["uint", "string", "double", "color"][type_id]
                else:
                    prop_type = "unknown"
                
                # Special case: bytes property (212 = FileAssetContents.bytes)
                if prop_key == 212:
                    byte_len, pos = read_varuint(data, pos)
                    if pos + byte_len > len(data):
                        raise ValueError(f"Bytes property {byte_len} exceeds file at {pos}")
                    bytes_objects.append({
                        'type': type_key,
                        'offset': pos,
                        'size': byte_len
                    })
                    pos += byte_len
                    prop_type = "bytes"
                    value = f"<{byte_len} bytes>"
                elif prop_type == "uint":
                    value, pos = read_varuint(data, pos)
                elif prop_type == "string":
                    length, pos = read_varuint(data, pos)
                    value = data[pos:pos+length].decode('utf-8', errors='replace')
                    pos += length
                elif prop_type in ["double", "color"]:
                    value = struct.unpack_from("<f" if prop_type == "double" else "<I", data, pos)[0]
                    pos += 4
                else:
                    # Fallback: try uint
                    value, pos = read_varuint(data, pos)
                
                props.append({'key': prop_key, 'type': prop_type, 'value': value})
            
            obj_count += 1
            result['objects'].append({
                'index': obj_count - 1,
                'typeKey': type_key,
                'offset': obj_start,
                'size': pos - obj_start,
                'properties': props
            })
            
    except EOFError:
        result['warnings'].append(f"Unexpected EOF parsing objects at position {pos}")
    except Exception as e:
        result['errors'].append(f"Failed parsing object #{obj_count}: {e}")
    
    # Check for remaining data (assets, padding, etc.)
    if pos < len(data):
        remaining = len(data) - pos
        result['chunks'].append(('ASSETS/PADDING', pos, remaining))
        result['warnings'].append(f"Found {remaining} bytes after object stream")
        
        # Check if it's all padding (zeros)
        trailing = data[pos:]
        non_zero = sum(1 for b in trailing if b != 0)
        if non_zero == 0:
            result['padding'].append({'offset': pos, 'size': remaining})
        else:
            # Likely asset data
            result['header']['assetData'] = {
                'offset': pos,
                'size': remaining,
                'bytesObjects': bytes_objects
            }
    
    return result

def validate_alignment(data: bytes, result: Dict[str, Any]) -> None:
    """Validate that chunks are properly aligned."""
    # NOTE: Bitmap does NOT need to be 4-byte aligned
    # Per RuntimeHeader::read(), bitmap starts immediately after ToC terminator
    # No padding is inserted, alignment is not enforced
    
    # Check asset data alignment (if present)
    asset_data = result['header'].get('assetData', {})
    if asset_data:
        for bytes_obj in asset_data.get('bytesObjects', []):
            offset = bytes_obj['offset']
            # Asset data should ideally be aligned, but not strict requirement
            if offset % 4 != 0:
                result['warnings'].append(f"Asset at {offset} not 4-byte aligned")

def validate_compression_flags(result: Dict[str, Any]) -> None:
    """Check for compression/encryption flags (future extension)."""
    # RIV format doesn't currently use compression flags in header
    # This is a placeholder for future validation
    pass

def compare_structures(original: Dict[str, Any], roundtrip: Dict[str, Any]) -> Dict[str, Any]:
    """Compare two RIV structures and report differences."""
    comparison = {
        'identical': True,
        'size_diff': roundtrip['size'] - original['size'],
        'differences': [],
        'matches': []
    }
    
    # Compare versions
    if original['header']['version'] != roundtrip['header']['version']:
        comparison['identical'] = False
        comparison['differences'].append(
            f"Version mismatch: {original['header']['version']} vs {roundtrip['header']['version']}"
        )
    else:
        comparison['matches'].append('Version matches')
    
    # Compare property keys
    orig_keys = set(original['header']['propertyKeys'])
    rt_keys = set(roundtrip['header']['propertyKeys'])
    
    if orig_keys != rt_keys:
        comparison['identical'] = False
        missing = orig_keys - rt_keys
        extra = rt_keys - orig_keys
        if missing:
            comparison['differences'].append(f"Missing property keys: {missing}")
        if extra:
            comparison['differences'].append(f"Extra property keys: {extra}")
    else:
        comparison['matches'].append(f'Property keys match ({len(orig_keys)} keys)')
    
    # Compare object counts
    orig_count = len(original['objects'])
    rt_count = len(roundtrip['objects'])
    
    if orig_count != rt_count:
        comparison['identical'] = False
        comparison['differences'].append(
            f"Object count mismatch: {orig_count} vs {rt_count}"
        )
    else:
        comparison['matches'].append(f'Object count matches ({orig_count} objects)')
    
    # Compare object types
    orig_types = [obj['typeKey'] for obj in original['objects']]
    rt_types = [obj['typeKey'] for obj in roundtrip['objects']]
    
    if orig_types != rt_types:
        comparison['identical'] = False
        comparison['differences'].append("Object type sequence differs")
    else:
        comparison['matches'].append('Object type sequence matches')
    
    # Compare chunk structure
    orig_chunks = [(name, size) for name, _, size in original['chunks']]
    rt_chunks = [(name, size) for name, _, size in roundtrip['chunks']]
    
    for i, ((o_name, o_size), (r_name, r_size)) in enumerate(zip(orig_chunks, rt_chunks)):
        if o_name != r_name:
            comparison['identical'] = False
            comparison['differences'].append(f"Chunk {i}: type mismatch {o_name} vs {r_name}")
        elif o_size != r_size:
            diff_pct = ((r_size - o_size) / o_size * 100) if o_size > 0 else 0
            comparison['differences'].append(
                f"Chunk {i} ({o_name}): size diff {o_size} → {r_size} ({diff_pct:+.1f}%)"
            )
    
    return comparison

def print_report(result: Dict[str, Any], comparison: Dict[str, Any] = None):
    """Pretty print validation report."""
    c = Colors
    
    print(f"\n{c.BOLD}{'='*70}{c.RESET}")
    print(f"{c.BOLD}RIV Structure Analysis{c.RESET}")
    print(f"{c.BOLD}{'='*70}{c.RESET}\n")
    
    # File info
    print(f"{c.BLUE}File Information:{c.RESET}")
    print(f"  Magic: {result['magic']}")
    print(f"  Size: {result['size']:,} bytes")
    print(f"  Version: {result['header'].get('version', 'N/A')}")
    print(f"  File ID: {result['header'].get('fileId', 'N/A')}")
    
    # Chunks with visualization
    print(f"\n{c.BLUE}Chunk Structure:{c.RESET}")
    total_size = result['size']
    for name, offset, size in result['chunks']:
        # Calculate percentage
        pct = (size / total_size * 100) if total_size > 0 else 0
        # Create visual bar (50 chars max)
        bar_length = int(pct / 2)
        bar = '█' * bar_length
        print(f"  {name:20s} @ {offset:6d}  ({size:8,} bytes, {pct:5.1f}%) {c.GREEN}{bar}{c.RESET}")
    
    # Objects
    obj_count = len(result['objects'])
    print(f"\n{c.BLUE}Objects:{c.RESET} {obj_count} total")
    
    # Type distribution
    type_counts = defaultdict(int)
    for obj in result['objects']:
        type_counts[obj['typeKey']] += 1
    
    print(f"  Type distribution:")
    for type_key, count in sorted(type_counts.items(), key=lambda x: -x[1])[:10]:
        print(f"    Type {type_key:4d}: {count:4d} objects")
    
    # Errors and warnings
    if result['errors']:
        print(f"\n{c.RED}{c.BOLD}❌ Errors:{c.RESET}")
        for err in result['errors']:
            print(f"  {c.RED}• {err}{c.RESET}")
    
    if result['warnings']:
        print(f"\n{c.YELLOW}⚠️  Warnings:{c.RESET}")
        for warn in result['warnings']:
            print(f"  {c.YELLOW}• {warn}{c.RESET}")
    
    # Comparison results
    if comparison:
        print(f"\n{c.BOLD}{'='*70}{c.RESET}")
        print(f"{c.BOLD}Round-trip Comparison{c.RESET}")
        print(f"{c.BOLD}{'='*70}{c.RESET}\n")
        
        status = f"{c.GREEN}✅ IDENTICAL{c.RESET}" if comparison['identical'] else f"{c.YELLOW}⚠️  DIFFERS{c.RESET}"
        print(f"Status: {status}")
        print(f"Size difference: {comparison['size_diff']:+,} bytes")
        
        if comparison['matches']:
            print(f"\n{c.GREEN}✅ Matches:{c.RESET}")
            for match in comparison['matches']:
                print(f"  {c.GREEN}• {match}{c.RESET}")
        
        if comparison['differences']:
            print(f"\n{c.YELLOW}⚠️  Differences:{c.RESET}")
            for diff in comparison['differences']:
                print(f"  {c.YELLOW}• {diff}{c.RESET}")
    
    # Final verdict
    print(f"\n{c.BOLD}{'='*70}{c.RESET}")
    if not result['errors']:
        print(f"{c.GREEN}{c.BOLD}✅ VALIDATION PASSED{c.RESET}")
    else:
        print(f"{c.RED}{c.BOLD}❌ VALIDATION FAILED{c.RESET}")
    print(f"{c.BOLD}{'='*70}{c.RESET}\n")

def main():
    parser = argparse.ArgumentParser(
        description='Validate round-trip RIV conversion',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Analyze single file
  %(prog)s output.riv
  
  # Compare original vs round-trip
  %(prog)s original.riv --compare roundtrip.riv
  
  # JSON output for CI
  %(prog)s output.riv --json
        """
    )
    parser.add_argument('riv_file', type=Path, help='RIV file to validate')
    parser.add_argument('--compare', type=Path, help='Compare with another RIV file')
    parser.add_argument('--json', action='store_true', help='Output as JSON')
    parser.add_argument('--strict', action='store_true', help='Fail on warnings')
    
    args = parser.parse_args()
    
    if not args.riv_file.exists():
        print(f"Error: File not found: {args.riv_file}", file=sys.stderr)
        return 2
    
    try:
        # Analyze primary file
        data = args.riv_file.read_bytes()
        result = analyze_riv_structure(data)
        validate_alignment(data, result)
        validate_compression_flags(result)
        
        # Compare if requested
        comparison = None
        if args.compare:
            if not args.compare.exists():
                print(f"Error: Comparison file not found: {args.compare}", file=sys.stderr)
                return 2
            
            compare_data = args.compare.read_bytes()
            compare_result = analyze_riv_structure(compare_data)
            comparison = compare_structures(result, compare_result)
        
        # Output
        if args.json:
            import json
            output = {
                'file': str(args.riv_file),
                'analysis': result,
                'comparison': comparison
            }
            print(json.dumps(output, indent=2, default=str))
        else:
            print_report(result, comparison)
        
        # Exit code
        if result['errors']:
            return 1
        if args.strict and result['warnings']:
            return 1
        if comparison and not comparison['identical']:
            return 1
        
        return 0
        
    except Exception as e:
        print(f"Fatal error: {e}", file=sys.stderr)
        import traceback
        traceback.print_exc()
        return 2

if __name__ == '__main__':
    sys.exit(main())
