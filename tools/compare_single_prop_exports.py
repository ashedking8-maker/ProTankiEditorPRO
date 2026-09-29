#!/usr/bin/env python3
"""Compare native collision records of two single-prop exports in prop-local space.

Ignores collision IDs and record order. Does not simulate game physics or claim
all alternative Euler representations/plane bases are textually equivalent.
"""
import argparse
from collections import Counter
from decimal import Decimal
import json
from pathlib import Path
import xml.etree.ElementTree as ET

def inspect(path):
    root = ET.parse(path).getroot()
    props = root.findall('./static-geometry/prop')
    if len(props) != 1:
        raise ValueError('This comparison requires exactly one prop per export.')
    prop = props[0]
    position = tuple(Decimal(prop.findtext(f'position/{axis}', '0')) for axis in 'xyz')
    rotation = tuple(Decimal(prop.findtext(f'rotation/{axis}', '0')) for axis in 'xyz')
    if any(rotation):
        raise ValueError('This diagnostic compares unrotated prop instances only.')
    records = []
    ids = []
    for node in root.findall('./collision-geometry/*'):
        if node.tag not in ('collision-plane', 'collision-box', 'collision-triangle'):
            raise ValueError('Unrecognized collision record: ' + node.tag)
        values = []
        for child in node:
            if list(child):
                for axis in child:
                    number = Decimal(axis.text or '0')
                    if child.tag == 'position':
                        number -= position['xyz'.index(axis.tag)]
                    values.append((child.tag + '/' + axis.tag, number))
            else:
                values.append((child.tag, Decimal(child.text or '0')))
        records.append((node.tag, tuple(sorted(values))))
        ids.append(node.get('id'))
    identity = tuple(prop.get(k, '') for k in ('library-name', 'group-name', 'name'))
    return dict(identity=identity, position=position, records=Counter(records), ids=ids)

def compare(first, second):
    a, b = inspect(first), inspect(second)
    return dict(same_identity=a['identity'] == b['identity'],
                identical_local_collision_records=a['records'] == b['records'],
                position_A_minus_B=[str(x-y) for x,y in zip(a['position'], b['position'])],
                collision_count_A=sum(a['records'].values()), collision_count_B=sum(b['records'].values()),
                ids_A=a['ids'], ids_B=b['ids'])

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('first', type=Path)
    parser.add_argument('second', type=Path)
    args = parser.parse_args()
    result = compare(args.first, args.second)
    print(json.dumps(result, indent=2))
    raise SystemExit(0 if result['same_identity'] and result['identical_local_collision_records'] else 1)
