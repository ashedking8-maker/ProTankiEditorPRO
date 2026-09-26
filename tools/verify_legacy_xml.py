#!/usr/bin/env python3
"""Check ProTanki/GTanks XML exports without normalizing away gameplay data.

Usage:
  python tools/verify_legacy_xml.py original.xml exported.xml
  python tools/verify_legacy_xml.py --allow-prop-add-delete original.xml exported.xml

Default: identical static prop identity/material/order and arbitrary edited transforms.
Add/delete mode: checks all non-static sections, attributes and metadata unchanged,
while allowing the static prop list to change. This mode does not validate a
specific intended add/delete operation; use MapEditRegression for that contract.
"""
from __future__ import annotations
import argparse
import copy
import xml.etree.ElementTree as ET
from pathlib import Path


def strip_transform_values(root: ET.Element) -> None:
    geometry = root.find("static-geometry")
    if geometry is None:
        return
    for prop in geometry.findall("prop"):
        for name in ("position", "rotation"):
            node = prop.find(name)
            if node is not None:
                for child in list(node):
                    child.text = "<editable>"


def canonical(element: ET.Element):
    return (
        element.tag,
        tuple(sorted(element.attrib.items())),
        (element.text or "").strip(),
        tuple(canonical(child) for child in list(element)),
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--allow-prop-add-delete", action="store_true")
    parser.add_argument("original", type=Path)
    parser.add_argument("exported", type=Path)
    args = parser.parse_args()
    try:
        original = ET.parse(args.original).getroot()
        exported = ET.parse(args.exported).getroot()
    except (ET.ParseError, OSError) as e:
        print(f"FAIL: could not parse map: {e}")
        return 1
    if original.tag != "map" or exported.tag != "map":
        print("FAIL: root element must be <map>")
        return 1
    if original.attrib != exported.attrib:
        print(f"FAIL: map attributes changed: {original.attrib!r} -> {exported.attrib!r}")
        return 1

    a, b = copy.deepcopy(original), copy.deepcopy(exported)
    props_a = original.findall("./static-geometry/prop")
    props_b = exported.findall("./static-geometry/prop")
    if args.allow_prop_add_delete:
        # All children under <static-geometry> are prop records in the supplied
        # legacy schema; preserve non-prop extension records when present.
        ga, gb = a.find("static-geometry"), b.find("static-geometry")
        if (ga is None) != (gb is None):
            print("FAIL: static-geometry section added or removed")
            return 1
        if ga is not None and gb is not None:
            for prop in list(ga.findall("prop")):
                ga.remove(prop)
            for prop in list(gb.findall("prop")):
                gb.remove(prop)
        if canonical(a) != canonical(b):
            print("FAIL: collision/gameplay/map metadata or non-prop static content changed")
            return 1
        print(f"PASS: non-prop legacy XML preserved; {len(props_a)} -> {len(props_b)} props")
        return 0

    strip_transform_values(a)
    strip_transform_values(b)
    if canonical(a) != canonical(b):
        print("FAIL: content outside editable static-prop transforms changed")
        return 1
    if len(props_a) != len(props_b):
        print(f"FAIL: prop count changed: {len(props_a)} -> {len(props_b)}")
        return 1
    print(f"PASS: legacy structure preserved; {len(props_a)} static props checked")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
