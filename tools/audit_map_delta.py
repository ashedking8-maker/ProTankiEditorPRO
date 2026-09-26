#!/usr/bin/env python3
"""Read-only, order-independent XML inventory of two original-editor map exports.

Does not infer collider ownership: matching identities are normalized XML subtrees;
repeated identical elements are counted using a multiset. No input is modified.
"""
from __future__ import annotations
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import xml.etree.ElementTree as ET

SECTIONS = {'static-geometry': ('prop',),
            'collision-geometry': ('collision-plane', 'collision-box', 'collision-triangle')}


def canonical(node: ET.Element) -> tuple:
    """Ignore XML pretty-print whitespace and attribute order, preserve actual data."""
    value = (node.text or '').strip()
    return (node.tag, tuple(sorted(node.attrib.items())), value,
            tuple(canonical(child) for child in node))


def identity(node: ET.Element) -> dict:
    return {'tag': node.tag, 'attributes': dict(sorted(node.attrib.items())),
            'with_collision': node.findtext('with_collision'),
            'texture_name': node.findtext('texture-name'),
            'position': {coord.tag: (coord.text or '').strip()
                         for coord in node.findall('./position/*')},
            'rotation': {coord.tag: (coord.text or '').strip()
                         for coord in node.findall('./rotation/*')}}


def inspect_map(path: Path) -> dict:
    # Reject malformed/oversized input rather than running an unreliable comparison.
    if not path.is_file() or path.stat().st_size > 100 * 1024 * 1024:
        raise ValueError(f'not a map file or map too large: {path}')
    raw = path.read_bytes()
    root = ET.fromstring(raw)
    if root.tag != 'map':
        raise ValueError(f'expected <map> root: {path}')
    section_items = {}
    for section, names in SECTIONS.items():
        parent = root.find(section)
        if parent is None:
            raise ValueError(f'missing <{section}>: {path}')
        section_items[section] = {name: [node for node in parent.findall(name)] for name in names}
    return {'sha256': hashlib.sha256(raw).hexdigest(), 'root': root,
            'items': section_items}


def compare(before: Path, after: Path) -> dict:
    lhs, rhs = inspect_map(before), inspect_map(after)
    changes = {}
    for section, names in SECTIONS.items():
        changes[section] = {}
        for name in names:
            old, new = lhs['items'][section][name], rhs['items'][section][name]
            a, b = Counter(canonical(n) for n in old), Counter(canonical(n) for n in new)
            # Keep source ordering for diagnostic samples and explicit multiplicity.
            removed, added = a - b, b - a
            def samples(nodes: list[ET.Element], counts: Counter) -> list[dict]:
                result = []
                for node in nodes:
                    key = canonical(node)
                    count = counts.pop(key, 0)
                    if count:
                        result.append(dict(identity(node), occurrences=count))
                return result
            changes[section][name] = {
                'before': len(old), 'after': len(new),
                'removed': sum(removed.values()), 'added': sum(added.values()),
                'removed_samples': samples(old, removed),
                'added_samples': samples(new, added),
            }

    return {'schema': 1, 'before_sha256': lhs['sha256'], 'after_sha256': rhs['sha256'],
            'before_version': lhs['root'].attrib.get('version'),
            'after_version': rhs['root'].attrib.get('version'),
            'changes': changes,
            'warning': 'XML structural delta only: no inferred prop-to-collider ownership or game compatibility.'}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('before', type=Path)
    parser.add_argument('after', type=Path)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    try:
        result = compare(args.before, args.after)
        payload = json.dumps(result, ensure_ascii=False, indent=2) + '\n'
        if args.output:
            out = args.output.resolve()
            if out in (args.before.resolve(), args.after.resolve()):
                parser.error('report must not overwrite either input map')
            out.write_text(payload, encoding='utf-8')
        else:
            print(payload, end='')
    except (OSError, ET.ParseError, ValueError) as exc:
        parser.error(str(exc))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
