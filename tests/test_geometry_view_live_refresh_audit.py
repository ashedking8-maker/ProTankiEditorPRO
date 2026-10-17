"""Source-level guard: entering G must refresh actual XML collision GPU data."""
import pathlib
import re
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]


class GeometryViewRefreshAudit(unittest.TestCase):
    def test_scene_renderer_has_independent_collision_refresh(self):
        head = (ROOT / "src/SceneRenderer.h").read_text(encoding="utf-8")
        body = (ROOT / "src/SceneRenderer.cpp").read_text(encoding="utf-8")
        self.assertIn("void RefreshCollisionGeometry(const MapDocument& map);", head)
        self.assertRegex(body, r"void SceneRenderer::RefreshCollisionGeometry\([^)]*\)\s*\{[^}]*BuildCollisionGeometry\(map\)")
        # A full BuildScene must not be required for one modified collider.
        refresh = body.split("void SceneRenderer::RefreshCollisionGeometry", 1)[1].split("void SceneRenderer::BuildCollisionGeometry", 1)[0]
        self.assertNotIn("BuildScene(", refresh)
        self.assertNotIn("SetPropTransform", refresh)

    def test_all_three_geometry_entry_points_refresh(self):
        ui = (ROOT / "src/EditorUi.cpp").read_text(encoding="utf-8")
        for key in ('ImGui::MenuItem("Geometry / collision view (G)"',
                    'ImGui::IsKeyPressed(ImGuiKey_G,false)',
                    'ToolButton("Geometry view [G]"'):
            tail = ui.split(key, 1)[1][:850]
            self.assertIn("scene.RefreshCollisionGeometry(map);", tail, key)

    def test_edits_and_history_refresh_while_visible(self):
        ui = (ROOT / "src/EditorUi.cpp").read_text(encoding="utf-8")
        live = ui.split("void EditorUi::ApplyLiveTransform", 1)[1].split("void EditorUi::PushDiscreteTransform", 1)[0]
        self.assertIn("if (showCollision_) scene.RefreshCollisionGeometry(map);", live)
        for method, following in (("Undo", "Redo"), ("Redo", "DeleteSelected")):
            excerpt = ui.split(f"void EditorUi::{method}(", 1)[1].split(f"void EditorUi::{following}(", 1)[0]
            self.assertIn("scene.RefreshCollisionGeometry(map);", excerpt)


if __name__ == "__main__":
    unittest.main()
