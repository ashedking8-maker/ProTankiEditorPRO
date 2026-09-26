#!/usr/bin/env python3
"""READ-ONLY inventory of native ProTLVK map XML features, not a game spec.

Usage:
  python tools/audit_native_map_features.py path/to/map.xml
  python tools/audit_native_map_features.py path/to/game.zip
  python tools/audit_native_map_features.py path/to/maps-directory
This tool never edits originals. In particular, with_collision is an observed
per-prop XML field, not proof of specific particle/physics semantics.
"""
from __future__ import annotations
import argparse
from collections import Counter
from pathlib import Path
import json
import xml.etree.ElementTree as ET
import zipfile


def sources(path: Path):
    if path.is_file() and path.suffix.lower() == '.zip':
        with zipfile.ZipFile(path) as archive:
            for info in archive.infolist():
                if '/maps/' in '/' + info.filename and info.filename.lower().endswith('.xml'):
                    if info.file_size <= 256 * 1024 * 1024:
                        yield info.filename, archive.read(info)
    elif path.is_dir():
        for file in sorted(path.rglob('*.xml')):
            if file.stat().st_size <= 256 * 1024 * 1024:
                yield str(file), file.read_bytes()
    elif path.is_file() and path.suffix.lower() == '.xml':
        yield str(path), path.read_bytes()
    else:
        raise ValueError('Expected a legacy map XML, maps directory, or ZIP of original maps.')


def audit(path: Path) -> dict:
    result = {'maps': 0, 'props': 0, 'prop_with_collision': Counter(),
              'prop_free': Counter(), 'prop_extra_attributes': Counter(),
              'prop_extra_children': Counter(), 'map_sections': Counter(),
              'collision_shape_tags': Counter(), 'texture_variants': Counter(),
              'failed_xml': []}
    for name, blob in sources(path):
        try: root = ET.fromstring(blob)
        except ET.ParseError as exc:
            result['failed_xml'].append({'file':name, 'reason':str(exc)}); continue
        if root.tag != 'map': continue
        result['maps'] += 1
        for n in root:
            result['map_sections'][n.tag] += 1
        geometry = root.find('static-geometry')
        if geometry is not None:
            for prop in geometry.findall('prop'):
                result['props'] += 1
                c = prop.find('with_collision')
                result['prop_with_collision']['absent' if c is None else (c.text or '').strip()] += 1
                result['prop_free'][prop.get('free','absent')] += 1
                for key in prop.attrib:
                    if key not in {'library-name','group-name','name','free'}:
                        result['prop_extra_attributes'][key] += 1
                for item in prop:
                    if item.tag not in {'rotation','texture-name','position','with_collision'}:
                        result['prop_extra_children'][item.tag] += 1
                result['texture_variants'][prop.findtext('texture-name','')] += 1
        for collision in root.findall('collision-geometry'):
            for item in collision:
                result['collision_shape_tags'][item.tag] += 1
    return {k: dict(sorted(v.items())) if isinstance(v,Counter) else v for k,v in result.items()}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source',type=Path)
    args=parser.parse_args()
    print(json.dumps(audit(args.source), indent=2, sort_keys=True))
