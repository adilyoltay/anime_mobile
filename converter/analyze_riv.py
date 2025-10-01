#!/usr/bin/env python3
"""
PR4: Robust .riv analyzer with catalog support and graceful EOF handling.
Inspects .riv binaries: prints property keys per object type using
Rive runtime metadata (generated headers).
"""
from __future__ import annotations

import argparse
import json
import re
import struct
import sys
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, Iterable, List, Tuple, Any, DefaultDict, Optional

RIVE_ROOT = Path(__file__).resolve().parents[1]
GENERATED_DIR = RIVE_ROOT / "include" / "rive" / "generated"

# -- Metadata extraction -----------------------------------------------------

def build_property_key_maps() -> Tuple[Dict[str, int], DefaultDict[int, List[str]]]:
    name_to_value: Dict[str, int] = {}
    value_to_names: DefaultDict[int, List[str]] = defaultdict(list)
    pattern = re.compile(r"static const uint16_t\\s+([\\w:]+)\\s*=\\s*(\\d+);")
    for header in GENERATED_DIR.rglob("*.hpp"):
        text = header.read_text(encoding="utf-8")
        for name, value in pattern.findall(text):
            base_name = name.split("::")[-1]
            if not base_name.endswith("PropertyKey"):
                continue
            numeric = int(value)
            name_to_value[name] = numeric
            value_to_names[numeric].append(base_name)
    return name_to_value, value_to_names


def build_property_type_map(key_map: Dict[str, int]) -> Dict[int, str]:
    result: Dict[int, str] = {}
    case_pattern = re.compile(
        r"case\\s+(\\w+)\\s*:\\s*(.*?)return\\s+true;",
        re.DOTALL,
    )
    for header in GENERATED_DIR.rglob("*.hpp"):
        text = header.read_text(encoding="utf-8")
        for prop_name, body in case_pattern.findall(text):
            match = re.search(r"Core(\\w+)Type::deserialize", body)
            if not match:
                continue
            core_type = match.group(1)
            key_value = key_map.get(prop_name)
            if key_value is not None:
                result[key_value] = core_type
    return result

def build_type_name_map() -> Dict[int, str]:
    result: Dict[int, str] = {}
    class_pattern = re.compile(
        r"class\\s+(\\w+)Base\\s*:\\s*public\\s+[^\\{]+\\{\\s*public:\\s*static const uint16_t typeKey = (\\d+);",
        re.DOTALL,
    )
    for header in GENERATED_DIR.rglob("*.hpp"):
        text = header.read_text(encoding="utf-8")
        for class_name, value in class_pattern.findall(text):
            result[int(value)] = class_name.replace("Base", "")
    return result

NAME_TO_VALUE, VALUE_TO_NAMES = build_property_key_maps()
PROPERTY_CORE_TYPE = build_property_type_map(NAME_TO_VALUE)
TYPE_KEY_NAME = build_type_name_map()

# Mapping from CoreXXXType to readable category
CORE_TO_CATEGORY = {
    "Uint": "uint",
    "Double": "double",
    "String": "string",
    "Color": "color",
    "Bool": "bool",
    "Bytes": "bytes",
    "Callback": "callback",
}

CATEGORY_DECODERS = {
    "uint": lambda data, pos: read_varuint(data, pos),
    "bool": lambda data, pos: read_varuint(data, pos),
    "string": lambda data, pos: read_string(data, pos),
    "double": lambda data, pos: (struct.unpack_from("<f", data, pos)[0], pos + 4),
    "color": lambda data, pos: (struct.unpack_from("<I", data, pos)[0], pos + 4),
    "bytes": lambda data, pos: read_bytes(data, pos),
    # Fallback for callback/unknown -> treat as uint
}

# -- Binary helper functions -------------------------------------------------

def read_varuint(data: bytes, pos: int) -> Tuple[int, int]:
    result = 0
    shift = 0
    while True:
        if pos >= len(data):
            raise EOFError("Unexpected EOF while reading varuint")
        byte = data[pos]
        pos += 1
        result |= (byte & 0x7F) << shift
        if byte & 0x80:
            shift += 7
            continue
        return result, pos

def read_string(data: bytes, pos: int) -> Tuple[str, int]:
    length, pos = read_varuint(data, pos)
    end = pos + length
    return data[pos:end].decode("utf-8", errors="replace"), end

def read_bytes(data: bytes, pos: int) -> Tuple[bytes, int]:
    length, pos = read_varuint(data, pos)
    end = pos + length
    return data[pos:end], end

@dataclass
class PropertyEntry:
    key: int
    category: str
    value: Any

@dataclass
class ObjectEntry:
    type_key: int
    properties: List[PropertyEntry]

# -- Parser ------------------------------------------------------------------

def parse_objects(data: bytes, pos: int, header_keys: List[int], strict: bool = False) -> Tuple[int, List[ObjectEntry], List[int]]:
    """PR4: Enhanced parser with catalog support and robust EOF handling."""
    bitmap_count = (len(header_keys) + 3) // 4
    bitmap_bytes = data[pos - bitmap_count * 4 : pos]
    bitmaps = [struct.unpack_from("<I", bitmap_bytes, i * 4)[0] for i in range(bitmap_count)]
    key_index = {key: idx for idx, key in enumerate(header_keys)}

    objects: List[ObjectEntry] = []
    artboard_ids: List[int] = []  # PR4: Track artboard IDs from catalog
    obj_index = 0
    
    while True:
        # PR4: Graceful EOF at object boundary
        if pos >= len(data):
            print(f"  [info] Clean EOF at object boundary (parsed {obj_index} objects)")
            break
            
        try:
            type_key, pos = read_varuint(data, pos)
        except (EOFError, struct.error) as e:
            if pos >= len(data) - 8:  # Near end, likely clean termination
                print(f"  [info] EOF near end of file (parsed {obj_index} objects)")
                break
            else:
                msg = f"EOF while reading object #{obj_index} typeKey at pos {pos}"
                if strict:
                    raise ValueError(msg) from e
                print(f"  [warning] {msg}")
                break
                
        if type_key == 0:
            # Object stream terminator
            print(f"  [info] Object stream ended with 0 terminator (parsed {obj_index} objects)")
            # PR4: May have catalog chunks after this
            continue
            
        type_name = TYPE_KEY_NAME.get(type_key, f"Unknown({type_key})")
        props: List[PropertyEntry] = []
        prop_keys: List[int] = []
        
        try:
            while True:
                key, pos = read_varuint(data, pos)
                if key == 0:
                    break
                prop_keys.append(key)
                
                idx = key_index.get(key)
                category = "unknown"
                if idx is not None:
                    bucket = idx // 4
                    shift = (idx % 4) * 2
                    type_id = (bitmaps[bucket] >> shift) & 0x3
                    category = ["uint", "string", "double", "color"][type_id]
                    
                core = PROPERTY_CORE_TYPE.get(key)
                if core:
                    category = CORE_TO_CATEGORY.get(core, category)
                    
                # PR4: Handle bytes property (212) - FileAssetContents
                if key == 212:  # bytes property
                    byte_len, pos = read_varuint(data, pos)
                    if pos + byte_len > len(data):
                        raise ValueError(f"bytes property length {byte_len} exceeds file size")
                    value = f"<{byte_len} bytes>"
                    pos += byte_len
                    category = "bytes"
                else:
                    decoder = CATEGORY_DECODERS.get(category)
                    if decoder is None:
                        value, pos = read_varuint(data, pos)
                    else:
                        value, pos = decoder(data, pos)
                        
                props.append(PropertyEntry(key, category, value))
                
                # PR4: Track artboard IDs from ArtboardListItem (8776)
                if type_key == 8776 and key == 3:  # ArtboardListItem.id
                    artboard_ids.append(int(value))
                    
        except (EOFError, struct.error) as e:
            msg = f"EOF in object #{obj_index} ({type_name}) after keys {prop_keys} at pos {pos}"
            if strict:
                raise ValueError(msg) from e
            print(f"  [warning] {msg}")
            if props:  # Save partial object if we got any properties
                objects.append(ObjectEntry(type_key, props))
            break
            
        objects.append(ObjectEntry(type_key, props))
        obj_index += 1
        
    return pos, objects, artboard_ids

# -- Main --------------------------------------------------------------------

def analyze(path: Path, return_data: bool = False, strict: bool = False, dump_catalog: bool = False) -> Any:
    """PR4: Enhanced analyzer with catalog support."""
    data = path.read_bytes()
    if data[:4] != b"RIVE":
        raise ValueError("Not a RIVE file")
    pos = 4
    major, pos = read_varuint(data, pos)
    minor, pos = read_varuint(data, pos)
    file_id, pos = read_varuint(data, pos)

    header_keys: List[int] = []
    while True:
        key, pos = read_varuint(data, pos)
        if key == 0:
            break
        header_keys.append(key)

    bitmap_count = (len(header_keys) + 3) // 4
    pos_bitmap_end = pos + bitmap_count * 4
    pos, objects, artboard_ids = parse_objects(data, pos_bitmap_end, header_keys, strict=strict)
    
    # PR4: Dump catalog if requested
    if dump_catalog and artboard_ids:
        print(f"\n=== PR4 Artboard Catalog ===")
        print(f"Artboard IDs from ArtboardListItem (8776): {artboard_ids}")
        print(f"Total artboards: {len(artboard_ids)}")
        print(f"===========================")

    if return_data:
        header_named = [
            {
                "key": key,
                "names": VALUE_TO_NAMES.get(key, ["?"])
            }
            for key in header_keys
        ]
        object_list = []
        for obj in objects:
            entries = []
            for prop in obj.properties:
                entries.append(
                    {
                        "key": prop.key,
                        "names": VALUE_TO_NAMES.get(prop.key, ["?"]),
                        "category": prop.category,
                        "value": prop.value,
                    }
                )
            object_list.append(
                {
                    "typeKey": obj.type_key,
                    "typeName": TYPE_KEY_NAME.get(obj.type_key, f"type_{obj.type_key}"),
                    "properties": entries,
                }
            )
        return {
            "file": str(path),
            "version": f"{major}.{minor}",
            "fileId": file_id,
            "headerKeys": header_named,
            "objects": object_list,
            "remainingBytes": len(data) - pos,
        }

    print(f"File: {path}")
    print(f"Version: {major}.{minor}, fileId: {file_id}")
    print(f"Header property keys ({len(header_keys)}):")
    for key in header_keys:
        names = VALUE_TO_NAMES.get(key, ["?"])
        print(f"  {key}: {names[0]}")
    print()
    for obj in objects:
        type_name = TYPE_KEY_NAME.get(obj.type_key, f"type_{obj.type_key}")
        labeled = []
        for prop in obj.properties:
            name = VALUE_TO_NAMES.get(prop.key, ["?"])[0]
            value_repr = prop.value
            if prop.category == "double":
                value_repr = f"{prop.value:.3f}"
            elif prop.category == "color":
                value_repr = f"0x{prop.value:08X}"
            labeled.append(f"{prop.key}:{name}={value_repr}")
        print(f"Object {type_name} ({obj.type_key}) -> {labeled}")

    if pos < len(data):
        remaining = len(data) - pos
        print(f"\nRemaining {remaining} bytes after object stream (likely asset data).")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="PR4: Robust .riv analyzer")
    parser.add_argument("file", type=Path, help=".riv file to analyze")
    parser.add_argument("--json", action="store_true", help="Output as JSON")
    parser.add_argument("--strict", action="store_true", help="PR4: Abort on any parse anomaly")
    parser.add_argument("--dump-catalog", action="store_true", help="PR4: Show artboard catalog (8726/8776)")
    args = parser.parse_args()

    try:
        summary = analyze(args.file, return_data=args.json, strict=args.strict, dump_catalog=args.dump_catalog)
        if args.json:
            # JSON mode: summary is dict
            print(json.dumps(summary, indent=2))
        # else: Non-JSON mode already printed output in analyze()
        sys.exit(0)
    except ValueError as e:
        print(f"ERROR: {e}", file=sys.stderr)
        sys.exit(1 if args.strict else 0)
