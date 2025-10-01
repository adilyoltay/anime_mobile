#!/usr/bin/env python3
"""
FINAL DECODE - Type 64 complete structure!
We're at offset 150 with values. Let's find Type 64 START!
"""

import struct

with open('converter/exampleriv/bee_baby.riv', 'rb') as f:
    data = f.read()

print("🎯 FINAL DECODE - Finding Type 64 Structure")
print("="*70)

# We know offset 150 has our data
# Type 64 = 0x40 in decimal
# Let's scan backwards from 150

print("\n🔍 Scanning backwards from offset 150 for Type 64 marker (0x40)...")

for i in range(149, 100, -1):
    if data[i] == 0x40:  # Type 64
        print(f"\n✅ FOUND Type 64 marker at offset {i}!")
        
        # Show full structure
        start = i
        end = 160  # Past our known values
        
        chunk = data[start:end]
        print(f"\n📦 Type 64 Structure (offset {start}-{end-1}):")
        print("="*70)
        
        for j in range(0, len(chunk), 16):
            offset = start + j
            line = chunk[j:j+16]
            hex_str = ' '.join(f'{b:02x}' for b in line)
            
            # Annotate known offsets
            annotations = []
            for k, byte in enumerate(line):
                abs_offset = offset + k
                if abs_offset == 150:
                    annotations.append((k, "VALUE1 START"))
                elif abs_offset == 155:
                    annotations.append((k, "VALUE2 START"))
            
            print(f"  {offset:04x}: {hex_str}")
            for k, ann in annotations:
                print(f"        {' '*k*3}^^^^ {ann}")
        
        # Parse structure
        print(f"\n🔬 PARSING:")
        pos = start
        print(f"  [{pos-start:3d}] Offset {pos}: 0x{data[pos]:02x} = Type 64 marker")
        pos += 1
        
        # Read until we hit our known value at 150
        while pos < 150:
            byte = data[pos]
            print(f"  [{pos-start:3d}] Offset {pos}: 0x{byte:02x} = {byte:3d} dec")
            pos += 1
        
        print(f"\n  [{pos-start:3d}] Offset {pos}: VALUE1 (251.0) = {data[pos:pos+4].hex()}")
        pos += 4
        
        byte = data[pos]
        print(f"  [{pos-start:3d}] Offset {pos}: 0x{byte:02x} = {byte:3d} dec (separator/property)")
        pos += 1
        
        print(f"  [{pos-start:3d}] Offset {pos}: VALUE2 (116.5) = {data[pos:pos+4].hex()}")
        
        break

# Now let's understand the pattern
print(f"\n" + "="*70)
print(f"💡 STRUCTURE HYPOTHESIS:")
print(f"="*70)

# Let's check if there are multiple Type 64 blobs
print(f"\n📊 All Type 64 markers in first 500 bytes:")
count = 0
for i in range(500):
    if data[i] == 0x40:
        count += 1
        # Check context - is this really a type marker?
        if i > 0:
            prev = data[i-1]
            next_bytes = data[i+1:i+5].hex()
            print(f"  Offset {i:4d}: 0x40 (prev=0x{prev:02x}, next={next_bytes})")

print(f"\nTotal 0x40 bytes found: {count}")

# The one we found is likely the real one!
print(f"\n🎯 CONCLUSION:")
print(f"  Type 64 starts at offset ~145 or before")
print(f"  Contains float values for keyframes")
print(f"  Uses property encoding (0x0d, 0x0e)")
