from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parents[1]

class LegacyHandedness0523(unittest.TestCase):
    def test_map_position_keeps_legacy_y_sign(self):
        s=(ROOT/'src/LegacyTransform.h').read_text()
        self.assertIn('return {legacy.x, legacy.z, legacy.y};',s)
        self.assertIn('return {internal.x, internal.z, internal.y};',s)
        self.assertNotIn('return {legacy.x, legacy.z, -legacy.y};',s)

    def test_mesh_and_map_use_same_basis(self):
        mesh=(ROOT/'src/LegacyMeshImport.h').read_text()
        self.assertIn('(x,z,y)',mesh)
        self.assertIn('const aiMatrix4x4 basis(1,0,0,0, 0,0,1,0, 0,1,0,0, 0,0,0,1);',mesh)

    def test_overlay_and_edge_snap_follow_same_y_sign(self):
        scene=(ROOT/'src/SceneRenderer.cpp').read_text()
        self.assertIn('p.z+std::sin(angle)*180',scene)
        self.assertIn('legacyDy=snap.y;',scene)
        self.assertNotIn('legacyDy=-snap.y;',scene)

    def test_camera_workaround_was_removed(self):
        scene=(ROOT/'src/SceneRenderer.cpp').read_text()
        hdr=(ROOT/'src/SceneRenderer.h').read_text()
        self.assertIn('cameraYaw_=-0.75f;',scene)
        self.assertIn('float cameraYaw_ = -0.75f;',hdr)
        self.assertNotIn('cameraYaw_=2.39159265f;',scene)

if __name__=='__main__': unittest.main()
