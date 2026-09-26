#!/usr/bin/env python3
"""Read-only inventory of legacy library XML and 3DS object chunks.

This is evidence collection, NOT a conversion/exporter. Never changes inputs.
"""
from __future__ import annotations
import argparse
import json
from pathlib import Path
import struct
import xml.etree.ElementTree as ET

# Official 3DS chunk identifiers relevant to mesh-node diagnosis.
CONTAINERS = {0x4D4D, 0x3D3D, 0x4100, 0xAFFF, 0xB000, 0xB002, 0xB003, 0xB004}
NAMES = {0x4D4D:'MAIN', 0x3D3D:'EDIT', 0x4000:'OBJECT', 0x4100:'TRI_MESH',
         0x4110:'VERTICES', 0x4120:'FACES', 0x4140:'TEXCOORDS', 0xAFFF:'MATERIAL',
         0xB000:'KEYFRAMES', 0xB002:'OBJECT_NODE', 0xB010:'NODE_HEADER',
         0xB013:'PIVOT', 0x4160:'LOCAL_AXIS'}


def inspect_3ds(path: Path) -> dict:
    data = path.read_bytes()
    if len(data) < 6 or len(data) > 256 * 1024 * 1024:
        raise ValueError('invalid 3DS size')
    ids: dict[str, int] = {}
    objects: list[str] = []
    def scan(start: int, end: int, depth: int) -> None:
        if depth > 20:
            raise ValueError('3DS nesting too deep')
        offset = start
        while offset < end:
            if end - offset < 6:
                raise ValueError('truncated 3DS chunk header')
            ident, length = struct.unpack_from('<HI', data, offset)
            if length < 6 or length > end - offset:
                raise ValueError('invalid 3DS chunk length')
            key = f'{ident:04X}'
            ids[key] = ids.get(key, 0) + 1
            payload = offset + 6
            if ident == 0x4000:
                name_end = data.find(b'\x00', payload, offset + length)
                if name_end < 0 or name_end - payload > 255:
                    raise ValueError('invalid 3DS object name')
                objects.append(data[payload:name_end].decode('cp1252', errors='replace'))
                scan(name_end + 1, offset + length, depth + 1)
            elif ident in CONTAINERS:
                scan(payload, offset + length, depth + 1)
            offset += length
    first, total = struct.unpack_from('<HI', data, 0)
    if first != 0x4D4D or total != len(data):
        raise ValueError('invalid 3DS MAIN chunk or trailing bytes')
    scan(0, len(data), 0)
    return {'size': len(data), 'objects': objects, 'chunks': ids,
            'known_chunks': {NAMES[int(k,16)]: v for k,v in ids.items() if int(k,16) in NAMES}}


def audit(root: Path) -> dict:
    if not root.is_dir():
        raise ValueError('library root is not a directory')
    result: dict = {'schema': 1, 'library_root': str(root.resolve()), 'libraries': [],
                    'summary': {'libraries': 0, 'props': 0, 'mesh_props': 0,
                                'sprites': 0, 'missing_files': 0, 'invalid_3ds': 0}}
    for library in sorted(root.iterdir()):
        xml = library / 'library.xml'
        if not library.is_dir() or not xml.is_file():
            continue
        record: dict = {'folder': library.name, 'props': [], 'issues': []}
        try:
            tree = ET.parse(xml)
            element = tree.getroot()
            if element.tag != 'library':
                raise ValueError('root element is not library')
            record['name'] = element.attrib.get('name', library.name)
            for group in element.findall('prop-group'):
                for prop in group.findall('prop'):
                    p: dict = {'group': group.attrib.get('name',''), 'name': prop.attrib.get('name','')}
                    result['summary']['props'] += 1
                    mesh = prop.find('mesh')
                    sprite = prop.find('sprite')
                    if mesh is not None:
                        result['summary']['mesh_props'] += 1
                        relative = Path(mesh.attrib.get('file',''))
                        p['mesh'] = str(relative)
                        p['textures'] = [dict(t.attrib) for t in mesh.findall('texture')]
                        if relative.is_absolute() or '..' in relative.parts or not str(relative):
                            p['issue'] = 'unsafe or missing mesh path'
                        else:
                            file = library / relative
                            if not file.is_file():
                                p['issue'] = 'mesh file missing'; result['summary']['missing_files'] += 1
                            elif file.suffix.lower() != '.3ds':
                                p['issue'] = 'not a native 3DS mesh'
                            else:
                                try:
                                    p['mesh_inventory'] = inspect_3ds(file)
                                except (ValueError, OSError) as exc:
                                    p['issue'] = f'invalid 3DS: {exc}'
                                    result['summary']['invalid_3ds'] += 1
                        for tex in p['textures']:
                            path = Path(tex.get('diffuse-map',''))
                            if path.is_absolute() or '..' in path.parts or not (library/path).is_file():
                                p.setdefault('missing_textures', []).append(str(path))
                                result['summary']['missing_files'] += 1
                    elif sprite is not None:
                        result['summary']['sprites'] += 1
                        p['sprite'] = sprite.attrib.get('file','')
                    else:
                        p['issue'] = 'no mesh or sprite'
                    record['props'].append(p)
        except (ET.ParseError, ValueError, OSError) as exc:
            record['issues'].append(str(exc))
        result['libraries'].append(record)
    result['summary']['libraries'] = len(result['libraries'])
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('library_root', type=Path)
    parser.add_argument('--output', type=Path, help='Write report outside the original library')
    args = parser.parse_args()
    report = audit(args.library_root)
    payload = json.dumps(report, indent=2, ensure_ascii=False)
    if args.output:
        output = args.output.resolve()
        root = args.library_root.resolve()
        if output == root or root in output.parents:
            parser.error('report must not be written inside the original library')
        output.write_text(payload + '\n', encoding='utf-8')
    else:
        print(payload)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
