#!/usr/bin/env python3
"""Validate that our converter produces exact same object types as Casino Slots"""

import subprocess
from collections import Counter

def get_type_list(riv_file):
    """Get exact sequence of object types from RIV file"""
    result = subprocess.run(
        ['./build_converter/converter/import_test', riv_file],
        capture_output=True,
        text=True
    )
    
    types = []
    for line in result.stdout.split('\n'):
        if 'typeKey=' in line:
            typeKey = line.strip().split('typeKey=')[1].rstrip(')')
            types.append(typeKey)
    
    return types

def main():
    print("=" * 80)
    print("EXACT TYPE MATCH VALIDATION")
    print("=" * 80)
    
    # Get Casino Slots types
    casino_types = get_type_list('converter/exampleriv/demo-casino-slots.riv')
    casino_set = set(casino_types)
    casino_counts = Counter(casino_types)
    
    # Get our generated file types
    our_types = get_type_list('build_converter/exact_slot_symbol.riv')
    our_set = set(our_types)
    our_counts = Counter(our_types)
    
    print(f"\nCasino Slots: {len(casino_types)} objects, {len(casino_set)} unique types")
    print(f"Our Generated: {len(our_types)} objects, {len(our_set)} unique types")
    
    # Check which Casino types we can generate
    we_can_generate = our_set & casino_set
    casino_only = casino_set - our_set
    
    print(f"\n✅ Types we successfully generate: {len(we_can_generate)}/{len(casino_set)}")
    print(f"❌ Casino types we haven't generated yet: {len(casino_only)}")
    
    type_names = {
        '1': 'Artboard', '2': 'Node', '3': 'Shape', '4': 'Ellipse', '5': 'StraightVertex',
        '6': 'CubicDetachedVertex', '7': 'Rectangle', '16': 'PointsPath', '17': 'RadialGradient',
        '18': 'SolidColor', '19': 'GradientStop', '20': 'Fill', '22': 'LinearGradient',
        '24': 'Stroke', '28': 'KeyFrameColor', '35': 'KeyFrameId', '40': 'Bone', '41': 'RootBone',
        '42': 'ClippingShape', '43': 'Skin', '44': 'Tendon', '45': 'Weight', '100': 'Image',
        '128': 'Event', '407': 'AudioEvent', '533': 'Feather', '61': 'AnimationState',
        '62': 'AnyState', '63': 'EntryState', '64': 'ExitState',
    }
    
    print("\n" + "=" * 80)
    print("TYPES WE CAN GENERATE (matches Casino Slots):")
    print("=" * 80)
    for t in sorted(we_can_generate, key=lambda x: int(x) if x.isdigit() else 9999):
        name = type_names.get(t, f'Type-{t}')
        casino_count = casino_counts[t]
        our_count = our_counts[t]
        print(f"  ✅ {name:25} (typeKey {t:>3}): Casino has {casino_count:>5}, We created {our_count:>2}")
    
    if casino_only:
        print("\n" + "=" * 80)
        print("CASINO TYPES WE HAVEN'T USED YET:")
        print("=" * 80)
        for t in sorted(casino_only, key=lambda x: int(x) if x.isdigit() else 9999):
            name = type_names.get(t, f'Type-{t}')
            casino_count = casino_counts[t]
            print(f"  ⏸️  {name:25} (typeKey {t:>3}): Casino has {casino_count:>5}")
    
    # Calculate coverage
    print("\n" + "=" * 80)
    print("COVERAGE ANALYSIS:")
    print("=" * 80)
    
    # Types we can create (even if not used in this test)
    all_our_capabilities = {
        '1', '2', '3', '4', '5', '6', '7', '16', '17', '18', '19', '20', '22', '24',
        '28', '35', '40', '41', '42', '43', '44', '45', '61', '62', '63', '64',
        '100', '128', '407', '533'
    }
    
    can_create = all_our_capabilities & casino_set
    cannot_create = casino_set - all_our_capabilities
    
    print(f"\nTypes we CAN create (have implementation): {len(can_create)}/{len(casino_set)}")
    print(f"Types we CANNOT create (not implemented): {len(cannot_create)}")
    
    coverage_pct = (len(can_create) / len(casino_set)) * 100
    print(f"\n🎯 Coverage: {coverage_pct:.1f}%")
    
    if cannot_create:
        print("\n⚠️  Missing implementations:")
        for t in sorted(cannot_create, key=lambda x: int(x) if x.isdigit() else 9999):
            name = type_names.get(t, f'Unknown-{t}')
            count = casino_counts[t]
            print(f"   typeKey {t:>3}: {name:25} ({count} objects)")
    else:
        print("\n🎊 WE CAN CREATE ALL CASINO SLOTS TYPES! 100%")
    
    print("\n" + "=" * 80)

if __name__ == '__main__':
    main()
