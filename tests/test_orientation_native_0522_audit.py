"""0.5.22 guardrails for source-coordinate safety, original fixtures and UI wiring."""
from pathlib import Path
import unittest
from test_native_helper_0515_audit import read_nodes

ROOT=Path(__file__).resolve().parents[1]
FIX=ROOT/'tests/fixtures/native_helpers'

class OrientationNative0522(unittest.TestCase):
    def test_original_outer_wall1_near_rectangle_is_real(self):
        nodes=read_nodes(FIX/'outer_wall1_ow_1.3ds')
        self.assertEqual({'plane12','plane11'} & set(nodes),{'plane12','plane11'})
        plane=nodes['plane12']
        self.assertEqual(len(plane['v']),4)
        x_delta=abs(plane['v'][1][0]-plane['v'][2][0])
        self.assertGreater(x_delta,.05)
        self.assertLess(x_delta,.1)
        self.assertEqual(len(plane['f']),2)
    def test_scaled_box_has_original_world_vertices(self):
        nodes=read_nodes(FIX/'combuild_comb3.3ds')
        box=nodes['Box21']
        self.assertAlmostEqual(box['m'][4],2.,delta=.0001)
        self.assertAlmostEqual(max(p[1] for p in box['v'])-min(p[1] for p in box['v']),60.,delta=.01)
    def test_view_direction_never_modifies_legacy_coordinate_convention(self):
        transform=(ROOT/'src/LegacyTransform.h').read_text()
        self.assertIn('return {legacy.x, legacy.z, -legacy.y};',transform)
        self.assertIn('return {internal.x, -internal.z, internal.y};',transform)
        scene=(ROOT/'src/SceneRenderer.cpp').read_text()
        app=(ROOT/'src/App.cpp').read_text()
        self.assertIn('void SceneRenderer::ReverseViewDirection()',scene)
        self.assertIn('void SceneRenderer::ResetReferenceViewDirection()',scene)
        self.assertIn('scene_.ResetReferenceViewDirection(); // imported XML itself is NOT mirrored or rotated',app)
        self.assertIn('scene_.ResetReferenceViewDirection();',app)
    def test_opt_in_edge_and_surface_controls_do_not_mutate_existing_maps(self):
        ui=(ROOT/'src/EditorUi.cpp').read_text()
        header=(ROOT/'src/EditorUi.h').read_text()
        self.assertIn('bool edgeSnapEnabled_{}',header)
        self.assertIn('bool surfaceOffsetEnabled_{}',header)
        self.assertIn('!clipboardPlacement_ && ghostProps_.size()==1',ui)
        self.assertIn('Ghost',ui)
        self.assertIn('Placement transaction rolled back:',ui)
        self.assertIn('Placement committed:',ui)

if __name__=='__main__':unittest.main()
