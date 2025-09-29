#!/usr/bin/env python3
"""
Analyze RIV file chunks and special markers.
Identifies asset packs, artboard catalogs, and other special blocks.
"""

import sys
import struct
import json
from pathlib import Path

def read_varuint(data, offset):
    """Read variable-length unsigned integer."""
    result = 0
    shift = 0
    while offset < len(data):
        byte = data[offset]
        offset += 1
        result |= (byte & 0x7F) << shift
        if (byte & 0x80) == 0:
            break
        shift += 7
    return result, offset

def analyze_chunks(filepath):
    """Analyze RIV file for special chunks and markers."""
    
    with open(filepath, 'rb') as f:
        data = f.read()
    
    # Skip header
    if data[:4] != b'RIVE':
        print(f"Error: Not a valid RIV file")
        return
    
    offset = 4
    
    # Read version
    major, offset = read_varuint(data, offset)
    minor, offset = read_varuint(data, offset)
    print(f"Version: {major}.{minor}")
    
    # Read file ID
    file_id, offset = read_varuint(data, offset)
    print(f"File ID: {file_id}")
    
    # Read property table
    properties = []
    while True:
        prop_key, offset = read_varuint(data, offset)
        if prop_key == 0:
            break
        properties.append(prop_key)
    
    print(f"Property keys in header: {properties}")
    
    # Read bitmap
    num_properties = len(properties)
    bitmap_size = (num_properties + 3) // 4
    bitmap = []
    for i in range(bitmap_size):
        if offset + 4 <= len(data):
            value = struct.unpack('<I', data[offset:offset+4])[0]
            bitmap.append(value)
            offset += 4
    
    print(f"Bitmap: {[hex(b) for b in bitmap]}")
    
    # Now analyze objects and look for special typeKeys
    print("\n" + "="*60)
    print("Object Stream Analysis:")
    print("="*60)
    
    chunks = []
    current_chunk = None
    object_count = 0
    
    while offset < len(data):
        # Try to read typeKey
        if offset + 2 > len(data):
            break
            
        start_offset = offset
        typekey, offset = read_varuint(data, offset)
        
        if typekey == 0:
            # End of chunk or file
            if current_chunk:
                current_chunk['end_offset'] = offset
                chunks.append(current_chunk)
                current_chunk = None
            
            # Check if there's more data (another chunk)
            if offset < len(data):
                print(f"\n--- Chunk boundary at offset {offset} ---")
                # Try to read next chunk header
                continue
            else:
                break
        
        # Check for special typeKeys that indicate chunk starts
        if typekey in [105, 106, 8698, 8726, 8776, 420, 1098432]:
            if current_chunk:
                current_chunk['end_offset'] = start_offset
                chunks.append(current_chunk)
            
            current_chunk = {
                'typekey': typekey,
                'start_offset': start_offset,
                'objects': []
            }
            
            print(f"\n*** SPECIAL CHUNK: typeKey {typekey} (0x{typekey:04X}) at offset {start_offset}")
        
        # Track object
        if current_chunk:
            current_chunk['objects'].append({
                'typekey': typekey,
                'offset': start_offset
            })
        
        object_count += 1
        
        # For special typeKeys, try to read additional info
        if typekey == 105:  # FileAssetContents - Asset pack
            print("  -> Asset Pack detected")
            # Read asset data length
            if offset < len(data):
                asset_len, offset = read_varuint(data, offset)
                print(f"     Asset data length: {asset_len} bytes")
                offset += asset_len  # Skip asset data
        
        elif typekey == 8726:  # Suspected ArtboardList
            print("  -> Artboard Catalog detected")
        
        elif typekey == 420:  # LayoutComponentStyle
            print("  -> Layout Component Style detected")
        
        elif typekey == 1098432:
            print("  -> Large typeKey marker (possibly end-of-section)")
        
        # Skip properties for this object (simplified)
        # In a real parser, we'd properly read all properties
        while offset < len(data):
            prop_key, next_offset = read_varuint(data, offset)
            if prop_key == 0:
                offset = next_offset
                break
            
            # Skip property value (simplified - would need proper type handling)
            offset = min(next_offset + 10, len(data))
            if offset >= len(data) - 10:
                break
    
    # Final chunk
    if current_chunk:
        current_chunk['end_offset'] = len(data)
        chunks.append(current_chunk)
    
    print(f"\nTotal objects found: {object_count}")
    print(f"Total special chunks: {len(chunks)}")
    
    # Summary
    print("\n" + "="*60)
    print("Chunk Summary:")
    print("="*60)
    
    for i, chunk in enumerate(chunks):
        size = chunk['end_offset'] - chunk['start_offset']
        print(f"Chunk {i+1}: typeKey {chunk['typekey']} (0x{chunk['typekey']:04X})")
        print(f"  Offset: {chunk['start_offset']} - {chunk['end_offset']} ({size} bytes)")
        print(f"  Objects in chunk: {len(chunk['objects'])}")
    
    return chunks

def main():
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <file.riv>")
        sys.exit(1)
    
    filepath = Path(sys.argv[1])
    if not filepath.exists():
        print(f"Error: File not found: {filepath}")
        sys.exit(1)
    
    analyze_chunks(filepath)

if __name__ == '__main__':
    main()
