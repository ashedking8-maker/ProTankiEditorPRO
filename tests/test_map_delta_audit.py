import tempfile
import unittest
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from audit_map_delta import compare, inspect_map

class DeltaAuditTest(unittest.TestCase):
    def test_duplicate_colliders_and_order_independent(self):
        with tempfile.TemporaryDirectory() as tmp:
            a, b = Path(tmp)/'a.xml', Path(tmp)/'b.xml'
            a.write_text('''<map version="1.0.Light"><static-geometry>
            <prop name="A"/><prop name="B"/></static-geometry><collision-geometry>
            <collision-box id="shared"><position><x>5</x></position></collision-box>
            <collision-box id="shared"><position><x>5</x></position></collision-box>
            <collision-plane id="floor"/><collision-triangle id="tri"/>
            </collision-geometry></map>''')
            b.write_text('''<map version="1.0.Light"><static-geometry>
            <prop name="B"/></static-geometry><collision-geometry>
            <collision-triangle id="tri"/><collision-box id="shared"><position><x>5</x></position></collision-box>
            <collision-plane id="floor"/></collision-geometry></map>''')
            r = compare(a,b)['changes']
            self.assertEqual(r['static-geometry']['prop']['removed'], 1)
            self.assertEqual(r['collision-geometry']['collision-box']['removed'], 1)
            self.assertEqual(r['collision-geometry']['collision-box']['removed_samples'][0]['occurrences'], 1)
            self.assertEqual(r['collision-geometry']['collision-triangle']['removed'], 0)
            self.assertEqual(r['collision-geometry']['collision-plane']['added'], 0)
            self.assertEqual(a.read_text().count('collision-box'),4) # read-only

    def test_invalid_map(self):
        with tempfile.TemporaryDirectory() as tmp:
            f=Path(tmp)/'bad.xml'; f.write_text('<map><static-geometry/></map>')
            with self.assertRaisesRegex(ValueError, 'collision-geometry'): inspect_map(f)

    def test_native_flag_and_texture_change_is_visible_in_delta(self):
        with tempfile.TemporaryDirectory() as tmp:
            a,b=Path(tmp)/'a.xml',Path(tmp)/'b.xml'
            first='<map version="1.0.Light"><static-geometry><prop name="floor"><texture-name>Tile Asp 1</texture-name><with_collision>1</with_collision></prop></static-geometry><collision-geometry/></map>'
            second=first.replace('Tile Asp 1','Tile Beton 1').replace('<with_collision>1','<with_collision>0')
            a.write_text(first);b.write_text(second)
            delta=compare(a,b)['changes']['static-geometry']['prop']
            self.assertEqual(delta['removed_samples'][0]['with_collision'],'1')
            self.assertEqual(delta['added_samples'][0]['with_collision'],'0')
            self.assertEqual(delta['removed_samples'][0]['texture_name'],'Tile Asp 1')
            self.assertEqual(delta['added_samples'][0]['texture_name'],'Tile Beton 1')

if __name__ == '__main__': unittest.main()
