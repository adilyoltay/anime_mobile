#!/usr/bin/env python3
"""
TEST THE PACKER!
Quick prototype in Python to validate encoding hypothesis.
"""

import struct

def write_varuint(value):
    """Encode as Rive varuint."""
    bytes_out = []
    while True:
        byte = value & 0x7F
        value >>= 7
        if value > 0:
            byte |= 0x80
        bytes_out.append(byte)
        if value == 0:
            break
    return bytes(bytes_out)

def write_float(value):
    """Encode as little-endian float."""
    return struct.pack('<f', value)

def pack_keyframes(keyframes):
    """Pack keyframes using discovered pattern."""
    packed = bytearray()
    
    for kf in keyframes:
        frame_int = int(kf['frame'])
        
        # Property 67: frame (as varuint!)
        packed.extend(write_varuint(67))
        packed.extend(write_varuint(frame_int))
        
        # Property 68: value (as float!)
        packed.extend(write_varuint(68))
        packed.extend(write_float(kf['value']))
        
        # Property 69: interpolatorId (if non-zero)
        if kf.get('interpId', 0) != 0:
            packed.extend(write_varuint(69))
            packed.extend(write_varuint(kf['interpId']))
    
    return bytes(packed)

# TEST DATA from bee_baby
test_keyframes = [
    {'frame': 0, 'value': 251.0, 'interpId': 0},
    {'frame': 0, 'value': 116.5, 'interpId': 0}
]

print("🧪 TESTING PACKER")
print("="*70)

packed = pack_keyframes(test_keyframes)

print(f"\nInput keyframes:")
for kf in test_keyframes:
    print(f"  Frame={kf['frame']}, Value={kf['value']}, InterpId={kf.get('interpId', 0)}")

print(f"\nPacked output ({len(packed)} bytes):")
print(f"  Hex: {packed.hex()}")
print(f"  Bytes: {' '.join(f'{b:02x}' for b in packed)}")

# Compare with original pattern (offset 145-158)
print(f"\n🔍 COMPARISON with original (offset 145-158):")
with open('converter/exampleriv/bee_baby.riv', 'rb') as f:
    f.seek(145)
    original = f.read(14)

print(f"  Original: {original.hex()}")
print(f"  Packed:   {packed[:14].hex()}")

# Byte-by-byte comparison
print(f"\n📊 Byte-by-byte:")
for i in range(min(14, len(packed))):
    orig_byte = original[i] if i < len(original) else None
    pack_byte = packed[i]
    match = "✅" if orig_byte == pack_byte else "❌"
    print(f"  [{i:2d}] Original: {orig_byte:02x} | Packed: {pack_byte:02x} {match}")

print(f"\n{'='*70}")
if packed[:14] == original[:14]:
    print("🎉 PERFECT MATCH! Encoding is CORRECT!")
else:
    print("⚠️  Mismatch - need to adjust encoding")
    
    # Analyze differences
    print(f"\n🔧 Debugging:")
    print(f"  Property 67 varuint: {write_varuint(67).hex()}")
    print(f"  Frame 0 varuint: {write_varuint(0).hex()}")
    print(f"  Property 68 varuint: {write_varuint(68).hex()}")
    print(f"  Value 251.0 float: {write_float(251.0).hex()}")
