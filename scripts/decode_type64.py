#!/usr/bin/env python3
"""
Decode Type 64 (Packed KeyFrame Array) structure.
Hypothesis: Type 64 = compact KeyFrameDouble array
"""

import sys
import struct

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

def read_float(data, offset):
    """Read IEEE 754 float (4 bytes, little-endian)."""
    if offset + 4 > len(data):
        return None, 0
    return struct.unpack('<f', data[offset:offset+4])[0], 4

def decode_type64_block(data, offset):
    """Attempt to decode a Type 64 block."""
    print(f"\n{'='*60}")
    print(f"Decoding Type 64 at offset {offset}")
    print(f"{'='*60}")
    
    # Type marker should be 0x40 (64)
    if data[offset] != 0x40:
        print(f"❌ Expected 0x40, got 0x{data[offset]:02x}")
        return
    
    print(f"✅ Type marker: 0x40 (Type 64)")
    pos = offset + 1
    
    # Show next 64 bytes in hex
    chunk = data[offset:offset+64]
    print(f"\nHex dump (64 bytes):")
    for i in range(0, min(64, len(chunk)), 16):
        hex_str = ' '.join(f'{b:02x}' for b in chunk[i:i+16])
        ascii_str = ''.join(chr(b) if 32 <= b < 127 else '.' for b in chunk[i:i+16])
        print(f"  {offset+i:04x}: {hex_str:<48} {ascii_str}")
    
    # Try to decode as property sequence
    print(f"\n🔍 Attempting property decode:")
    properties = []
    
    for attempt in range(20):  # Try to read up to 20 properties
        if pos >= len(data) or pos - offset > 100:
            break
        
        # Read property key
        key, consumed = read_varuint(data, pos)
        if consumed == 0 or key > 1000:  # Sanity check
            break
        
        pos += consumed
        
        # Known property keys for KeyFrameDouble:
        # 67 (0x43): frame time (float)
        # 68 (0x44): value (float)
        # 69 (0x45): interpolatorId (uint)
        
        if key in [67, 68]:  # Float properties
            val, fconsumed = read_float(data, pos)
            if val is not None:
                pos += fconsumed
                properties.append((key, val, 'float'))
                print(f"  Property {key}: {val:.4f} (float)")
        elif key == 69:  # Uint property
            val, vconsumed = read_varuint(data, pos)
            pos += vconsumed
            properties.append((key, val, 'uint'))
            print(f"  Property {key}: {val} (uint)")
        else:
            # Try both float and varuint
            val_f, _ = read_float(data, pos)
            val_u, uconsumed = read_varuint(data, pos)
            
            if val_u < 1000:  # Looks like varuint
                pos += uconsumed
                properties.append((key, val_u, 'uint?'))
                print(f"  Property {key}: {val_u} (uint?)")
            elif val_f is not None and -1e6 < val_f < 1e6:  # Looks like float
                pos += 4
                properties.append((key, val_f, 'float?'))
                print(f"  Property {key}: {val_f:.4f} (float?)")
            else:
                break
    
    print(f"\n📊 Summary:")
    print(f"  Properties decoded: {len(properties)}")
    print(f"  Bytes consumed: {pos - offset}")
    
    # Group by keyframe pattern
    if len(properties) >= 3:
        print(f"\n🎯 KeyFrame patterns detected:")
        for i in range(0, len(properties), 3):
            if i + 2 < len(properties):
                p1, p2, p3 = properties[i:i+3]
                print(f"  Frame {i//3}:")
                print(f"    Prop {p1[0]}: {p1[1]} ({p1[2]})")
                print(f"    Prop {p2[0]}: {p2[1]} ({p2[2]})")
                print(f"    Prop {p3[0]}: {p3[1]} ({p3[2]})")
    
    return properties

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <file.riv>")
        sys.exit(1)
    
    with open(sys.argv[1], 'rb') as f:
        data = f.read()
    
    # Known Type 64 offsets from previous analysis
    offsets = [919, 1304, 1892, 2428, 2551, 2731, 3514, 3801, 4804, 4860, 4916]
    
    for offset in offsets[:3]:  # Decode first 3
        if offset < len(data):
            decode_type64_block(data, offset)
    
    print(f"\n{'='*60}")
    print(f"✅ Analysis complete!")
    print(f"{'='*60}")
