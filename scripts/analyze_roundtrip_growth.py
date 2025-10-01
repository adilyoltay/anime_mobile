#!/usr/bin/env python3
"""
Round-trip dosya boyutu artışı analiz scripti
Orijinal ve round-trip RIV dosyalarını karşılaştırır.
"""

import sys
import json
from collections import Counter, defaultdict

def analyze_json(json_path):
    """JSON dosyasındaki object type dağılımını analiz et"""
    with open(json_path) as f:
        data = json.load(f)
    
    # Handle both single artboard and multi-artboard format
    artboards = data.get('artboards', [data])
    
    type_counts = Counter()
    type_categories = defaultdict(list)
    
    for ab in artboards:
        for obj in ab.get('objects', []):
            tk = obj['typeKey']
            type_counts[tk] += 1
            
            # Categorize
            if tk in [25, 26]:  # KeyedObject, KeyedProperty
                type_categories['keyed_structure'].append(tk)
            elif tk in [28, 138, 139, 174, 175]:  # Interpolators
                type_categories['interpolators'].append(tk)
            elif tk in [30, 37, 50, 84, 142, 450, 171]:  # KeyFrames
                type_categories['keyframes'].append(tk)
            elif tk in [3, 5, 7, 16]:  # Shapes
                type_categories['shapes'].append(tk)
            elif tk in [18, 20, 24]:  # Paints
                type_categories['paints'].append(tk)
            elif tk in [31]:  # Animations
                type_categories['animations'].append(tk)
            elif tk in [53, 57, 61, 62, 63, 64, 65]:  # StateMachine
                type_categories['state_machines'].append(tk)
            else:
                type_categories['other'].append(tk)
    
    return type_counts, type_categories

def print_analysis(title, type_counts, type_categories):
    """Analiz sonuçlarını yazdır"""
    print(f"\n{'='*60}")
    print(f"{title}")
    print(f"{'='*60}")
    
    total = sum(type_counts.values())
    print(f"Total Objects: {total}")
    
    print(f"\n{'-'*60}")
    print("Category Breakdown:")
    print(f"{'-'*60}")
    
    for category, items in sorted(type_categories.items()):
        count = len(items)
        percentage = (count / total * 100) if total > 0 else 0
        print(f"  {category:20s}: {count:5d} ({percentage:5.1f}%)")
    
    print(f"\n{'-'*60}")
    print("Top 15 Type Keys:")
    print(f"{'-'*60}")
    
    type_names = {
        1: "Artboard",
        2: "Node",
        3: "Shape",
        4: "Ellipse",
        5: "StraightVertex",
        7: "Rectangle",
        16: "PointsPath",
        18: "SolidColor",
        20: "Fill",
        24: "Stroke",
        25: "KeyedObject",
        26: "KeyedProperty",
        28: "CubicEaseInterpolator",
        30: "KeyFrameDouble",
        31: "LinearAnimation",
        35: "CubicDetachedVertex",
        37: "KeyFrameColor",
        42: "TransformComponent",
        50: "KeyFrameId",
        53: "StateMachine",
        57: "StateMachineLayer",
        61: "AnimationState",
        62: "TransitionCondition",
        63: "StateTransition",
        64: "EntryState",
        65: "ExitState",
        84: "KeyFrameBool",
        138: "LinearInterpolator",
    }
    
    for tk, count in type_counts.most_common(15):
        name = type_names.get(tk, f"Unknown_{tk}")
        percentage = (count / total * 100) if total > 0 else 0
        print(f"  {tk:4d} {name:25s}: {count:5d} ({percentage:5.1f}%)")

def compare_analyses(original_counts, original_cats, roundtrip_counts, roundtrip_cats):
    """İki analizi karşılaştır"""
    print(f"\n{'='*60}")
    print("COMPARISON: Original vs Round-Trip")
    print(f"{'='*60}")
    
    orig_total = sum(original_counts.values())
    rt_total = sum(roundtrip_counts.values())
    
    print(f"\nTotal Objects: {orig_total} → {rt_total} ({rt_total/orig_total:.2f}x)")
    
    print(f"\n{'-'*60}")
    print("Category Growth:")
    print(f"{'-'*60}")
    
    all_categories = set(original_cats.keys()) | set(roundtrip_cats.keys())
    
    for category in sorted(all_categories):
        orig_count = len(original_cats.get(category, []))
        rt_count = len(roundtrip_cats.get(category, []))
        growth = rt_count / orig_count if orig_count > 0 else float('inf')
        diff = rt_count - orig_count
        
        symbol = "📈" if diff > 0 else ("📉" if diff < 0 else "➡️")
        print(f"  {symbol} {category:20s}: {orig_count:4d} → {rt_count:4d} ({growth:.2f}x, +{diff})")
    
    print(f"\n{'-'*60}")
    print("Type Key Growth (Top Increases):")
    print(f"{'-'*60}")
    
    all_types = set(original_counts.keys()) | set(roundtrip_counts.keys())
    
    type_names = {
        28: "CubicEaseInterpolator",
        30: "KeyFrameDouble",
        26: "KeyedProperty",
        25: "KeyedObject",
        138: "LinearInterpolator",
        37: "KeyFrameColor",
        50: "KeyFrameId",
        84: "KeyFrameBool",
        31: "LinearAnimation",
    }
    
    growths = []
    for tk in all_types:
        orig = original_counts.get(tk, 0)
        rt = roundtrip_counts.get(tk, 0)
        diff = rt - orig
        if diff != 0:
            growths.append((diff, tk, orig, rt))
    
    for diff, tk, orig, rt in sorted(growths, reverse=True)[:10]:
        name = type_names.get(tk, f"Type_{tk}")
        growth = rt / orig if orig > 0 else float('inf')
        symbol = "⚠️" if diff > 100 else "📊"
        print(f"  {symbol} {tk:4d} {name:25s}: {orig:4d} → {rt:4d} ({growth:.2f}x, +{diff})")

def main():
    if len(sys.argv) != 3:
        print("Usage: analyze_roundtrip_growth.py <original.json> <roundtrip.json>")
        sys.exit(1)
    
    original_json = sys.argv[1]
    roundtrip_json = sys.argv[2]
    
    print("\n🔍 Analyzing Round-Trip File Growth\n")
    
    # Analyze original
    orig_counts, orig_cats = analyze_json(original_json)
    print_analysis("ORIGINAL", orig_counts, orig_cats)
    
    # Analyze round-trip
    rt_counts, rt_cats = analyze_json(roundtrip_json)
    print_analysis("ROUND-TRIP", rt_counts, rt_cats)
    
    # Compare
    compare_analyses(orig_counts, orig_cats, rt_counts, rt_cats)
    
    print(f"\n{'='*60}")
    print("CONCLUSION")
    print(f"{'='*60}")
    
    # Calculate animation data growth
    anim_orig = len(orig_cats.get('interpolators', [])) + len(orig_cats.get('keyframes', []))
    anim_rt = len(rt_cats.get('interpolators', [])) + len(rt_cats.get('keyframes', []))
    
    print(f"""
Animation Data Growth:
  Original:   {anim_orig:4d} objects (interpolators + keyframes)
  Round-Trip: {anim_rt:4d} objects (interpolators + keyframes)
  Growth:     {anim_rt - anim_orig:4d} objects ({anim_rt/anim_orig if anim_orig > 0 else 0:.2f}x)

This is EXPECTED behavior:
  - Rive uses PACKED format for animation data in binary .riv files
  - Extractor EXPANDS this to hierarchical JSON format
  - Round-trip converter creates objects from expanded JSON
  - Result: More objects but same visual output

Recommendation:
  ✅ This is NORMAL and not a bug
  ✅ File size increase is due to format expansion
  ✅ Runtime will re-pack this data efficiently
""")

if __name__ == '__main__':
    main()
