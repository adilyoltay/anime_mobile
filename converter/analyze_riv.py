#!/usr/bin/env python3
"""
Utility to inspect .riv binaries: prints property keys per object type using
Rive runtime metadata (generated headers).
"""
from __future__ import annotations

import argparse
import json
import re
import struct
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, Iterable, List, Tuple, Any, DefaultDict

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

def parse_objects(data: bytes, pos: int, header_keys: List[int]) -> Tuple[int, List[ObjectEntry]]:
    bitmap_count = (len(header_keys) + 3) // 4
    bitmap_bytes = data[pos - bitmap_count * 4 : pos]
    bitmaps = [struct.unpack_from("<I", bitmap_bytes, i * 4)[0] for i in range(bitmap_count)]
    key_index = {key: idx for idx, key in enumerate(header_keys)}

    objects: List[ObjectEntry] = []
    while True:
        type_key, pos = read_varuint(data, pos)
        if type_key == 0:
            # consume trailing property terminator if present
            try:
                _, pos = read_varuint(data, pos)
            except EOFError:
                pass
            break
        props: List[PropertyEntry] = []
        while True:
            key, pos = read_varuint(data, pos)
            if key == 0:
                break
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

            decoder = CATEGORY_DECODERS.get(category)
            if decoder is None:
                # Fallback to uint semantics when unknown
                value, pos = read_varuint(data, pos)
            else:
                value, pos = decoder(data, pos)
            props.append(PropertyEntry(key, category, value))
        objects.append(ObjectEntry(type_key, props))
    return pos, objects

# -- Main --------------------------------------------------------------------

def analyze(path: Path, return_data: bool = False) -> Any:
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
    pos, objects = parse_objects(data, pos_bitmap_end, header_keys)

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


def main():
    parser = argparse.ArgumentParser(description="Inspect Rive .riv binary")
    parser.add_argument("file", type=Path, help="Path to .riv file")
    parser.add_argument("--json", action="store_true", help="Output JSON instead of text")
    args = parser.parse_args()
    if args.json:
        info = analyze(args.file, return_data=True)
        print(json.dumps(info, indent=2))
    else:
        analyze(args.file)

if __name__ == "__main__":
    main()
