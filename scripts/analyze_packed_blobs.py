#!/usr/bin/env python3
"""
Analyze packed animation blobs in RIV files.
Attempt to reverse-engineer types 8064, 7776, and 64.
"""

import sys
import struct
import json
from pathlib import Path

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

def analyze_riv_structure(filepath):
    """Analyze RIV file structure focusing on packed blobs."""
    with open(filepath, 'rb') as f:
        data = f.read()
    
    print(f"Analyzing: {filepath}")
    print(f"File size: {len(data)} bytes")
    print("=" * 60)
    
    # Read header
    if len(data) < 4:
        print("File too small")
        return
    
    fingerprint = data[0:4]
    print(f"Fingerprint: {fingerprint.hex()} ({fingerprint})")
    
    # Parse objects
    offset = 4
    objects_found = []
    
    while offset < len(data):
        if offset + 2 > len(data):
            break
        
        # Read type key (varuint)
        typeKey, consumed = read_varuint(data, offset)
        if consumed == 0:
            break
        
        start_offset = offset
        offset += consumed
        
        # Track packed blob types
        if typeKey in [8064, 7776, 64]:
            print(f"\n🎯 FOUND PACKED BLOB at offset {start_offset}")
            print(f"   Type: {typeKey}")
            
            # Read next 32 bytes to analyze structure
            chunk = data[start_offset:start_offset+32]
            print(f"   Hex: {chunk.hex()}")
            print(f"   Bytes: {' '.join(f'{b:02x}' for b in chunk[:16])}")
            
            # Try to parse as property keys
            temp_offset = start_offset + consumed
            properties = []
            
            for _ in range(10):  # Try to read up to 10 properties
                if temp_offset >= len(data):
                    break
                
                prop_key, prop_consumed = read_varuint(data, temp_offset)
                if prop_consumed == 0 or prop_key > 10000:  # Sanity check
                    break
                
                temp_offset += prop_consumed
                
                # Try to read value (assume varuint for now)
                prop_val, val_consumed = read_varuint(data, temp_offset)
                if val_consumed == 0:
                    break
                
                temp_offset += val_consumed
                properties.append((prop_key, prop_val))
            
            print(f"   Possible properties: {properties[:5]}")
            
            objects_found.append({
                'offset': start_offset,
                'typeKey': typeKey,
                'properties': properties[:5]
            })
        
        # Skip ahead (rough estimate)
        offset += 10  # Skip some data to find next object
    
    print(f"\n" + "=" * 60)
    print(f"Total packed blobs found: {len(objects_found)}")
    print(f"\nType distribution:")
    type_counts = {}
    for obj in objects_found:
        tk = obj['typeKey']
        type_counts[tk] = type_counts.get(tk, 0) + 1
    
    for tk, count in sorted(type_counts.items()):
        print(f"  Type {tk}: {count} objects")
    
    return objects_found

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <file.riv>")
        sys.exit(1)
    
    blobs = analyze_riv_structure(sys.argv[1])
    
    # Save analysis
    output_file = 'packed_blob_analysis.json'
    with open(output_file, 'w') as f:
        json.dump(blobs, f, indent=2)
    
    print(f"\n💾 Analysis saved to: {output_file}")
