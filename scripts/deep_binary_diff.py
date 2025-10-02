#!/usr/bin/env python3
"""
Deep binary diff: Find EXACTLY what's in original but missing in ours.
Focus on chunks AFTER main object stream.
"""

import struct

def read_varuint(data, offset):
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

def find_object_stream_end(data):
    """Find where main object stream ends (after 3rd terminator)."""
    pos = 6  # After RIVE header + version
    
    terminator_count = 0
    while pos < len(data):
        typeKey, consumed = read_varuint(data, pos)
        if consumed == 0:
            break
        
        if typeKey == 0:
            terminator_count += 1
            if terminator_count >= 3:
                return pos + consumed
        
        pos += consumed
        # Skip properties (rough estimate)
        for _ in range(20):
            if pos >= len(data):
                break
            val, consumed = read_varuint(data, pos)
            pos += consumed
            if val == 0:
                break
    
    return pos

def analyze_tail_chunks(filepath):
    """Analyze chunks after main object stream."""
    
    with open(filepath, 'rb') as f:
        data = f.read()
    
    print(f"📁 {filepath}")
    print(f"   Size: {len(data)} bytes")
    
    # Find stream end
    stream_end = find_object_stream_end(data)
    print(f"   Stream ends ~{stream_end}")
    
    # Analyze tail
    tail = data[stream_end:]
    print(f"   Tail: {len(tail)} bytes after stream")
    
    # Find type keys in tail
    type_keys = []
    pos = stream_end
    
    for i in range(50):  # Check first 50 potential type keys
        if pos >= len(data):
            break
        
        typeKey, consumed = read_varuint(data, pos)
        if consumed == 0:
            break
        
        type_keys.append((pos, typeKey))
        pos += consumed + 10  # Skip ahead
    
    print(f"   Type keys in tail: {[tk for _, tk in type_keys[:10]]}")
    
    return {
        'size': len(data),
        'stream_end': stream_end,
        'tail_size': len(tail),
        'type_keys': type_keys,
        'tail_hex': tail[:200].hex()
    }

# Analyze both files
print("🔍 DEEP BINARY COMPARISON")
print("="*70)
print()

orig = analyze_tail_chunks('converter/exampleriv/bee_baby.riv')
print()
rt = analyze_tail_chunks('output/bee_baby_new.riv')

print()
print("="*70)
print("📊 COMPARISON")
print("="*70)

print(f"\nOriginal tail size: {orig['tail_size']} bytes")
print(f"Round-trip tail size: {rt['tail_size']} bytes")
print(f"Difference: {orig['tail_size'] - rt['tail_size']:+d} bytes")

print(f"\nOriginal type keys in tail:")
for pos, tk in orig['type_keys'][:10]:
    print(f"  @{pos}: {tk}")

print(f"\nRound-trip type keys in tail:")
for pos, tk in rt['type_keys'][:10]:
    print(f"  @{pos}: {tk}")

# Find missing type keys
orig_keys = set(tk for _, tk in orig['type_keys'])
rt_keys = set(tk for _, tk in rt['type_keys'])

missing = orig_keys - rt_keys
extra = rt_keys - orig_keys

if missing:
    print(f"\n⚠️  MISSING type keys in round-trip: {missing}")
    print(f"   These may be CRITICAL for rendering!")

if extra:
    print(f"\n⚠️  EXTRA type keys in round-trip: {extra}")

print(f"\n📄 Tail hex preview (first 200 bytes):")
print(f"\nOriginal:")
print(f"  {orig['tail_hex'][:100]}")
print(f"  {orig['tail_hex'][100:200]}")

print(f"\nRound-trip:")
print(f"  {rt['tail_hex'][:100]}")
print(f"  {rt['tail_hex'][100:200]}")
