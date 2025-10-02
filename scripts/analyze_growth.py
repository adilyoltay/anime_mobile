#!/usr/bin/env python3
"""Analyze round-trip object count growth"""

import json
import sys
from collections import Counter

def analyze_json(filepath):
    with open(filepath, 'r') as f:
        data = json.load(f)
    
    objects = []
    for artboard in data.get('artboards', []):
        objects.extend(artboard.get('objects', []))
    
    type_counts = Counter(obj.get('typeKey') for obj in objects)
    
    return len(objects), type_counts

def main():
    files = [
        ('C1 (Original)', 'output/roundtrip/stability/bee_baby_c1.json'),
        ('C3 (1 cycle)', 'output/roundtrip/stability/bee_baby_c3.json'),
        ('C5 (2 cycles)', 'output/roundtrip/stability/bee_baby_c5.json'),
    ]
    
    print("╔══════════════════════════════════════════════════════════╗")
    print("║       ROUND-TRIP GROWTH ANALYSIS                        ║")
    print("╚══════════════════════════════════════════════════════════╝")
    print()
    
    results = []
    for label, filepath in files:
        try:
            total, types = analyze_json(filepath)
            results.append((label, total, types))
        except Exception as e:
            print(f"Error reading {filepath}: {e}")
            continue
    
    # Print summary
    print("📊 Total Object Counts:")
    for label, total, _ in results:
        print(f"  {label:20s}: {total:5d} objects")
    print()
    
    # Print growth
    if len(results) >= 2:
        print("📈 Growth:")
        for i in range(1, len(results)):
            prev_total = results[i-1][1]
            curr_total = results[i][1]
            growth = curr_total - prev_total
            pct = (growth / prev_total) * 100 if prev_total > 0 else 0
            print(f"  {results[i-1][0]} → {results[i][0]}: +{growth} objects ({pct:+.1f}%)")
        print()
    
    # Analyze specific types
    interesting_types = {
        28: "CubicInterpolator",
        138: "Interpolator",
        25: "KeyedObject",
        26: "KeyedProperty",
        30: "KeyFrame",
        20: "Fill",
        18: "SolidColor"
    }
    
    print("📋 Key Type Breakdown:")
    print()
    for type_key, type_name in sorted(interesting_types.items()):
        print(f"  {type_name} ({type_key}):")
        has_data = False
        for label, _, types in results:
            count = types.get(type_key, 0)
            if count > 0:
                print(f"    {label:20s}: {count:5d}")
                has_data = True
        if not has_data:
            print(f"    (none)")
        print()
    
    # Identify growth sources
    if len(results) >= 3:
        print("🔍 Growth Sources (C1 → C5):")
        types_c1 = results[0][2]
        types_c5 = results[2][2]
        
        growth_by_type = []
        for type_key in set(types_c1.keys()) | set(types_c5.keys()):
            c1_count = types_c1.get(type_key, 0)
            c5_count = types_c5.get(type_key, 0)
            growth = c5_count - c1_count
            if growth != 0:
                type_name = interesting_types.get(type_key, f"Type{type_key}")
                growth_by_type.append((growth, type_key, type_name, c1_count, c5_count))
        
        growth_by_type.sort(reverse=True)
        for growth, type_key, type_name, c1, c5 in growth_by_type[:10]:
            if growth > 0:
                print(f"  {type_name:20s} ({type_key:3d}): {c1:4d} → {c5:4d} (+{growth})")
        print()

if __name__ == '__main__':
    main()
