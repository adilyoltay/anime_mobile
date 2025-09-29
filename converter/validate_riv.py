#!/usr/bin/env python3
"""
Validate RIV file structure, especially multi-chunk format.
"""

import struct
import sys
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

def validate_riv(filepath):
    """Validate RIV file structure."""
    
    with open(filepath, 'rb') as f:
        data = f.read()
    
    print(f"File: {filepath}")
    print(f"Size: {len(data)} bytes")
    print("="*60)
    
    # Check header
    if data[:4] != b'RIVE':
        print("❌ Invalid magic number")
        return False
    
    print("✅ Valid RIVE header")
    offset = 4
    
    # Version
    major, offset = read_varuint(data, offset)
    minor, offset = read_varuint(data, offset)
    print(f"✅ Version: {major}.{minor}")
    
    # File ID
    file_id, offset = read_varuint(data, offset)
    print(f"✅ File ID: {file_id}")
    
    # Property table
    properties = []
    while True:
        prop_key, offset = read_varuint(data, offset)
        if prop_key == 0:
            break
        properties.append(prop_key)
    
    print(f"✅ Property keys: {properties}")
    
    # Check for required keys
    if 3 not in properties:
        print("⚠️  Warning: Missing property key 3 (id)")
    if 5 not in properties:
        print("⚠️  Warning: Missing property key 5 (parentId)")
    
    # Bitmap
    num_properties = len(properties)
    bitmap_size = (num_properties + 3) // 4
    print(f"✅ Bitmap size: {bitmap_size} words for {num_properties} properties")
    
    # Skip bitmap
    offset += bitmap_size * 4
    
    # Check for chunks
    print("\nChunk Analysis:")
    print("-"*60)
    
    chunk_count = 0
    found_asset_pack = False
    found_artboard = False
    found_catalog = False
    
    while offset < len(data):
        start_offset = offset
        typekey, offset = read_varuint(data, offset)
        
        if typekey == 0:
            print(f"  Chunk terminator at offset {start_offset}")
            continue
        
        chunk_count += 1
        
        if typekey == 105:  # FileAssetContents
            print(f"✅ Chunk {chunk_count}: Asset Pack (typeKey 105) at offset {start_offset}")
            found_asset_pack = True
        elif typekey == 23:  # Backboard
            print(f"✅ Chunk {chunk_count}: Main Scene starts (Backboard) at offset {start_offset}")
        elif typekey == 1:  # Artboard
            print(f"  - Artboard found")
            found_artboard = True
        elif typekey == 8776:  # ArtboardListItem
            print(f"✅ Chunk {chunk_count}: Artboard Catalog (typeKey 8776) at offset {start_offset}")
            found_catalog = True
        elif typekey > 1000:
            print(f"⚠️  Large typeKey {typekey} at offset {start_offset}")
        
        # Skip to next object (simplified)
        if offset >= len(data) - 2:
            break
    
    print("\n" + "="*60)
    print("Validation Summary:")
    print("-"*60)
    
    success = True
    
    if found_asset_pack:
        print("✅ Asset Pack chunk present (required by Rive Play)")
    else:
        print("❌ Missing Asset Pack chunk")
        success = False
    
    if found_artboard:
        print("✅ Artboard present")
    else:
        print("❌ Missing Artboard")
        success = False
    
    if found_catalog:
        print("✅ Artboard Catalog present (required by Rive Play)")
    else:
        print("⚠️  Missing Artboard Catalog (may cause issues in Rive Play)")
    
    if success:
        print("\n🎉 File structure appears valid for Rive Play!")
    else:
        print("\n❌ File may not work in Rive Play")
    
    return success

def main():
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <file.riv>")
        sys.exit(1)
    
    filepath = Path(sys.argv[1])
    if not filepath.exists():
        print(f"Error: File not found: {filepath}")
        sys.exit(1)
    
    success = validate_riv(filepath)
    sys.exit(0 if success else 1)

if __name__ == '__main__':
    main()
