#!/usr/bin/env python3
"""
SESSION 4: Decode the packed keyframe sequence!

We know:
- Value at offset 150: 251.0 (bytes: 00007b43)
- Value at offset 155: 116.5 (bytes: 0000e942)
- These are sequential keyframes!

Goal: Understand bytes BETWEEN 150-155 and BEFORE 150
"""

import sys
import struct

def hexdump(data, start_offset=0, length=None):
    """Pretty hex dump."""
    if length:
        data = data[:length]
    
    for i in range(0, len(data), 16):
        offset = start_offset + i
        chunk = data[i:i+16]
        hex_str = ' '.join(f'{b:02x}' for b in chunk)
        ascii_str = ''.join(chr(b) if 32 <= b < 127 else '.' for b in chunk)
        print(f"  {offset:04x}: {hex_str:<48} {ascii_str}")

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

def main():
    with open('converter/exampleriv/bee_baby.riv', 'rb') as f:
        data = f.read()
    
    print(f"🔬 SESSION 4: Decoding Packed Keyframe Sequence")
    print(f"="*70)
    
    # Known value locations
    val1_offset = 150  # 251.0
    val2_offset = 155  # 116.5
    
    # Show large context
    print(f"\n📍 Context around keyframe values (offsets 130-180)")
    print(f"{'─'*70}")
    hexdump(data[130:180], start_offset=130)
    
    # Verify values
    print(f"\n✅ Verification:")
    val1 = struct.unpack('<f', data[val1_offset:val1_offset+4])[0]
    val2 = struct.unpack('<f', data[val2_offset:val2_offset+4])[0]
    print(f"  Offset {val1_offset}: {val1:.6f} (expected: 251.0)")
    print(f"  Offset {val2_offset}: {val2:.6f} (expected: 116.5)")
    
    # Analyze structure BEFORE first value
    print(f"\n🔍 Analysis before first value (offset 130-149):")
    print(f"{'─'*70}")
    
    chunk = data[130:val1_offset]
    print(f"Raw bytes: {chunk.hex()}")
    print(f"Length: {len(chunk)} bytes")
    
    # Try to parse as varuints
    print(f"\n🔢 Attempting varuint parse:")
    pos = 130
    while pos < val1_offset:
        val, consumed = read_varuint(data, pos)
        if consumed == 0:
            break
        print(f"  Offset {pos}: varuint = {val:6d} (consumed {consumed} bytes, hex: {data[pos:pos+consumed].hex()})")
        pos += consumed
        if pos >= val1_offset:
            break
    
    # Analyze BETWEEN values (should contain frame encoding!)
    print(f"\n🎯 CRITICAL: Between values (offset 154-154)")
    print(f"{'─'*70}")
    
    between = data[154:155]  # Just 1 byte!
    print(f"Byte: 0x{between[0]:02x} = {between[0]} decimal")
    print(f"\nHypothesis: This could be:")
    print(f"  - Frame number for first keyframe (Frame=0)")
    print(f"  - Property key/marker")
    print(f"  - Separator")
    
    # What's before val2?
    print(f"\n🔍 Before second value (offset 154-154):")
    # Same byte!
    
    # Look for pattern
    print(f"\n📊 Pattern Analysis:")
    print(f"  Value1 at {val1_offset}: 4 bytes (float)")
    print(f"  Gap: 1 byte (0x{data[154]:02x})")
    print(f"  Value2 at {val2_offset}: 4 bytes (float)")
    
    print(f"\n💡 Structure Hypothesis:")
    print(f"  [... metadata ...] [value1 float] [??] [value2 float] ...")
    
    # Try to find frame encoding
    # Frame values we know: 0, 0, 25, 25, 14, 23, 25, 5, 30, 5
    print(f"\n🔢 Looking for frame values (0, 0, 25, ...):")
    
    # Bytes before val1
    pre_val1 = data[145:150]
    print(f"  5 bytes before val1: {pre_val1.hex()} = {list(pre_val1)}")
    
    # Check if any byte is 0 (Frame=0)
    if 0 in pre_val1:
        idx = pre_val1.index(0)
        print(f"  ✅ Found 0x00 at offset {145+idx} (could be Frame=0!)")
    
    # Check for 25 (0x19)
    if 25 in pre_val1:
        idx = pre_val1.index(25)
        print(f"  ✅ Found 0x19 (25) at offset {145+idx} (could be Frame=25!)")

if __name__ == '__main__':
    main()
