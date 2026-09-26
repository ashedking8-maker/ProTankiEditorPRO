"""Independent source-data audit for the sole verified 0.5.14 native collision shape.

The fixture contains original XML records from the user-supplied ProTLVK
map_beachfort.xml (ten original prop instances, 160 real collider records).
It is not produced by the editor's C++ serializer.
"""
from pathlib import Path
import math
import re
import unittest
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parent.parent
HEADER = ROOT / 'src' / 'VerifiedCollisionTemplates.h'
FIXTURE = ROOT / 'tests' / 'fixtures' / 'beach_wall_end2_reference.xml'
FLOAT = re.compile(r'[-+]?\d+\.\d+f')


def vec(node, field):
    return tuple(float(node.findtext(f'{field}/{axis}', '0')) for axis in 'xyz')


def angle_delta(a, b):
    return math.atan2(math.sin(a - b), math.cos(a - b))


def templates(array_name, count, value_count):
    content = HEADER.read_text(encoding='utf-8').splitlines()
    start = next(i for i, line in enumerate(content) if f' {array_name}{{{{' in line)
    lines = content[start + 1:start + count + 1]
    values = [tuple(float(x[:-1]) for x in FLOAT.findall(line)) for line in lines]
    assert len(values) == count and all(len(row) == value_count for row in values), (array_name, values)
    return values


class NativeCollision0514Audit(unittest.TestCase):
    def test_original_10_placements_match_all_160_template_primitives(self):
        root = ET.parse(FIXTURE).getroot()
        props = root.findall('./static-geometry/prop')
        planes = root.findall('./collision-geometry/collision-plane')
        triangles = root.findall('./collision-geometry/collision-triangle')
        self.assertEqual((len(props), len(planes), len(triangles)), (10, 60, 100))
        plane_tpl = templates('BeachWallEnd2Planes', 6, 8)
        triangle_tpl = templates('BeachWallEnd2Triangles', 10, 15)
        for i, prop in enumerate(props):
            self.assertEqual((prop.get('library-name'), prop.get('group-name'), prop.get('name')),
                             ('Beach', 'sidewalls', 'Wall End 2'))
            pivot = vec(prop, 'position')
            theta = float(prop.findtext('rotation/z', '0'))
            c, s = math.cos(theta), math.sin(theta)
            for j, row in enumerate(plane_tpl):
                node = planes[i * 6 + j]
                offset, euler, width, length = row[:3], row[3:6], row[6], row[7]
                want = (pivot[0] + c * offset[0] - s * offset[1],
                        pivot[1] + s * offset[0] + c * offset[1], pivot[2] + offset[2])
                for actual, expected in zip(vec(node, 'position'), want):
                    self.assertAlmostEqual(actual, expected, delta=.021)
                for actual, expected in zip(vec(node, 'rotation')[:2], euler[:2]):
                    self.assertAlmostEqual(actual, expected, delta=.0001)
                self.assertAlmostEqual(angle_delta(vec(node, 'rotation')[2], euler[2] + theta), 0, delta=.0001)
                self.assertAlmostEqual(float(node.findtext('width')), width, delta=.002)
                self.assertAlmostEqual(float(node.findtext('length')), length, delta=.002)
            for j, row in enumerate(triangle_tpl):
                node = triangles[i * 10 + j]
                offset, euler = row[:3], row[3:6]
                want = (pivot[0] + c * offset[0] - s * offset[1],
                        pivot[1] + s * offset[0] + c * offset[1], pivot[2] + offset[2])
                for actual, expected in zip(vec(node, 'position'), want):
                    self.assertAlmostEqual(actual, expected, delta=.021)
                for actual, expected in zip(vec(node, 'rotation')[:2], euler[:2]):
                    self.assertAlmostEqual(actual, expected, delta=.0001)
                self.assertAlmostEqual(angle_delta(vec(node, 'rotation')[2], euler[2] + theta), 0, delta=.0001)
                for field, start in [('v0', 6), ('v1', 9), ('v2', 12)]:
                    for actual, expected in zip(vec(node, field), row[start:start + 3]):
                        self.assertAlmostEqual(actual, expected, delta=.002)

    def test_collision_export_is_explicit_and_unsafe_default_is_disclosed(self):
        doc = (ROOT / 'src/MapDocument.cpp').read_text(encoding='utf-8')
        ui = (ROOT / 'src/EditorUi.cpp').read_text(encoding='utf-8')
        cmake = (ROOT / 'CMakeLists.txt').read_text(encoding='utf-8')
        self.assertIn('!VerifiedCollisionTemplates::Available(p.library,p.group,p.name)', doc)
        self.assertIn('v.authoredOwnerIndex=static_cast<int>(index)', doc)
        self.assertIn('else if(t->transformDirty)', doc)
        self.assertIn('t.legacySourceIndex<0', doc)
        self.assertIn('map.AddVerifiedCollisionForProp(index)', ui)
        self.assertIn('AuthorCollisionForPlacement(map,assets,index,info)', ui)
        self.assertIn('map.AddImportedCollisionForProp(index,imported)', ui)
        self.assertIn('without authored native collision (tank may pass through them)', ui)
        self.assertIn('Repair saved wall collision', ui)
        self.assertIn('HasVerifiedCollisionForProp', doc)
        self.assertIn('NativeCollisionAuthoringRegression', cmake)
        self.assertIn('beach_wall_end2_reference.xml', cmake)


if __name__ == '__main__':
    unittest.main()
