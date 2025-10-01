#!/usr/bin/env python3
"""
Generate test subset JSON files from full bee_baby extraction
Used by CI workflow to create 189/190/273 object test files
"""

import json
import sys

def generate_subsets(input_file, output_dir='output'):
    """Generate test subsets from full extraction"""
    # Load full file
    with open(input_file, 'r') as f:
        data = json.load(f)
    
    # Create subsets
    for count in [189, 190, 273]:
        subset = data.copy()
        subset['artboards'][0]['objects'] = data['artboards'][0]['objects'][:count]
        subset['artboards'] = [subset['artboards'][0]]
        
        output_file = f'{output_dir}/test_{count}_no_trim.json'
        with open(output_file, 'w') as f:
            json.dump(subset, f, indent=2)
        
        print(f'Created {output_file} ({count} objects)')

if __name__ == '__main__':
    input_file = sys.argv[1] if len(sys.argv) > 1 else 'output/bee_baby_NO_TRIMPATH.json'
    generate_subsets(input_file)
