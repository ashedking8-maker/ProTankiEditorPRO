"""Independent original NuBu 3 3DS box nodes versus game-author XML fixture."""
import math
from pathlib import Path
import unittest
import xml.etree.ElementTree as ET
from test_native_helper_0515_audit import read_nodes

ROOT=Path(__file__).resolve().parent.parent
FIX=ROOT/'tests/fixtures/native_helpers'

class NativeBox0519Audit(unittest.TestCase):
    def test_original_box_helpers_match_original_editor_xml_in_world_space(self):
        nodes=read_nodes(FIX/'nubu_3.3ds')
        root=ET.parse(FIX/'nubu3_original_map.xml').getroot()
        prop=root.find('./static-geometry/prop')
        x=float(prop.findtext('position/x'));y=float(prop.findtext('position/y'))
        z=float(prop.findtext('position/z'));yaw=float(prop.findtext('rotation/z'))
        anchor=nodes['nubu_03']['m'][9:12]
        source=[]
        for name,item in nodes.items():
            if not name.lower().startswith('box'):continue
            v=item['v']
            self.assertGreaterEqual(len(v),8)
            lo=[min(t[i] for t in v) for i in range(3)]
            hi=[max(t[i] for t in v) for i in range(3)]
            center=[(a+b)*.5-anchor[i] for i,(a,b) in enumerate(zip(lo,hi))]
            size=[b-a for a,b in zip(lo,hi)]
            cx=x+math.cos(yaw)*center[0]-math.sin(yaw)*center[1]
            cy=y+math.sin(yaw)*center[0]+math.cos(yaw)*center[1]
            source.append((name,(cx,cy,z+center[2]),size))
        self.assertEqual(len(source),2)
        boxes=root.findall('./collision-geometry/collision-box')
        self.assertEqual(len(boxes),2)
        matched=set()
        for name,pos,size in source:
            candidates=[]
            for index,node in enumerate(boxes):
                got_pos=[float(node.findtext('position/'+k)) for k in ('x','y','z')]
                got_size=[float(node.findtext('size/'+k)) for k in ('x','y','z')]
                if all(abs(a-b)<.005 for a,b in zip(got_pos,pos)) and all(abs(a-b)<.005 for a,b in zip(got_size,size)):
                    candidates.append(index)
            self.assertEqual(len(candidates),1,name)
            matched.add(candidates[0])
        self.assertEqual(len(matched),2)
    def test_box_writer_and_xml_inspectors_are_present(self):
        imp=(ROOT/'src/NativeCollisionImport.h').read_text()
        doc=(ROOT/'src/MapDocument.cpp').read_text()
        ui=(ROOT/'src/EditorUi.cpp').read_text()
        for token in ('result.boxes.push_back(out)','Missing box corners:','No native plane/box/triangle helpers'):
            self.assertIn(token,imp)
        for token in ('for(const auto& shape:source.boxes)','collisionBoxes_.push_back(c)','for(const auto i:boxes)collisionBoxes_[i].authoredOwnerIndex=owner'):
            self.assertIn(token,doc)
        for token in ('DrawRawPropertyTree(', 'Original library <prop> (all fields)',
                      'Original map <prop> (all fields)', 'All imported object properties',
                      'Imported library properties (read only)'):
            self.assertIn(token,ui)
        self.assertNotIn('Native geometry: unsupported',ui)
if __name__=='__main__':unittest.main()
