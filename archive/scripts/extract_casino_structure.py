#!/usr/bin/env python3
"""Extract exact object structure from Casino Slots for replication"""

import subprocess
from collections import Counter, defaultdict

def main():
    riv_file = 'converter/exampleriv/demo-casino-slots.riv'
    
    result = subprocess.run(
        ['./build_converter/converter/import_test', riv_file],
        capture_output=True,
        text=True
    )
    
    lines = result.stdout.split('\n')
    
    # Extract exact object sequence
    objects = []
    for line in lines:
        if 'typeKey=' in line:
            typeKey = line.strip().split('typeKey=')[1].rstrip(')')
            objects.append(typeKey)
    
    # Type name mapping
    type_names = {
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
        '40': 'Bone',
        '41': 'RootBone',
        '42': 'ClippingShape',
        '43': 'Skin',
        '44': 'Tendon',
        '45': 'Weight',
        '100': 'Image',
        '128': 'Event',
        '407': 'AudioEvent',
        '533': 'Feather',
        '61': 'AnimationState',
        '62': 'AnyState',
        '63': 'EntryState',
        '64': 'ExitState',
    }
    
    print("=" * 80)
    print("CASINO SLOTS - EXACT OBJECT STRUCTURE")
    print("=" * 80)
    print(f"\nTotal Objects: {len(objects)}")
    
    # Count by type
    type_counts = Counter(objects)
    print("\nObject Distribution:")
    for typeKey, count in sorted(type_counts.items(), key=lambda x: -x[1]):
        name = type_names.get(typeKey, f'Unknown-{typeKey}')
        print(f"  {name:25} (typeKey {typeKey:>3}): {count:>6} objects")
    
    # Generate Python code to create JSON
    print("\n" + "=" * 80)
    print("GENERATION TEMPLATE")
    print("=" * 80)
    print("\nTo recreate Casino Slots, we need:")
    
    # Group by category
    categories = defaultdict(list)
    for typeKey, count in type_counts.items():
        name = type_names.get(typeKey, f'Unknown-{typeKey}')
        if typeKey in ['5', '6', '16']:
            categories['Path Vertices'].append((name, typeKey, count))
        elif typeKey in ['19', '17', '22']:
            categories['Gradients'].append((name, typeKey, count))
        elif typeKey in ['18', '20', '24']:
            categories['Paint'].append((name, typeKey, count))
        elif typeKey in ['61', '62', '63', '64']:
            categories['States'].append((name, typeKey, count))
        elif typeKey in ['128', '407']:
            categories['Events'].append((name, typeKey, count))
        elif typeKey in ['40', '41', '43', '44', '45']:
            categories['Bones'].append((name, typeKey, count))
        else:
            categories['Other'].append((name, typeKey, count))
    
    for category, items in sorted(categories.items()):
        print(f"\n{category}:")
        for name, typeKey, count in sorted(items, key=lambda x: -x[2]):
            print(f"  {count:>6}× {name}")
    
    # Calculate complexity
    print("\n" + "=" * 80)
    print("REPLICATION STRATEGY")
    print("=" * 80)
    
    path_vertices = type_counts.get('5', 0) + type_counts.get('6', 0)
    paths = type_counts.get('16', 0)
    
    print(f"\nTo match Casino Slots exactly:")
    print(f"  - Create {paths} PointsPath containers")
    print(f"  - Add {path_vertices} vertices total")
    print(f"    - {type_counts.get('5', 0)} StraightVertex")
    print(f"    - {type_counts.get('6', 0)} CubicDetachedVertex")
    print(f"  - Average {path_vertices/paths if paths > 0 else 0:.1f} vertices per path")
    print(f"\nThis represents the slot symbols (cherry, diamond, seven, etc.)")
    print("Each symbol is a complex Bezier path with 10-15 vertices")

if __name__ == '__main__':
    main()
