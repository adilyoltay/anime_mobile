#!/usr/bin/env python3
"""
BREAKTHROUGH APPROACH: Binary diff between original and round-trip.

Strategy:
1. Extract EXPANDED keyframes from round-trip RIV
2. Find PACKED equivalents in original RIV
3. Compare byte-by-byte to understand encoding
4. Reverse engineer the packing algorithm

This is the key to cracking the format!
"""

import sys
import struct
import json
from collections import defaultdict

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

def extract_keyframes_expanded(riv_file):
    """Extract all KeyFrameDouble objects from expanded RIV."""
    with open(riv_file, 'rb') as f:
        data = f.read()
    
    keyframes = []
    pos = 0
    
    # Type 30 = KeyFrameDouble (varuint: 0x1E = 30)
    while pos < len(data):
        if pos + 1 >= len(data):
            break
        
        # Read type
        typeKey, consumed = read_varuint(data, pos)
        if consumed == 0:
            pos += 1
            continue
        
        if typeKey == 30:  # KeyFrameDouble found!
            kf_start = pos
            pos += consumed
            
            # Read properties
            properties = {}
            for _ in range(20):  # Max 20 properties
                if pos >= len(data):
                    break
                
                prop_key, pk_consumed = read_varuint(data, pos)
                if pk_consumed == 0 or prop_key > 1000:
                    break
                
                pos += pk_consumed
                
                # Property 67 (frame) and 68 (value) are floats
                if prop_key in [67, 68]:
                    if pos + 4 > len(data):
                        break
                    val = struct.unpack('<f', data[pos:pos+4])[0]
                    pos += 4
                    properties[prop_key] = val
                elif prop_key == 69:  # interpolatorId (uint)
                    val, val_consumed = read_varuint(data, pos)
                    pos += val_consumed
                    properties[prop_key] = val
                else:
                    # Unknown property type, try varuint
                    val, val_consumed = read_varuint(data, pos)
                    if val_consumed > 0:
                        pos += val_consumed
                        properties[prop_key] = val
                    else:
                        break
            
            if 67 in properties:  # Valid keyframe
                keyframes.append({
                    'offset': kf_start,
                    'frame': properties.get(67, 0),
                    'value': properties.get(68, 0),
                    'interpId': properties.get(69, 0),
                    'raw_bytes': data[kf_start:pos].hex()
                })
        else:
            pos += 1
    
    return keyframes

def find_keyframe_sequences_in_packed(original_riv, keyframe_data):
    """
    Search for keyframe data patterns in original (packed) RIV.
    Key insight: Float values should appear SOMEWHERE in packed format!
    """
    with open(original_riv, 'rb') as f:
        data = f.read()
    
    # Search for each keyframe's float values
    matches = []
    
    for kf in keyframe_data[:10]:  # First 10 keyframes
        frame_val = kf['frame']
        value_val = kf['value']
        
        # Convert to bytes
        frame_bytes = struct.pack('<f', frame_val)
        value_bytes = struct.pack('<f', value_val)
        
        # Find in original
        frame_pos = data.find(frame_bytes)
        value_pos = data.find(value_bytes)
        
        if frame_pos >= 0 and value_pos >= 0:
            matches.append({
                'kf': kf,
                'frame_pos': frame_pos,
                'value_pos': value_pos,
                'distance': abs(value_pos - frame_pos)
            })
    
    return matches

def analyze_packed_structure(original_riv, matches):
    """Analyze the structure around matched float locations."""
    with open(original_riv, 'rb') as f:
        data = f.read()
    
    print(f"\n{'='*70}")
    print(f"🔍 PACKED STRUCTURE ANALYSIS")
    print(f"{'='*70}")
    
    for i, match in enumerate(matches[:3]):
        print(f"\n{'─'*70}")
        print(f"Match #{i+1}")
        print(f"{'─'*70}")
        
        kf = match['kf']
        print(f"KeyFrame data:")
        print(f"  Frame: {kf['frame']:.6f}")
        print(f"  Value: {kf['value']:.6f}")
        print(f"  InterpId: {kf['interpId']}")
        
        frame_pos = match['frame_pos']
        value_pos = match['value_pos']
        
        print(f"\nLocations in packed format:")
        print(f"  Frame at offset {frame_pos}")
        print(f"  Value at offset {value_pos}")
        print(f"  Distance: {match['distance']} bytes")
        
        # Show context (32 bytes before and after frame)
        start = max(0, frame_pos - 16)
        end = min(len(data), frame_pos + 32)
        context = data[start:end]
        
        print(f"\nContext around frame value:")
        for j in range(0, len(context), 16):
            offset = start + j
            hex_str = ' '.join(f'{b:02x}' for b in context[j:j+16])
            
            # Highlight frame bytes
            if frame_pos >= offset and frame_pos < offset + 16:
                mark = ' ' * ((frame_pos - offset) * 3) + '^^^^'
                print(f"  {offset:04x}: {hex_str}")
                print(f"        {mark} frame")
            else:
                print(f"  {offset:04x}: {hex_str}")

def main():
    print(f"🔬 BINARY DIFF ANALYZER - The Breakthrough Approach!")
    print(f"="*70)
    
    # Step 1: Extract expanded keyframes
    print(f"\n[1/4] Extracting KeyFrames from round-trip (expanded)...")
    rt_keyframes = extract_keyframes_expanded('/tmp/roundtrip_compare.riv')
    print(f"✅ Found {len(rt_keyframes)} KeyFrameDouble objects")
    
    if rt_keyframes:
        print(f"\nFirst 5 keyframes:")
        for i, kf in enumerate(rt_keyframes[:5]):
            print(f"  {i+1}. Frame={kf['frame']:.4f}, Value={kf['value']:.4f}, InterpId={kf['interpId']}")
    
    # Step 2: Find patterns in original (packed)
    print(f"\n[2/4] Searching for patterns in original (packed)...")
    matches = find_keyframe_sequences_in_packed('converter/exampleriv/bee_baby.riv', rt_keyframes)
    print(f"✅ Found {len(matches)} matches")
    
    # Step 3: Analyze structure
    print(f"\n[3/4] Analyzing packed structure...")
    analyze_packed_structure('converter/exampleriv/bee_baby.riv', matches)
    
    # Step 4: Save results
    print(f"\n[4/4] Saving analysis...")
    with open('binary_diff_analysis.json', 'w') as f:
        json.dump({
            'expanded_keyframes': rt_keyframes[:20],
            'matches': matches[:10]
        }, f, indent=2)
    
    print(f"\n{'='*70}")
    print(f"✅ ANALYSIS COMPLETE!")
    print(f"{'='*70}")
    print(f"\n💾 Results saved to: binary_diff_analysis.json")
    
    return rt_keyframes, matches

if __name__ == '__main__':
    keyframes, matches = main()
