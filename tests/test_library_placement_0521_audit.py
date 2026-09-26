"""Regression checks for original scaled-plane fixtures and input/UI routing.

These are source-level guards plus geometric original-3DS validation; Windows
CTest tests the C++ reader directly against the same binary fixtures.
"""
import math
import unittest
from pathlib import Path
from test_native_helper_0515_audit import read_nodes

ROOT=Path(__file__).resolve().parents[1]
FIX=ROOT/'tests/fixtures/native_helpers'

class LibraryPlacement0521(unittest.TestCase):
    def test_original_tower_has_nonrigid_plane_matrices_and_valid_rectangles(self):
        for name in ('fab_tow.3ds','fab_tow2.3ds'):
            nodes=read_nodes(FIX/name)
            planes=[node for key,node in nodes.items() if key.lower().startswith('plane')]
            self.assertEqual(len(planes),6,name)
            for node in planes:
                m=node['m']; norm=lambda v:math.sqrt(sum(x*x for x in v))
                self.assertAlmostEqual(norm(m[6:9]),1.64,delta=.001)
                self.assertEqual(len(node['v']),4)
                self.assertEqual(len(node['f']),2)
                a,b,c,d=node['v']
                points=node['v']
                self.assertEqual(len(set(points)),4)
                # A rectangle has the same diagonal midpoint regardless of
                # vertex order; pair diagonals by their maximal distance.
                pairs=sorted(((sum((points[i][k]-points[j][k])**2 for k in range(3)),i,j)
                              for i in range(4) for j in range(i+1,4)), reverse=True)
                _,i,j=pairs[0];other=[k for k in range(4) if k not in (i,j)]
                for k in range(3):
                    self.assertAlmostEqual(points[i][k]+points[j][k],
                                           points[other[0]][k]+points[other[1]][k],delta=.1)
    def test_source_uses_world_geometry_for_scaled_planes_and_still_guards_boxes(self):
        s=(ROOT/'src/NativeCollisionImport.h').read_text()
        self.assertIn('if((box && !scaledBox) || !n.hasMatrix',s)
        self.assertIn('if(i1<0) {result.error="Nonrectangular 3DS plane helper:',s)
        self.assertIn('fab_tow.3ds', (ROOT/'tests/native_3ds_helper_regression.cpp').read_text())
    def test_mousewheel_requires_tab_and_no_global_placement_capture(self):
        s=(ROOT/'src/EditorUi.cpp').read_text()
        self.assertIn('if (!tab || browseLibraryOpen_) return false;',s)
        self.assertIn('if (axTabHeld_ && placementWheel_!=0.0f',s)
        self.assertIn('const bool scroll=axTabHeld_;',s)
        self.assertNotIn('axTabHeld_ || placementActive_ || overPinnedAx',s)
    def test_browser_preserves_texture_variants_and_flattened_display(self):
        s=(ROOT/'src/EditorUi.cpp').read_text()
        for token in ('struct BrowseTile { size_t asset, variant; };',
                      'CaptureBrowseThumbnail(index,tile.variant,assets,previewScene)',
                      'selectedTextureVariant_=static_cast<int>(tile.variant)',
                      'const uint64_t key=', 'a.group+" / "+a.name'):
            self.assertIn(token,s)
        self.assertNotIn('ImGui::TreeNodeEx(groupLabel.c_str()',s)
    def test_absolute_grid_and_linked_colliders_are_protected(self):
        s=(ROOT/'src/EditorUi.cpp').read_text()
        self.assertIn('after.position.x=GridStep::Quantize(after.position.x,cell);',s)
        self.assertIn('if(values[i].authoredOwnerIndex>=0)',s)
        self.assertIn('Log::Info("Map Undo applied;',s)

if __name__=='__main__': unittest.main()
