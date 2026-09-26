import sys
from pathlib import Path
import tempfile
import struct
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from audit_native_library import inspect_3ds, audit


def chunk(ident, payload=b''):
    return struct.pack('<HI', ident, len(payload)+6)+payload


class TestNativeLibraryAudit(unittest.TestCase):
    def test_3ds_tree_and_library(self):
        with tempfile.TemporaryDirectory() as t:
            root=Path(t); lib=root/'sample'; lib.mkdir()
            (lib/'model.3ds').write_bytes(chunk(0x4D4D, chunk(0x3D3D, chunk(0x4000, b'visual\x00'+chunk(0x4100, chunk(0x4110, b'\x00\x00'))))))
            (lib/'library.xml').write_text('<library name="Sample"><prop-group name="default"><prop name="Wall"><mesh file="model.3ds"><texture name="Default" diffuse-map="missing.jpg"/></mesh></prop></prop-group></library>',encoding='utf-8')
            report=audit(root)
            self.assertEqual(report['summary']['props'],1)
            self.assertEqual(report['summary']['missing_files'],1)
            self.assertEqual(report['libraries'][0]['props'][0]['mesh_inventory']['objects'],['visual'])
            self.assertEqual(report['libraries'][0]['props'][0]['mesh_inventory']['chunks']['4110'],1)
            with self.assertRaises(ValueError): inspect_3ds(lib/'library.xml')

    def test_reject_broken_lengths(self):
        with tempfile.TemporaryDirectory() as t:
            path=Path(t)/'bad.3ds'
            path.write_bytes(struct.pack('<HI',0x4D4D,99999))
            with self.assertRaises(ValueError): inspect_3ds(path)

if __name__=='__main__': unittest.main()

class TestBundledLegacyEvidence(unittest.TestCase):
    def test_real_legacy_fixtures_contain_additional_mesh_nodes(self):
        fixtures=Path(__file__).parent/'fixtures'
        tile=inspect_3ds(fixtures/'LandTiles'/'tile_01.3ds')
        chest=inspect_3ds(fixtures/'Stuffs'/'chest02.3ds')
        bridge=inspect_3ds(fixtures/'IndustrialBridge'/'brid_7.3ds')
        self.assertIn('Plane02',tile['objects'])
        self.assertEqual(len(chest['objects']),5)
        self.assertTrue(any(n.startswith('plane') for n in bridge['objects']))
        self.assertTrue(any(n.startswith('tri') for n in bridge['objects']))
