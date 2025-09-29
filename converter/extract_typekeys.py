#!/usr/bin/env python3
"""
Extract typeKey mappings from Rive generated headers.
This creates a mapping of typeKey values to class names.
"""

import os
import re
from pathlib import Path
import json

def extract_typekeys(headers_dir):
    """Extract typeKey mappings from generated header files."""
    
    typekey_map = {}
    headers_path = Path(headers_dir)
    
    # Pattern to find typeKey definitions
    typekey_pattern = re.compile(r'static const uint16_t typeKey = (\d+);')
    
    # Pattern to find class names
    class_pattern = re.compile(r'class\s+(\w+Base)\s*:')
    
    # Walk through all header files
    for header_file in headers_path.rglob('*_base.hpp'):
        with open(header_file, 'r') as f:
            content = f.read()
            
            # Find class name
            class_match = class_pattern.search(content)
            if not class_match:
                continue
            
            class_name = class_match.group(1)
            # Remove "Base" suffix
            clean_name = class_name.replace('Base', '')
            
            # Find typeKey
            typekey_match = typekey_pattern.search(content)
            if typekey_match:
                typekey = int(typekey_match.group(1))
                typekey_map[typekey] = {
                    'class': clean_name,
                    'file': str(header_file.relative_to(headers_path))
                }
    
    return typekey_map

def analyze_known_typekeys():
    """Analyze typeKeys from reference RIV files."""
    
    # Known typeKeys from nature.riv analysis
    known_typekeys = {
        23: 'Backboard',
        1: 'Artboard', 
        2: 'ArtboardBase',  # Alternative artboard?
        3: 'Shape',
        4: 'Path',
        5: 'VertexBase',
        6: 'StraightVertex',
        7: 'CubicDetachedVertex',
        13: 'Drawable',
        14: 'Node',
        18: 'SolidColor',
        20: 'Fill',
        24: 'Stroke',
        31: 'LinearGradient',  # Suspected
        52: 'Rectangle',
        56: 'StateMachine',
        99: 'Asset',
        103: 'FileAsset',
        104: 'ImageAsset',
        105: 'FileAssetContents',  # Asset pack
        106: 'FileAssetReference',
        420: 'DrawRules',  # Suspected from hex 0x1A4
        8698: 'Unknown_8698',  # 0x220A - needs identification
        8726: 'ArtboardList',  # 0x2216 - artboard catalog?
        8776: 'ArtboardListItem',  # 0x2248
        1098432: 'Unknown_1098432'  # Large typeKey - special marker?
    }
    
    return known_typekeys

def main():
    # Get the Rive headers directory
    script_dir = Path(__file__).parent
    headers_dir = script_dir.parent / 'include' / 'rive' / 'generated'
    
    print(f"Scanning headers in: {headers_dir}")
    
    # Extract typeKeys from headers
    extracted_map = extract_typekeys(headers_dir)
    
    # Get known typeKeys from RIV analysis
    known_map = analyze_known_typekeys()
    
    # Merge maps
    merged_map = {}
    for key, value in extracted_map.items():
        merged_map[key] = value
    
    # Add known typeKeys not found in headers
    for key, name in known_map.items():
        if key not in merged_map:
            merged_map[key] = {
                'class': name,
                'file': 'unknown',
                'source': 'riv_analysis'
            }
    
    # Sort by typeKey
    sorted_map = dict(sorted(merged_map.items()))
    
    # Output results
    print(f"\nFound {len(sorted_map)} typeKey mappings:")
    print("-" * 60)
    
    for typekey, info in sorted_map.items():
        source = info.get('source', 'header')
        print(f"{typekey:6d} (0x{typekey:04X}) -> {info['class']:30s} [{source}]")
    
    # Save to JSON
    output_file = script_dir / 'typekey_mapping.json'
    with open(output_file, 'w') as f:
        json.dump(sorted_map, f, indent=2)
    
    print(f"\nMapping saved to: {output_file}")
    
    # Check for our mystery typeKeys
    mystery_keys = [8698, 8726, 8776, 420, 31, 1098432]
    print("\nMystery TypeKeys:")
    print("-" * 60)
    for key in mystery_keys:
        if key in sorted_map:
            info = sorted_map[key]
            print(f"{key:6d} (0x{key:04X}) -> {info['class']}")
        else:
            print(f"{key:6d} (0x{key:04X}) -> NOT FOUND")

if __name__ == '__main__':
    main()
