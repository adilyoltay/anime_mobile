#!/usr/bin/env python3
"""
MINIMAL WORKING ENCODER V1
Strategy: Build simplest possible packed format, TEST IT!
"""

import struct
import json

def write_varuint(value):
    """Rive varuint encoding."""
    result = []
    while True:
        byte = value & 0x7F
        value >>= 7
        if value > 0:
            byte |= 0x80
        result.append(byte)
        if value == 0:
            break
    return bytes(result)

def write_float(value):
    """IEEE 754 little-endian float."""
    return struct.pack('<f', value)

def create_minimal_packed_riv():
    """
    Create MINIMAL test RIV with ONE packed keyframe blob!
    
    Based on discoveries:
    - Type 64 (0x40) for packed keyframes
    - Float values stored as IEEE 754
    - Some property encoding (exact format TBD)
    """
    
    buffer = bytearray()
    
    # Rive header
    buffer.extend(b'RIVE')
    
    # Version (major=7, minor=0)
    buffer.extend(write_varuint(7))   # major version
    buffer.extend(write_varuint(0))   # minor version
    
    # Backboard (typeKey 23)
    buffer.extend(write_varuint(23))
    # Backboard properties (minimal)
    buffer.extend(write_varuint(44))  # property: mainArtboardId
    buffer.extend(write_varuint(0))   # value: 0
    
    # Artboard (typeKey 1)
    buffer.extend(write_varuint(1))
    buffer.extend(write_varuint(4))   # property: name
    buffer.extend(write_varuint(4))   # string length
    buffer.extend(b'Test')            # name
    buffer.extend(write_varuint(7))   # property: width
    buffer.extend(write_float(500.0))
    buffer.extend(write_varuint(8))   # property: height
    buffer.extend(write_float(500.0))
    
    # LinearAnimation (typeKey 31)
    buffer.extend(write_varuint(31))
    buffer.extend(write_varuint(4))   # property: name
    buffer.extend(write_varuint(4))
    buffer.extend(b'Anim')
    buffer.extend(write_varuint(56))  # property: fps
    buffer.extend(write_varuint(60))
    buffer.extend(write_varuint(57))  # property: duration
    buffer.extend(write_varuint(60))
    
    # KeyedProperty (typeKey 26)
    buffer.extend(write_varuint(26))
    buffer.extend(write_varuint(53))  # property: propertyKey
    buffer.extend(write_varuint(13))  # target property 13 (x position?)
    
    # NOW: PACKED KEYFRAME BLOB!
    # Type 64 - our custom packed format
    buffer.extend(bytes([0x40]))  # Type 64 marker
    
    # TEST: Just write 2 float values (our discovered pattern!)
    # Property 0x0d (13)
    buffer.extend(bytes([0x0d]))
    buffer.extend(write_float(251.0))
    
    # Property 0x0e (14)  
    buffer.extend(bytes([0x0e]))
    buffer.extend(write_float(116.5))
    
    return bytes(buffer)

# Generate test file
print("🔧 GENERATING MINIMAL TEST RIV")
print("="*70)

test_riv = create_minimal_packed_riv()

output_path = '/tmp/test_packed_minimal.riv'
with open(output_path, 'wb') as f:
    f.write(test_riv)

print(f"✅ Created: {output_path}")
print(f"   Size: {len(test_riv)} bytes")
print(f"\n📦 Structure:")
print(f"   - Rive header (4 bytes)")
print(f"   - Backboard (minimal)")
print(f"   - Artboard (500x500)")
print(f"   - LinearAnimation")
print(f"   - KeyedProperty")
print(f"   - PACKED BLOB (Type 64) ← THE TEST!")
print(f"\n🧪 Now testing import...")

# Test with import_test
import subprocess
result = subprocess.run(
    ['./build_converter/converter/import_test', output_path],
    capture_output=True,
    text=True
)

print(f"\n{'='*70}")
if result.returncode == 0:
    print("🎉 IMPORT SUCCESS!")
    print(result.stdout)
else:
    print("❌ Import failed (expected - we're iterating!)")
    print("STDOUT:", result.stdout[-500:] if len(result.stdout) > 500 else result.stdout)
    print("STDERR:", result.stderr[-500:] if len(result.stderr) > 500 else result.stderr)

print(f"\n💡 Next steps:")
print(f"   1. Analyze failure (if any)")
print(f"   2. Adjust encoding")
print(f"   3. Repeat until SUCCESS!")
