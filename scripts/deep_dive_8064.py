#!/usr/bin/env python3
"""
Deep dive into Type 8064 structure.
Extract ALL Type 8064 blobs and analyze patterns.
"""

import sys
import struct
from collections import defaultdict

def read_varuint(data, offset):
    """Read Rive variable-length unsigned integer."""
    result = 0
    shift = 0
    pos = offset
    
    while pos < len(data):
        byte = data[pos]
        result |= (byte & 0x7F) << shift
        pos += 1
        if (byte & 0x80) == 0:
            break
        shift += 7
    
    return result, pos - offset

def find_all_type_markers(data, target_type):
    """Find all occurrences of a specific type in the file."""
    # Encode target type as varuint
    varuint_bytes = []
    temp = target_type
    while True:
        byte = temp & 0x7F
        temp >>= 7
        if temp > 0:
            byte |= 0x80
        varuint_bytes.append(byte)
        if temp == 0:
            break
    
    # Search for this varuint pattern
    offsets = []
    pattern = bytes(varuint_bytes)
    
    pos = 0
    while pos < len(data) - len(pattern):
        if data[pos:pos+len(pattern)] == pattern:
            offsets.append(pos)
        pos += 1
    
    return offsets

def analyze_8064_structure(data, offset):
    """Deep analysis of a single Type 8064 blob."""
    # Skip type marker (2 bytes: 0x80 0x7C for 8064)
    pos = offset + 2
    
    if pos >= len(data):
        return None
    
    # Read next 128 bytes for analysis
    chunk = data[pos:pos+128]
    
    analysis = {
        'offset': offset,
        'hex_head': chunk[:32].hex(),
        'bytes': list(chunk[:32]),
        'properties': []
    }
    
    # Try to identify patterns
    # Common patterns in Rive:
    # - Property key (varuint)
    # - Value (depends on type: varuint, float, string, etc.)
    
    temp_pos = pos
    for attempt in range(20):
        if temp_pos >= len(data):
            break
        
        # Read potential property key
        key, consumed = read_varuint(data, temp_pos)
        if consumed == 0 or key > 10000:
            break
        
        temp_pos += consumed
        
        # Property 14, 8484, 4, 193 were seen before
        # Let's see what follows each
        
        # Try varuint value
        val, val_consumed = read_varuint(data, temp_pos)
        
        analysis['properties'].append({
            'key': key,
            'value_uint': val,
            'value_bytes': data[temp_pos:temp_pos+4].hex()
        })
        
        temp_pos += val_consumed
        
        if temp_pos - pos > 60:  # Don't go too far
            break
    
    return analysis

def main(filepath):
    with open(filepath, 'rb') as f:
        data = f.read()
    
    print(f"🔬 DEEP DIVE: Type 8064 Analysis")
    print(f"File: {filepath}")
    print(f"Size: {len(data)} bytes")
    print("="*70)
    
    # Find all Type 8064 occurrences
    offsets_8064 = find_all_type_markers(data, 8064)
    
    print(f"\n📍 Found {len(offsets_8064)} Type 8064 blobs")
    
    # Analyze each one
    all_analyses = []
    for i, offset in enumerate(offsets_8064[:10]):  # First 10
        print(f"\n{'─'*70}")
        print(f"Blob #{i+1} at offset {offset}")
        print(f"{'─'*70}")
        
        analysis = analyze_8064_structure(data, offset)
        if analysis:
            all_analyses.append(analysis)
            
            print(f"Hex head: {analysis['hex_head']}")
            print(f"\nProperties detected:")
            for p in analysis['properties']:
                print(f"  Key {p['key']:5d}: {p['value_uint']:10d} | bytes: {p['value_bytes']}")
    
    # Look for commonalities
    print(f"\n{'='*70}")
    print(f"🔍 PATTERN ANALYSIS")
    print(f"{'='*70}")
    
    # Count property keys
    key_counts = defaultdict(int)
    for analysis in all_analyses:
        for prop in analysis['properties']:
            key_counts[prop['key']] += 1
    
    print(f"\nMost common property keys:")
    for key, count in sorted(key_counts.items(), key=lambda x: x[1], reverse=True)[:10]:
        print(f"  Key {key:5d}: appears {count} times")
    
    # Look for byte patterns
    print(f"\nFirst byte after type marker:")
    first_bytes = defaultdict(int)
    for analysis in all_analyses:
        if analysis['bytes']:
            first_bytes[analysis['bytes'][0]] += 1
    
    for byte, count in sorted(first_bytes.items(), key=lambda x: x[1], reverse=True):
        print(f"  0x{byte:02x}: {count} times")
    
    return all_analyses

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <file.riv>")
        sys.exit(1)
    
    analyses = main(sys.argv[1])
    
    # Save detailed analysis
    import json
    with open('type_8064_analysis.json', 'w') as f:
        json.dump(analyses, f, indent=2)
    
    print(f"\n💾 Detailed analysis saved to: type_8064_analysis.json")
