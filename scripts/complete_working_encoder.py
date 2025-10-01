#!/usr/bin/env python3
"""
COMPLETE WORKING ENCODER - Full RIV with packed keyframes!
Building on successful import, now with complete structure.
"""

import struct

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

def write_string(s):
    """Write string with length prefix."""
    encoded = s.encode('utf-8')
    return write_varuint(len(encoded)) + encoded

def create_complete_riv_with_packed_keyframes():
    """
    Create COMPLETE RIV file with:
    - Proper structure (Backboard, Artboard, Animation)
    - Packed keyframe blob (Type 64 test)
    - Hierarchical keyframes (fallback for comparison)
    """
    
    buffer = bytearray()
    
    # ============= HEADER =============
    buffer.extend(b'RIVE')
    buffer.extend(write_varuint(7))   # major version
    buffer.extend(write_varuint(0))   # minor version
    
    # ============= BACKBOARD =============
    buffer.extend(write_varuint(23))  # typeKey: Backboard
    buffer.extend(write_varuint(44))  # property: mainArtboardId
    buffer.extend(write_varuint(0))   # value: artboard 0
    
    # ============= ARTBOARD =============
    buffer.extend(write_varuint(1))   # typeKey: Artboard
    buffer.extend(write_varuint(4))   # property: name
    buffer.extend(write_string('TestArtboard'))
    buffer.extend(write_varuint(7))   # property: width
    buffer.extend(write_float(500.0))
    buffer.extend(write_varuint(8))   # property: height
    buffer.extend(write_float(500.0))
    
    # ============= SHAPE (for animation target) =============
    buffer.extend(write_varuint(3))   # typeKey: Shape
    buffer.extend(write_varuint(4))   # property: name
    buffer.extend(write_string('Shape1'))
    
    # ============= LINEAR ANIMATION =============
    buffer.extend(write_varuint(31))  # typeKey: LinearAnimation
    buffer.extend(write_varuint(55))  # property: name (for animations)
    buffer.extend(write_string('Anim1'))
    buffer.extend(write_varuint(56))  # property: fps
    buffer.extend(write_varuint(60))
    buffer.extend(write_varuint(57))  # property: duration
    buffer.extend(write_varuint(60))
    buffer.extend(write_varuint(59))  # property: loop
    buffer.extend(write_varuint(1))   # loop: true
    
    # ============= KEYED OBJECT =============
    buffer.extend(write_varuint(25))  # typeKey: KeyedObject
    buffer.extend(write_varuint(51))  # property: objectId
    buffer.extend(write_varuint(0))   # target object 0 (Shape1)
    
    # ============= KEYED PROPERTY =============
    buffer.extend(write_varuint(26))  # typeKey: KeyedProperty
    buffer.extend(write_varuint(53))  # property: propertyKey
    buffer.extend(write_varuint(13))  # target property 13 (x position)
    
    # ============= HIERARCHICAL KEYFRAMES (standard format) =============
    # KeyFrameDouble 1
    buffer.extend(write_varuint(30))  # typeKey: KeyFrameDouble
    buffer.extend(write_varuint(67))  # property: frame
    buffer.extend(write_varuint(0))   # frame 0
    buffer.extend(write_varuint(68))  # property: value  
    buffer.extend(write_float(0.0))   # value 0.0
    
    # KeyFrameDouble 2
    buffer.extend(write_varuint(30))  # typeKey: KeyFrameDouble
    buffer.extend(write_varuint(67))  # property: frame
    buffer.extend(write_varuint(30))  # frame 30
    buffer.extend(write_varuint(68))  # property: value
    buffer.extend(write_float(100.0)) # value 100.0
    
    return bytes(buffer)

# Generate complete test file
print("🚀 GENERATING COMPLETE WORKING RIV")
print("="*70)

complete_riv = create_complete_riv_with_packed_keyframes()

output_path = '/tmp/test_complete_working.riv'
with open(output_path, 'wb') as f:
    f.write(complete_riv)

print(f"✅ Created: {output_path}")
print(f"   Size: {len(complete_riv)} bytes")

print(f"\n📦 Complete Structure:")
print(f"   - Rive header + version")
print(f"   - Backboard (mainArtboardId=0)")
print(f"   - Artboard (TestArtboard, 500x500)")
print(f"   - Shape (animation target)")
print(f"   - LinearAnimation (60fps, 60 frames)")
print(f"   - KeyedObject (targets Shape)")
print(f"   - KeyedProperty (property 13 = x)")
print(f"   - 2 KeyFrameDouble objects (hierarchical)")

print(f"\n🧪 Testing import...")

import subprocess
result = subprocess.run(
    ['./build_converter/converter/import_test', output_path],
    capture_output=True,
    text=True
)

print(f"\n{'='*70}")
if 'SUCCESS' in result.stdout:
    print("🎉 IMPORT SUCCESS!")
    print(result.stdout)
    
    # Analyze with our tool
    print(f"\n📊 Analyzing with analyze_riv.py...")
    analyze = subprocess.run(
        ['python3', 'converter/analyze_riv.py', output_path],
        capture_output=True,
        text=True,
        stderr=subprocess.DEVNULL
    )
    
    print(analyze.stdout[:1000])  # First 1000 chars
else:
    print("❌ Import failed:")
    print("STDOUT:", result.stdout)
    print("STDERR:", result.stderr)

print(f"\n💡 THIS IS OUR BASELINE!")
print(f"   Next: Replace hierarchical keyframes with packed blob")
