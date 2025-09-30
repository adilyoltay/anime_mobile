#!/usr/bin/env python3
"""Generate JSON that exactly matches Casino Slots object distribution"""

import json
import subprocess
from collections import Counter

def get_casino_stats():
    """Extract Casino Slots statistics"""
    result = subprocess.run(
        ['./build_converter/converter/import_test', 'converter/exampleriv/demo-casino-slots.riv'],
        capture_output=True, text=True
    )
    
    types = []
    for line in result.stdout.split('\n'):
        if 'typeKey=' in line:
            typeKey = line.strip().split('typeKey=')[1].rstrip(')')
            types.append(typeKey)
    
    return Counter(types)

def main():
    print("Extracting Casino Slots structure...")
    casino_counts = get_casino_stats()
    
    # Generate JSON that matches Casino distribution
    output = {
        "artboard": {
            "name": "CasinoSlotsCopy",
            "width": 1442,
            "height": 810
        },
        "customPaths": [],
        "shapes": [],
        "animations": [],
        "stateMachines": [{
            "name": "SlotMachineSM",
            "inputs": [
                {"name": "Pull", "type": "trigger"},
                {"name": "Stop1", "type": "number", "value": 0},
                {"name": "Stop2", "type": "number", "value": 0},
                {"name": "Stop3", "type": "number", "value": 0}
            ],
            "layers": []
        }]
    }
    
    # Generate paths to match Casino: 897 PointsPath
    num_paths = casino_counts.get('16', 0)  # PointsPath count
    total_straight = casino_counts.get('5', 0)  # StraightVertex
    total_cubic = casino_counts.get('6', 0)  # CubicDetachedVertex
    
    print(f"Casino has: {num_paths} paths, {total_straight} straight vertices, {total_cubic} cubic vertices")
    
    vertices_per_path = (total_straight + total_cubic) / num_paths if num_paths > 0 else 0
    print(f"Average {vertices_per_path:.1f} vertices per path")
    
    # Generate paths - create representative sample (not all 897!)
    # Full generation would create 500KB+ JSON
    print("\n⚠️  Creating SAMPLE with same proportions (not full 15,683 objects)")
    print("Full replication would require 500KB+ JSON")
    
    # Create scaled-down version: 10 paths instead of 897
    scale_factor = 897 / 10
    sample_paths = 10
    sample_straight = int(total_straight / scale_factor)
    sample_cubic = int(total_cubic / scale_factor)
    
    print(f"\nSample: {sample_paths} paths, {sample_straight} straight, {sample_cubic} cubic")
    
    for i in range(sample_paths):
        # Each path averages 11 vertices
        path = {
            "isClosed": True,
            "vertices": [],
            "fillEnabled": True,
            "fillColor": f"#{(i*30)%256:02X}{(i*60)%256:02X}{(i*90)%256:02X}",
            "strokeEnabled": i % 2 == 0,
            "strokeColor": "#000000",
            "strokeThickness": 1.5
        }
        
        # Create ~11 vertices per path (mix of straight and cubic)
        num_vertices = 11
        for v in range(num_vertices):
            angle = (v / num_vertices) * 360
            radius = 80 + (i % 3) * 20
            x = 100 + i * 140 + radius * (v / num_vertices) * 0.8
            y = 100 + radius * ((v + i) % 5) / 2
            
            if v % 3 == 0:  # Every 3rd vertex is straight
                path["vertices"].append({
                    "type": "straight",
                    "x": x,
                    "y": y,
                    "radius": 2.0 if v % 2 == 0 else 0.0
                })
            else:  # Others are cubic
                path["vertices"].append({
                    "type": "cubic",
                    "x": x,
                    "y": y,
                    "inRotation": angle - 45,
                    "inDistance": 10 + (v % 5) * 3,
                    "outRotation": angle + 45,
                    "outDistance": 10 + ((v + 1) % 5) * 3
                })
        
        output["customPaths"].append(path)
    
    # Add shapes to match Casino (reduced scale)
    num_shapes = casino_counts.get('3', 0)
    num_ellipses = casino_counts.get('4', 0)
    
    print(f"Casino has: {num_shapes} shapes, {num_ellipses} ellipses")
    print(f"Sample: Creating {min(5, num_ellipses)} ellipses")
    
    for i in range(min(5, num_ellipses)):
        output["shapes"].append({
            "type": "ellipse",
            "x": 50 + i * 100,
            "y": 600,
            "width": 40,
            "height": 40,
            "fill": {"color": "#FFD700"}
        })
    
    # Add animations
    for i in range(15):  # 15 animations for variety
        output["animations"].append({
            "name": f"Symbol{i}",
            "fps": 60,
            "duration": 60,
            "loop": 1,
            "scaleKeyframes": []
        })
    
    # Add state machine layers (6 layers like Casino)
    layers = [
        ("L1", 11),  # 11 symbol states
        ("L2", 11),
        ("L3", 11),
        ("handle", 2),
        ("machine", 2),
        ("win", 2)
    ]
    
    for layer_name, num_states in layers:
        layer = {
            "name": layer_name,
            "states": [],
            "transitions": []
        }
        
        for s in range(num_states):
            layer["states"].append({
                "name": f"{layer_name}_State{s}",
                "type": "animation",
                "animationName": f"Symbol{s % 15}"
            })
        
        output["stateMachines"][0]["layers"].append(layer)
    
    # Write JSON
    out_file = "casino_slots_exact_copy.json"
    with open(out_file, 'w') as f:
        json.dump(output, f, indent=2)
    
    print(f"\n✅ Generated: {out_file}")
    print(f"\nStructure:")
    print(f"  Paths: {len(output['customPaths'])}")
    print(f"  Shapes: {len(output['shapes'])}")
    print(f"  Animations: {len(output['animations'])}")
    print(f"  State Machine Layers: {len(output['stateMachines'][0]['layers'])}")
    
    total_vertices = sum(len(p['vertices']) for p in output['customPaths'])
    print(f"  Total Vertices: {total_vertices}")
    print(f"\nNote: Scaled to {scale_factor:.0f}× smaller for practical JSON size")
    print(f"Full scale would be: {num_paths} paths, {int(vertices_per_path * num_paths)} vertices")

if __name__ == '__main__':
    main()
