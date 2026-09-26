"""Evidence audit and static guard. Windows CTest exercises real XML serialization.

Ground/pavement effects are not inferred from texture names or made-up flags.
"""
from pathlib import Path
import json
import re
import unittest
import xml.etree.ElementTree as ET

ROOT=Path(__file__).resolve().parent.parent

class NativeMetadata0516Audit(unittest.TestCase):
    def test_original_map_fixture_stays_independent(self):
        ref=ET.parse(ROOT/'tests/fixtures/native_helpers/concrete_wall_end1_original_map.xml').getroot()
        self.assertEqual(len(ref.findall('collision-geometry/collision-plane')),6)
        self.assertEqual(len(ref.findall('collision-geometry/collision-triangle')),10)

    def test_explicit_per_prop_flag_preservation_and_no_surface_invention(self):
        h=(ROOT/'src/MapDocument.h').read_text(encoding='utf8')
        m=(ROOT/'src/MapDocument.cpp').read_text(encoding='utf8')
        u=(ROOT/'src/EditorUi.cpp').read_text(encoding='utf8')
        draft=(ROOT/'src/ObjectDraft.h').read_text(encoding='utf8')
        self.assertIn('int nativeWithCollision{-1}',h)
        self.assertIn('int nativeFree{-1}',h)
        self.assertIn('bool hasUncopyableMetadata{}',h)
        self.assertIn('p.child("with_collision")',m)
        self.assertIn('p.nativeWithCollision >= 0',m)
        self.assertIn('p.nativeFree >= 0',m)
        self.assertIn('originalNodes.push_back(node)',m)
        self.assertIn('geometry.remove_child(originalNodes[i])',m)
        self.assertIn('no owned collision primitives',m)
        self.assertIn('unknown source metadata',m)
        self.assertIn('pending.nativeWithCollision==1',u)
        self.assertIn('Object purpose (draft)',u)
        self.assertIn('native_export false',draft)
        self.assertIn('Purpose::DriveableDraft',draft)
        self.assertNotIn('surface="dirt"',m)
        self.assertNotIn('surface="asphalt"',m)

    def test_windows_integration_is_registered(self):
        cmake=(ROOT/'CMakeLists.txt').read_text(encoding='utf8')
        self.assertIn('LegacyFidelityRegression',cmake)
        self.assertIn('OriginalMetadataAndClipboardRoundTrip',cmake)
        self.assertTrue((ROOT/'tests/legacy_fidelity_regression.cpp').is_file())

    def test_read_only_original_map_inventory(self):
        evidence=json.loads((ROOT/'docs/ORIGINAL_MAP_FEATURE_AUDIT_0516.json').read_text(encoding='utf8'))
        self.assertEqual(evidence['maps'],201)
        self.assertEqual(evidence['props'],289730)
        self.assertEqual(evidence['prop_with_collision'],{'0':1093,'1':6754,'absent':281883})
        self.assertEqual(evidence['prop_free'].get('true'),2935)
        self.assertEqual(evidence['failed_xml'],[])

if __name__=='__main__':unittest.main()
