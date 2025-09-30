#!/usr/bin/env python3
"""Detailed analysis of Casino Slots RIV file"""

import subprocess
import sys
from collections import Counter

def run_import_test(riv_file):
    """Run import test and capture output"""
    result = subprocess.run(
        ['./build_converter/converter/import_test', riv_file],
        capture_output=True,
        text=True
    )
    return result.stdout

def main():
    riv_file = 'converter/exampleriv/demo-casino-slots.riv'
    
    print("=" * 80)
    print("CASINO SLOTS DETAILED ANALYSIS")
    print("=" * 80)
    
    output = run_import_test(riv_file)
    
    # Count object types
    type_counts = Counter()
    for line in output.split('\n'):
        if 'typeKey=' in line:
            typeKey = line.split('typeKey=')[1].strip()
            type_counts[typeKey] += 1
    
    print("\n📊 OBJECT TYPE DISTRIBUTION (Top 30):")
    print("-" * 80)
    print(f"{'TypeKey':<10} {'Count':<10} {'Known As':<30}")
    print("-" * 80)
    
    # Known types mapping
    known_types = {
        '1': 'Artboard',
        '2': 'Node',
        '3': 'Shape',
        '4': 'Ellipse',
        '5': 'StraightVertex',
        '6': 'CubicDetachedVertex',
        '7': 'Rectangle',
        '16': 'PointsPath',
        '17': 'RadialGradient',
        '18': 'SolidColor',
        '19': 'GradientStop',
        '20': 'Fill',
        '22': 'LinearGradient',
        '24': 'Stroke',
        '28': 'KeyFrameColor',
        '35': 'KeyFrameId',
        '41': 'Bone',
        '42': 'ClippingShape',
        '44': 'RootBone',
        '45': 'Skin',
        '100': 'Image',
        '128': 'Event',
        '407': 'AudioEvent',
        '533': 'Feather',
    }
    
    for typeKey, count in sorted(type_counts.items(), key=lambda x: -x[1])[:30]:
        known_as = known_types.get(typeKey, '??? UNKNOWN')
        print(f"{typeKey:<10} {count:<10} {known_as:<30}")
    
    print("\n" + "=" * 80)
    print("📋 FEATURE ANALYSIS:")
    print("=" * 80)
    
    # Feature detection
    features = {
        'Custom Paths (Vertices)': int(type_counts.get('6', 0)) + int(type_counts.get('5', 0)),
        'PointsPath Containers': int(type_counts.get('16', 0)),
        'Events': int(type_counts.get('128', 0)),
        'Audio Events': int(type_counts.get('407', 0)),
        'Shapes': int(type_counts.get('3', 0)),
        'Fills': int(type_counts.get('20', 0)),
        'Strokes': int(type_counts.get('24', 0)),
        'Gradients': int(type_counts.get('17', 0)) + int(type_counts.get('22', 0)),
        'Images': int(type_counts.get('100', 0)),
        'ClippingShapes': int(type_counts.get('42', 0)),
        'Bones': int(type_counts.get('41', 0)),
        'Skin': int(type_counts.get('45', 0)),
        'Feather Effects': int(type_counts.get('533', 0)),
    }
    
    for feature, count in features.items():
        status = '✅ SUPPORTED' if count > 0 and feature in ['Shapes', 'Fills', 'Strokes', 'Gradients', 'Images', 'ClippingShapes', 'Feather Effects'] else '❌ NOT SUPPORTED' if count > 0 else '   N/A'
        print(f"{feature:<30} {count:>6} {status}")
    
    print("\n" + "=" * 80)
    print("🎯 IMPLEMENTATION PRIORITY:")
    print("=" * 80)
    
    # Priority calculation
    priorities = []
    if features['Custom Paths (Vertices)'] > 0:
        priorities.append(('Custom Path Vertices', features['Custom Paths (Vertices)'], 'HIGH'))
    if features['Events'] > 0:
        priorities.append(('Event System', features['Events'], 'MEDIUM'))
    if features['Audio Events'] > 0:
        priorities.append(('Audio Events', features['Audio Events'], 'LOW'))
    if features['Bones'] > 0:
        priorities.append(('Bones & Skinning', features['Bones'], 'MEDIUM'))
    
    for feature, count, priority in priorities:
        print(f"{priority:<10} {feature:<30} ({count} objects)")
    
    print("\n" + "=" * 80)

if __name__ == '__main__':
    main()
