#!/usr/bin/env python3
"""
Generate a minimal multi-chunk RIV file for testing.
This implements the same logic as our C++ serializer updates.
"""

import struct
import sys

def write_varuint(data, value):
    """Write variable-length unsigned integer."""
    while True:
        byte = value & 0x7F
        value >>= 7
        if value == 0:
            data.append(byte)
            break
        else:
            data.append(byte | 0x80)

def generate_test_riv():
    """Generate a minimal multi-chunk RIV file."""
    
    data = bytearray()
    
    # Header
    data.extend(b'RIVE')
    
    # Version 7.0
    write_varuint(data, 7)  # major
    write_varuint(data, 0)  # minor
    
    # File ID
    write_varuint(data, 0)
    
    # Property table - include id(3) and parentId(5)
    properties = [3, 5, 7, 8]  # id, parentId, width, height
    for prop in properties:
        write_varuint(data, prop)
    write_varuint(data, 0)  # End properties
    
    # Bitmap - 2 bits per property
    # 4 properties = 1 32-bit word
    # All are uint type (00) 
    bitmap = 0x00000000  # All uint
    data.extend(struct.pack('<I', bitmap))
    
    # CHUNK 1: Empty Asset Pack (typeKey 105)
    write_varuint(data, 105)  # FileAssetContents
    write_varuint(data, 0)     # No properties
    write_varuint(data, 0)     # Asset data length = 0
    write_varuint(data, 0)     # Chunk terminator
    
    # CHUNK 2: Main Scene Graph
    
    # Backboard (typeKey 23)
    write_varuint(data, 23)
    write_varuint(data, 3)     # id property
    write_varuint(data, 1)     # id = 1
    write_varuint(data, 0)     # End properties
    
    # Artboard (typeKey 1)
    write_varuint(data, 1)
    write_varuint(data, 3)     # id property
    write_varuint(data, 2)     # id = 2
    write_varuint(data, 5)     # parentId property
    write_varuint(data, 0)     # parentId = 0 (no parent)
    write_varuint(data, 7)     # width property
    data.extend(struct.pack('<f', 500.0))  # width = 500
    write_varuint(data, 8)     # height property
    data.extend(struct.pack('<f', 400.0))  # height = 400
    write_varuint(data, 0)     # End properties
    
    # Shape (typeKey 3)
    write_varuint(data, 3)
    write_varuint(data, 3)     # id property
    write_varuint(data, 3)     # id = 3
    write_varuint(data, 5)     # parentId property
    write_varuint(data, 0)     # parentId = 0 (artboard at index 0)
    write_varuint(data, 0)     # End properties
    
    # Rectangle (typeKey 7)
    write_varuint(data, 7)
    write_varuint(data, 3)     # id property
    write_varuint(data, 4)     # id = 4
    write_varuint(data, 5)     # parentId property
    write_varuint(data, 1)     # parentId = 1 (shape component index)
    write_varuint(data, 0)     # End properties
    
    # Fill (typeKey 20)
    write_varuint(data, 20)
    write_varuint(data, 3)     # id property
    write_varuint(data, 5)     # id = 5
    write_varuint(data, 5)     # parentId property
    write_varuint(data, 1)     # parentId = 1 (shape component index)
    write_varuint(data, 0)     # End properties
    
    # SolidColor (typeKey 18)
    write_varuint(data, 18)
    write_varuint(data, 3)     # id property
    write_varuint(data, 6)     # id = 6
    write_varuint(data, 5)     # parentId property
    write_varuint(data, 3)     # parentId = 3 (fill component index)
    write_varuint(data, 0)     # End properties
    
    write_varuint(data, 0)     # End main chunk
    
    # CHUNK 3: Artboard Catalog (typeKey 8776)
    write_varuint(data, 8776)  # ArtboardListItem
    write_varuint(data, 3)     # id property
    write_varuint(data, 2)     # Reference to artboard id=2
    write_varuint(data, 0)     # End properties
    write_varuint(data, 0)     # Chunk terminator
    
    # Multiple terminators (as seen in nature.riv)
    for _ in range(4):
        write_varuint(data, 0)
    
    return bytes(data)

def main():
    riv_data = generate_test_riv()
    
    # Write to file
    output_file = "test_multi_chunk.riv"
    with open(output_file, 'wb') as f:
        f.write(riv_data)
    
    print(f"Generated {output_file} ({len(riv_data)} bytes)")
    print(f"Structure:")
    print(f"  - Header with properties [3, 5, 7, 8]")
    print(f"  - Chunk 1: Empty Asset Pack (typeKey 105)")
    print(f"  - Chunk 2: Main Scene (Backboard, Artboard, Shape, Rectangle, Fill, SolidColor)")
    print(f"  - Chunk 3: Artboard Catalog (typeKey 8776)")
    print(f"  - Multiple terminators")
    
    return 0

if __name__ == '__main__':
    sys.exit(main())
