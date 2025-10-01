#!/usr/bin/env python3
"""
Detect float patterns in Type 8064 blobs.
Hypothesis: Type 8064 contains packed float arrays (keyframe data).
"""

import sys
import struct

def find_type_8064_offsets(data):
    """Find all Type 8064 markers (varuint: 0x80 0x7C)."""
    offsets = []
    for i in range(len(data) - 1):
        if data[i] == 0x80 and data[i+1] == 0x7C:
            offsets.append(i)
    return offsets

def extract_floats(data, offset, max_floats=20):
    """Extract potential floats from binary data."""
    floats = []
    pos = offset + 2  # Skip type marker
    
    for _ in range(max_floats):
        if pos + 4 > len(data):
            break
        
        try:
            val = struct.unpack('<f', data[pos:pos+4])[0]
            
            # Only keep reasonable values (filter garbage)
            if -1e6 < val < 1e6 and not (val != val):  # Not NaN
                floats.append({
                    'offset': pos,
                    'value': val,
                    'hex': data[pos:pos+4].hex()
                })
        except:
            pass
        
        pos += 1  # Slide window by 1 byte
    
    return floats

def main(filepath):
    with open(filepath, 'rb') as f:
        data = f.read()
    
    print(f"🔢 FLOAT DETECTION: Type 8064")
    print(f"File: {filepath}")
    print("="*70)
    
    offsets = find_type_8064_offsets(data)
    print(f"\nFound {len(offsets)} Type 8064 blobs\n")
    
    for i, offset in enumerate(offsets[:5]):  # First 5
        print(f"{'─'*70}")
        print(f"Blob #{i+1} at offset {offset}")
        print(f"{'─'*70}")
        
        # Show raw hex (first 48 bytes)
        chunk = data[offset:offset+48]
        print(f"Raw hex:")
        for j in range(0, len(chunk), 16):
            hex_str = ' '.join(f'{b:02x}' for b in chunk[j:j+16])
            print(f"  {hex_str}")
        
        # Extract floats
        floats = extract_floats(data, offset, max_floats=30)
        
        # Filter to keep only likely keyframe data
        # Keyframe properties: time (0-100s), value (-100 to 100), etc.
        likely_keyframe_floats = [f for f in floats if 0 <= f['value'] <= 100]
        
        if likely_keyframe_floats:
            print(f"\n🎯 Likely keyframe floats (0-100 range):")
            for f in likely_keyframe_floats[:10]:
                print(f"  Offset {f['offset']:4d}: {f['value']:10.6f}  (hex: {f['hex']})")
        
        # Look for repeating patterns
        all_values = [f['value'] for f in floats]
        if len(all_values) > 3:
            print(f"\n📊 All float values found:")
            for val in all_values[:15]:
                print(f"  {val:10.6f}")
    
    # Global analysis
    print(f"\n{'='*70}")
    print(f"🔍 GLOBAL PATTERN ANALYSIS")
    print(f"{'='*70}")
    
    all_floats_in_range = []
    for offset in offsets:
        floats = extract_floats(data, offset, max_floats=50)
        all_floats_in_range.extend([f['value'] for f in floats if 0 <= f['value'] <= 100])
    
    if all_floats_in_range:
        print(f"\nAll floats in 0-100 range across all blobs:")
        print(f"  Total: {len(all_floats_in_range)}")
        print(f"  Min: {min(all_floats_in_range):.6f}")
        print(f"  Max: {max(all_floats_in_range):.6f}")
        print(f"  Avg: {sum(all_floats_in_range)/len(all_floats_in_range):.6f}")
        
        # Common values
        from collections import Counter
        common = Counter(all_floats_in_range).most_common(10)
        print(f"\n  Most common values:")
        for val, count in common:
            print(f"    {val:10.6f}: {count} times")

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <file.riv>")
        sys.exit(1)
    
    main(sys.argv[1])
