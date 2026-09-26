"""0.5.18 UI and original 3DS two-sided atlas source regression."""
from pathlib import Path
import struct
import unittest
from collections import defaultdict

ROOT=Path(__file__).resolve().parent.parent

def source(path):return (ROOT/path).read_text(encoding='utf-8')

def original_visible_mesh(path):
    data=path.read_bytes(); found=[]
    def chunks(start,end):
        while start+6<=end:
            tag,length=struct.unpack_from('<HI',data,start)
            if length<6 or start+length>end:raise ValueError('invalid 3DS chunk')
            yield tag,start+6,start+length
            start+=length
    def read(start,end):
        for tag,a,b in chunks(start,end):
            if tag==0x4000:
                end_name=data.index(b'\0',a,b)
                name=data[a:end_name].decode('latin1')
                item={'name':name,'points':[],'faces':[],'uv':[]}
                found.append(item)
                read(end_name+1,b)
            elif tag==0x4110:
                n=struct.unpack_from('<H',data,a)[0]
                found[-1]['points']=[struct.unpack_from('<fff',data,a+2+12*i) for i in range(n)]
            elif tag==0x4120:
                n=struct.unpack_from('<H',data,a)[0]
                found[-1]['faces']=[struct.unpack_from('<HHHH',data,a+2+8*i)[:3] for i in range(n)]
                read(a+2+8*n,b)
            elif tag==0x4140:
                n=struct.unpack_from('<H',data,a)[0]
                found[-1]['uv']=[struct.unpack_from('<ff',data,a+2+8*i) for i in range(n)]
            elif tag in (0x4d4d,0x3d3d,0x4100):read(a,b)
    read(0,len(data))
    return next(m for m in found if m['name'].lower()=='brid_1')

class UxBridge0518Audit(unittest.TestCase):
    def test_original_bridge_uses_six_opposing_atlas_face_pairs(self):
        mesh=original_visible_mesh(ROOT/'tests/fixtures/native_helpers/brid_1.3ds')
        faces=defaultdict(list)
        for face in mesh['faces']:
            xyz=[mesh['points'][j] for j in face]
            key=tuple(sorted(tuple(round(v,3) for v in p) for p in xyz))
            a,b,c=xyz
            u=tuple(b[i]-a[i] for i in range(3));v=tuple(c[i]-a[i] for i in range(3))
            cross=(u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0])
            avg_u=sum(mesh['uv'][j][0] for j in face)/3
            faces[key].append((cross,avg_u))
        self.assertEqual(len(mesh['faces']),12)
        self.assertEqual(len(faces),6)
        for pair in faces.values():
            self.assertEqual(len(pair),2)
            self.assertLess(sum(pair[0][0][i]*pair[1][0][i] for i in range(3)),0)
            self.assertGreater(abs(pair[0][1]-pair[1][1]),.25)
    def test_selective_culling_not_global_and_preserves_original_xml(self):
        imp=source('src/LegacyMeshImport.h');r=source('src/SceneRenderer.cpp')
        self.assertIn('HasOppositeFaceAtlas(',imp)
        self.assertIn('groups.size()*6 != indexCount',imp)
        self.assertIn('rd.CullMode = D3D11_CULL_NONE',r)
        self.assertIn('rd.CullMode = D3D11_CULL_BACK',r)
        self.assertIn('rd.FrontCounterClockwise = TRUE',r)
        self.assertIn('rasterizerPairedAtlas_',r)
        self.assertIn('context_->RSSetState(rasterizer_.Get())',r)
        self.assertNotIn('BRID_1.PNG',r) # generic detection, no hardcoded replacement
    def test_explanations_hover_and_placement_uncluttered(self):
        ui=source('src/EditorUi.cpp')
        lib=ui.split('void EditorUi::DrawLibrary(')[1].split('void EditorUi::CaptureBrowseThumbnail(')[0]
        self.assertIn('Placement settings##library',lib)
        self.assertIn('HoverHelp(',lib)
        self.assertNotIn('ImGui::DragFloat("Placement Z"',lib)
        self.assertNotIn('ImGui::DragFloat("Placement height (Z)"',lib)
        self.assertNotIn('ImGui::Button("Add at view focus")',lib)
        self.assertNotIn('Default embedded material / sprite',ui)
        self.assertNotIn('Complete imported helper set bound to object',ui)
        self.assertIn('Decoration: no physical collision',ui)
        self.assertIn('Save object changes?##objclose",nullptr,ImGuiWindowFlags_AlwaysAutoResize',ui)
        self.assertIn('"Add Light"',ui)

if __name__=='__main__':unittest.main()
