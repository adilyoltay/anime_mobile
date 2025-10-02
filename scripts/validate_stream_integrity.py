#!/usr/bin/env python3
"""
RIV Stream Integrity Validator
Validates consistency between RIV file header and object stream
"""

import sys
import struct
from pathlib import Path
from typing import Dict, List, Set, Tuple, Optional
from collections import defaultdict

def read_varuint(data: bytes, pos: int) -> Tuple[int, int]:
    """Read LEB128 variable-length unsigned integer."""
    result = 0
    shift = 0
    start_pos = pos
    while pos < len(data):
        byte = data[pos]
        pos += 1
        result |= (byte & 0x7F) << shift
        if (byte & 0x80) == 0:
            return result, pos
        shift += 7
    # If we exit the loop, continuation bit was set but no more bytes available
    raise EOFError(f"Unexpected EOF reading varuint at offset {start_pos}")

def analyze_stream_integrity(riv_path: Path) -> Dict:
    """Analyze RIV file for stream integrity issues."""
    data = riv_path.read_bytes()
    
    result = {
        'file': str(riv_path),
        'size': len(data),
        'errors': [],
        'warnings': [],
        'stats': {}
    }
    
    # Check magic
    if data[:4] != b"RIVE":
        result['errors'].append("Invalid magic header (expected 'RIVE')")
        return result
    
    pos = 4
    
    # Read version
    try:
        major, pos = read_varuint(data, pos)
        minor, pos = read_varuint(data, pos)
        file_id, pos = read_varuint(data, pos)
        
        result['version'] = f"{major}.{minor}"
        result['file_id'] = file_id
    except Exception as e:
        result['errors'].append(f"Failed to read header: {e}")
        return result
    
    # Read ToC (Table of Contents)
    toc_keys = []
    try:
        while True:
            key, pos = read_varuint(data, pos)
            if key == 0:
                break
            toc_keys.append(key)
        
        result['toc_keys'] = toc_keys
        result['stats']['toc_key_count'] = len(toc_keys)
    except Exception as e:
        result['errors'].append(f"Failed to read ToC: {e}")
        return result
    
    # Read bitmap
    bitmap_count = (len(toc_keys) + 3) // 4
    bitmap_size = bitmap_count * 4
    
    if pos + bitmap_size > len(data):
        result['errors'].append(f"Bitmap exceeds file size: need {bitmap_size} bytes at {pos}")
        return result
    
    bitmap_data = data[pos:pos + bitmap_size]
    bitmap = [struct.unpack_from("<I", bitmap_data, i * 4)[0] for i in range(bitmap_count)]
    
    # Decode type codes from bitmap
    toc_types = {}
    for i, key in enumerate(toc_keys):
        bucket = i // 4
        shift = (i % 4) * 2
        code = (bitmap[bucket] >> shift) & 0x3
        
        type_name = {
            0: "uint",
            1: "string/bytes",
            2: "double/float",
            3: "color"
        }.get(code, f"unknown({code})")
        
        toc_types[key] = {'code': code, 'type': type_name}
    
    result['toc_types'] = toc_types
    pos += bitmap_size
    
    # Parse object stream
    stream_keys = set()
    stream_type_keys = defaultdict(set)  # typeKey -> set of property keys
    object_count = 0
    property_count = 0
    terminator_count = 0
    
    objects_start = pos
    
    try:
        while pos < len(data):
            type_key, pos = read_varuint(data, pos)
            
            if type_key == 0:
                # Object stream terminator
                terminator_count += 1
                break
            
            object_count += 1
            current_object_props = []
            
            # Read properties for this object
            while pos < len(data):
                prop_key, pos = read_varuint(data, pos)
                
                if prop_key == 0:
                    # Property terminator
                    terminator_count += 1
                    break
                
                stream_keys.add(prop_key)
                stream_type_keys[type_key].add(prop_key)
                current_object_props.append(prop_key)
                property_count += 1
                
                # Read value based on type
                if prop_key in toc_types:
                    code = toc_types[prop_key]['code']
                    
                    if code == 0:  # uint
                        _, pos = read_varuint(data, pos)
                    elif code == 1:  # string/bytes
                        length, pos = read_varuint(data, pos)
                        pos += length
                    elif code == 2:  # double/float
                        if pos + 4 > len(data):
                            result['errors'].append(f"Float value extends past EOF at offset {pos}")
                            break
                        pos += 4
                    elif code == 3:  # color
                        if pos + 4 > len(data):
                            result['errors'].append(f"Color value extends past EOF at offset {pos}")
                            break
                        pos += 4
                else:
                    # Property not in ToC - this is an error!
                    result['errors'].append(
                        f"Property key {prop_key} used in stream but not in ToC (object #{object_count}, typeKey {type_key})"
                    )
                    # Try to skip as uint
                    _, pos = read_varuint(data, pos)
            
            if pos >= len(data):
                result['errors'].append(f"Unexpected EOF after object #{object_count}")
                break
    
    except Exception as e:
        result['errors'].append(f"Stream parsing error at offset {pos}: {e}")
    
    # Statistics
    result['stats']['objects_parsed'] = object_count
    result['stats']['properties_parsed'] = property_count
    result['stats']['terminators'] = terminator_count
    result['stats']['stream_keys_used'] = len(stream_keys)
    result['stats']['objects_start_offset'] = objects_start
    result['stats']['objects_end_offset'] = pos
    result['stats']['objects_size'] = pos - objects_start
    
    # Remaining data (assets/padding)
    remaining = len(data) - pos
    result['stats']['remaining_bytes'] = remaining
    
    # Validate ToC vs Stream consistency
    toc_key_set = set(toc_keys)
    
    # Keys in ToC but not used in stream
    unused_keys = toc_key_set - stream_keys
    if unused_keys:
        result['warnings'].append(
            f"{len(unused_keys)} keys in ToC but not used in stream: {sorted(unused_keys)}"
        )
    
    # Keys used in stream but not in ToC (should be errors, already caught above)
    missing_keys = stream_keys - toc_key_set
    if missing_keys:
        result['errors'].append(
            f"{len(missing_keys)} keys used in stream but missing from ToC: {sorted(missing_keys)}"
        )
    
    # Validate terminator count (should be object_count + 1 for stream terminator)
    expected_terminators = object_count + 1
    if terminator_count != expected_terminators:
        result['warnings'].append(
            f"Terminator count mismatch: found {terminator_count}, expected {expected_terminators}"
        )
    
    return result

def print_report(result: Dict):
    """Print formatted integrity report."""
    print("=" * 70)
    print("RIV Stream Integrity Report")
    print("=" * 70)
    print()
    
    print(f"File: {result['file']}")
    print(f"Size: {result['size']:,} bytes")
    print(f"Version: {result.get('version', 'N/A')}")
    print()
    
    # Statistics
    stats = result.get('stats', {})
    print("Stream Statistics:")
    print(f"  ToC Keys:       {stats.get('toc_key_count', 0)}")
    print(f"  Objects:        {stats.get('objects_parsed', 0)}")
    print(f"  Properties:     {stats.get('properties_parsed', 0)}")
    print(f"  Stream Keys:    {stats.get('stream_keys_used', 0)}")
    print(f"  Terminators:    {stats.get('terminators', 0)}")
    print(f"  Objects Size:   {stats.get('objects_size', 0):,} bytes")
    print(f"  Remaining Data: {stats.get('remaining_bytes', 0):,} bytes")
    print()
    
    # Errors
    errors = result.get('errors', [])
    if errors:
        print(f"❌ Errors ({len(errors)}):")
        for err in errors:
            print(f"  • {err}")
        print()
    
    # Warnings
    warnings = result.get('warnings', [])
    if warnings:
        print(f"⚠️  Warnings ({len(warnings)}):")
        for warn in warnings:
            print(f"  • {warn}")
        print()
    
    # Verdict
    print("=" * 70)
    if errors:
        print("❌ VALIDATION FAILED")
        return 1
    elif warnings:
        print("⚠️  VALIDATION PASSED WITH WARNINGS")
        return 0
    else:
        print("✅ VALIDATION PASSED")
        return 0

def main():
    if len(sys.argv) < 2:
        print("Usage: validate_stream_integrity.py <file.riv>")
        print()
        print("Validates RIV file stream integrity:")
        print("  - ToC vs stream consistency")
        print("  - Type bitmap validation")
        print("  - Property encoding verification")
        print("  - Terminator checks")
        return 2
    
    riv_path = Path(sys.argv[1])
    
    if not riv_path.exists():
        print(f"Error: File not found: {riv_path}")
        return 2
    
    result = analyze_stream_integrity(riv_path)
    return print_report(result)

if __name__ == '__main__':
    sys.exit(main())
