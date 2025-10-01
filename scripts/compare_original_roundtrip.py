#!/usr/bin/env python3
"""
Compare original bee_baby.riv with round-trip version.
Identify packed format types that disappear in round-trip.
"""

import subprocess
import re

def count_types(text):
    """Count occurrences of each type in analyzer output."""
    types = {}
    for match in re.finditer(r'type_(\d+)', text):
        t = int(match.group(1))
        types[t] = types.get(t, 0) + 1
    return types

# Analyze original
print("Analyzing original...")
orig = subprocess.check_output(
    ['python3', 'converter/analyze_riv.py', 'converter/exampleriv/bee_baby.riv'], 
    stderr=subprocess.DEVNULL
).decode()

# Convert round-trip
print("Converting round-trip...")
subprocess.call(
    ['./build_converter/converter/rive_convert_cli', 
     'output/bee_baby_WITH_TARGETID.json', 
     '/tmp/roundtrip_compare.riv'], 
    stdout=subprocess.DEVNULL, 
    stderr=subprocess.DEVNULL
)

# Analyze round-trip
print("Analyzing round-trip...")
rt = subprocess.check_output(
    ['python3', 'converter/analyze_riv.py', '/tmp/roundtrip_compare.riv'], 
    stderr=subprocess.DEVNULL
).decode()

orig_types = count_types(orig)
rt_types = count_types(rt)

print('\n' + '='*60)
print('TYPE COMPARISON: Original vs Round-Trip')
print('='*60)

# Types only in original (these are the packed formats!)
only_orig = set(orig_types.keys()) - set(rt_types.keys())
print(f'\n📦 PACKED FORMATS (only in original):')
for t in sorted(only_orig):
    print(f'  Type {t:5d}: {orig_types[t]:3d} occurrences')

# Types only in round-trip (these are the expanded formats!)
only_rt = set(rt_types.keys()) - set(orig_types.keys())
print(f'\n📤 EXPANDED FORMATS (only in round-trip):')
for t in sorted(only_rt):
    if t < 1000:  # Skip noise/errors
        print(f'  Type {t:5d}: {rt_types[t]:3d} occurrences')

# Common types with different counts
print(f'\n📊 CHANGED COUNTS:')
common = set(orig_types.keys()) & set(rt_types.keys())
for t in sorted(common):
    if orig_types[t] != rt_types[t] and t < 1000:
        diff = rt_types[t] - orig_types[t]
        sign = '+' if diff > 0 else ''
        print(f'  Type {t:5d}: {orig_types[t]:3d} → {rt_types[t]:3d} ({sign}{diff})')

print('\n' + '='*60)
print('✅ Comparison complete!')
print('='*60)
