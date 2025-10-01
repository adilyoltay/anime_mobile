#!/usr/bin/env python3
"""
Analyze chunks in RIV file - find what comes after object stream.
Looking for drawable order / work area chunks!
"""

import struct
import sys

def read_varuint(data, offset):
    """Read Rive varuint."""
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

def analyze_file_chunks(filepath):
    """Analyze RIV file structure, especially after object stream."""
    
    with open(filepath, 'rb') as f:
        data = f.read()
    
    print(f"🔍 ANALYZING: {filepath}")
    print(f"Size: {len(data)} bytes")
    print("="*70)
    
    # Skip header
    if data[:4] != b'RIVE':
        print("❌ Not a RIVE file!")
        return
    
    pos = 4
    
    # Read version
    major, consumed = read_varuint(data, pos)
    pos += consumed
    minor, consumed = read_varuint(data, pos)
    pos += consumed
    
    print(f"\n📦 Header:")
    print(f"  Version: {major}.{minor}")
    print(f"  Header ends at: {pos}")
    
    # Track object stream
    print(f"\n📊 Object Stream:")
    
    object_count = 0
    terminators = []
    chunks_after_stream = []
    
    in_object_stream = True
    last_object_end = pos
    
    while pos < len(data):
        # Read type key
        typeKey, consumed = read_varuint(data, pos)
        
        if consumed == 0:
            break
        
        type_start = pos
        pos += consumed
        
        if typeKey == 0:
            # Terminator
            terminators.append(type_start)
            print(f"  [Terminator at {type_start}] Object count so far: {object_count}")
            
            # After a few terminators, we're likely past object stream
            if len(terminators) >= 3:
                in_object_stream = False
                last_object_end = pos
                
                # What comes next?
                if pos < len(data):
                    print(f"\n🎯 AFTER OBJECT STREAM (offset {pos}):")
                    
                    # Read next few type keys
                    temp_pos = pos
                    for i in range(10):
                        if temp_pos >= len(data):
                            break
                        
                        next_type, next_consumed = read_varuint(data, temp_pos)
                        if next_consumed == 0:
                            break
                        
                        chunk_start = temp_pos
                        temp_pos += next_consumed
                        
                        # Try to read some properties
                        props = []
                        for _ in range(5):
                            if temp_pos >= len(data):
                                break
                            
                            key, key_consumed = read_varuint(data, temp_pos)
                            if key_consumed == 0 or key == 0:
                                break
                            
                            temp_pos += key_consumed
                            val, val_consumed = read_varuint(data, temp_pos)
                            temp_pos += val_consumed
                            
                            props.append((key, val))
                        
                        chunk_info = {
                            'offset': chunk_start,
                            'typeKey': next_type,
                            'properties': props
                        }
                        chunks_after_stream.append(chunk_info)
                        
                        print(f"  Chunk {i+1}:")
                        print(f"    Offset: {chunk_start}")
                        print(f"    Type: {next_type}")
                        if props:
                            print(f"    Properties: {props[:3]}")
                        
                        # Skip ahead
                        temp_pos += 20
                
                break
        
        object_count += 1
        
        # Skip rest of object (rough estimate)
        pos += 20
    
    print(f"\n📋 Summary:")
    print(f"  Total objects: {object_count}")
    print(f"  Terminators found: {len(terminators)}")
    print(f"  Chunks after stream: {len(chunks_after_stream)}")
    
    if chunks_after_stream:
        print(f"\n🎯 KEY FINDING - Chunks after object stream:")
        for chunk in chunks_after_stream[:5]:
            print(f"  Type {chunk['typeKey']} at offset {chunk['offset']}")
    
    return chunks_after_stream

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <file.riv>")
        sys.exit(1)
    
    original_chunks = analyze_file_chunks(sys.argv[1])
    
    if len(sys.argv) >= 3:
        print(f"\n" + "="*70)
        roundtrip_chunks = analyze_file_chunks(sys.argv[2])
        
        print(f"\n" + "="*70)
        print(f"🔍 COMPARISON:")
        print(f"="*70)
        
        orig_types = [c['typeKey'] for c in original_chunks]
        rt_types = [c['typeKey'] for c in roundtrip_chunks]
        
        print(f"\nOriginal chunks: {orig_types[:5]}")
        print(f"Round-trip chunks: {rt_types[:5]}")
        
        missing = set(orig_types) - set(rt_types)
        if missing:
            print(f"\n⚠️  MISSING in round-trip: {missing}")
            print(f"   These chunks may be causing grey screen!")
