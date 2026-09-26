"""Guard against regressions uncovered by the September 2026 Windows logs."""
import pathlib
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]

class Ux058Audit(unittest.TestCase):
    def test_sprite_texture_is_bound_to_vertex_shader(self):
        src = (ROOT / "src/SceneRenderer.cpp").read_text(encoding="utf-8")
        self.assertIn("spriteTexture.GetDimensions(width, height)", src)
        regular = src.split("void SceneRenderer::RenderSprites(", 1)[1].split("void SceneRenderer::", 1)[0]
        self.assertIn("context_->VSSetShaderResources(0, 1, &srv)", regular)
        ghost = src.split("void SceneRenderer::RenderGhost(", 1)[1].split("void SceneRenderer::", 1)[0]
        self.assertIn("context_->VSSetShaderResources(0,1,&srv)", ghost)
    def test_geometry_key_and_palette(self):
        src = (ROOT / "src/EditorUi.cpp").read_text(encoding="utf-8")
        self.assertIn("case A::Grid: return {ImGuiKey_H};", src)
        self.assertIn("ImGui::IsKeyPressed(ImGuiKey_G,false)", src)
        self.assertIn("Add gameplay elements##palette", src)
        self.assertIn("functionalResizeBonusBase_", src)
        self.assertIn("functionalResizeZoneBase_", src)
    def test_quiet_library_success_not_failure(self):
        src = (ROOT / "src/App.cpp").read_text(encoding="utf-8")
        self.assertNotIn('SetMessage("Library indexed:', src)
        self.assertIn('SetMessage(err,true)', src)
    def test_bonus_modes_round_trip(self):
        src = (ROOT / "src/MapDocument.cpp").read_text(encoding="utf-8")
        self.assertIn("if(savedModes!=region.modes)", src)
        self.assertIn('dst.append_child("game-mode")', src)

if __name__ == "__main__": unittest.main()
