"""Source/test wiring audit; Windows C++ CTest remains the runtime authority."""
from pathlib import Path
import re
import unittest
from xml.etree import ElementTree as ET

ROOT=Path(__file__).resolve().parents[1]
class VerifiedGroundAudit(unittest.TestCase):
    def test_original_independent_reference_six(self):
        fixture=ROOT/'tests/fixtures/fogtown_ground'
        root=ET.parse(fixture/'original_fogtown_ground_six.xml').getroot()
        props=list(root.find('static-geometry'))
        planes=list(root.find('collision-geometry'))
        specs={'t11':(500,500),'t21':(500,1000),'t22':(1000,1000),
               't32':(1000,1500),'t33':(1500,1500),'t55':(2500,2500)}
        self.assertEqual(len(props),len(planes)==6 and 6)
        self.assertEqual([x.attrib['name'] for x in props],list(specs))
        self.assertEqual([x.attrib['id'] for x in planes],['3','105','309','5','302','196'])
        for prop,plane in zip(props,planes):
            self.assertEqual(prop.attrib['library-name'],'Fogtown')
            self.assertEqual(prop.attrib['group-name'],'l')
            self.assertEqual(plane.tag,'collision-plane')
            self.assertEqual((float(plane.findtext('width')),float(plane.findtext('length'))),specs[prop.attrib['name']])
            self.assertTrue((fixture/(prop.attrib['name']+'.3ds')).is_file())
    def test_exact_user_map_repro_is_in_native_regression(self):
        fixture=ROOT/'tests/fixtures/fogtown_ground/TEST_MAP_AAA-original-repro.xml'
        root=ET.parse(fixture).getroot()
        tiles=[p for p in root.find('static-geometry') if p.attrib.get('library-name')=='Fogtown'
            and p.attrib.get('name') in ('t11','t22')]
        self.assertEqual(len(tiles),11)
        self.assertEqual(len(root.find('static-geometry')),15)
        self.assertEqual(len(root.find('collision-geometry')),13)
        cpp=(ROOT/'tests/verified_ground_regression.cpp').read_text()
        self.assertIn('added==11',cpp)
        self.assertIn('reproRound.CollisionPlanes().size()==14',cpp)
    def test_mesh_geometry_is_required_not_guessed(self):
        src=(ROOT/'src/VerifiedGroundCollision.h').read_text()
        for token in ('ReadNodes(path,error)','area-double(width)*length','out.flatRectangle=true',
                      'out.matchesNativeReference=true','reference->visualWidth','reference->visualLength',
                      'source 3DS now has collision helpers'):
            if token=='source 3DS now has collision helpers':
                self.assertIn('Source 3DS now has collision helpers',src)
            else:self.assertIn(token,src)
    def test_placement_and_save_have_separate_safety_guards(self):
        ui=(ROOT/'src/EditorUi.cpp').read_text()
        doc=(ROOT/'src/MapDocument.cpp').read_text()
        self.assertIn('map.AddVerifiedGroundSurfaceForProp(index)',ui)
        self.assertIn('candidateMissingFloor',ui)
        self.assertIn('map.RepairVerifiedGroundSurfaces(certified,repaired,missing)',ui)
        self.assertIn('map=std::move(beforeRepair);',ui)
        self.assertIn('HasVerifiedGroundSurfaceForProp(index)',doc)
        self.assertIn('BindVerifiedGroundOwners();',doc)
        self.assertIn('Original-map verified ground plane authored',doc)
        self.assertIn('occupiedColliderIds',doc)
    def test_cpp_regression_registered(self):
        cm=(ROOT/'CMakeLists.txt').read_text()
        self.assertIn('SourceVerifiedFogtownGroundAndRoundTrip',cm)
        self.assertIn('VerifiedGroundCollisionRegression',cm)
        cpp=(ROOT/'tests/verified_ground_regression.cpp').read_text()
        for token in ('SetPropTransform','DeletePropWithCollision','RepairVerifiedGroundSurfaces',
                      'HasNativeCollisionForProp','ambiguous','original_fogtown_ground_six'):
            if token=='original_fogtown_ground_six':self.assertIn(token,cm)
            else:self.assertIn(token,cpp)
if __name__=='__main__':unittest.main()
