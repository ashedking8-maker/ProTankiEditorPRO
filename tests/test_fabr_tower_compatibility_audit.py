import sys
from pathlib import Path
import unittest
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from compare_single_prop_exports import compare, inspect

class FabrTowerUserEvidence(unittest.TestCase):
    def test_all_six_planes_are_identical_in_prop_local_coordinates(self):
        folder=ROOT/'tests/fixtures/fabr_tower_user'
        result=compare(folder/'AAA.xml',folder/'BBB.xml')
        self.assertTrue(result['same_identity'])
        self.assertTrue(result['identical_local_collision_records'])
        self.assertEqual(result['collision_count_A'],6)
        self.assertEqual(result['position_A_minus_B'],['-500.000','0.000','0.500'])
        self.assertEqual(len(set(result['ids_A'])),6)
        self.assertEqual(len(set(result['ids_B'])),1)
    def test_no_roof_or_floor_was_added_to_either_export(self):
        folder=ROOT/'tests/fixtures/fabr_tower_user'
        for name in ('AAA.xml','BBB.xml'):
            data=inspect(folder/name)
            self.assertEqual({kind for kind,values in data['records']},{'collision-plane'})
            for kind,values in data['records']:
                self.assertEqual(str(dict(values)['rotation/y']),'1.570796')

if __name__=='__main__':unittest.main()
