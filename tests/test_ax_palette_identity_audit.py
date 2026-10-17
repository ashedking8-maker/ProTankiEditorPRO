"""Source-level regression of AX activation and clipboard isolation."""
from pathlib import Path
import unittest
ROOT=Path(__file__).resolve().parents[1]
class AxPaletteAudit(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.ui=(ROOT/'src/EditorUi.cpp').read_text(encoding='utf-8')
        cls.header=(ROOT/'src/EditorUi.h').read_text(encoding='utf-8')
        cls.cmake=(ROOT/'CMakeLists.txt').read_text(encoding='utf-8')
    def test_copy_does_not_modify_ax(self):
        self.assertNotIn('RememberCopiedAssets(',self.ui)
        self.assertNotIn('RememberCopiedAssets(',self.header)
        self.assertIn('StartClipboardPlacement();',self.ui)
    def test_ax_click_activates_instead_of_removing(self):
        overlay=self.ui[self.ui.index('void EditorUi::DrawAxLibrary('):]
        self.assertIn('if (choose)',overlay)
        self.assertIn('ActivateRecent(at,assets,previewScene);',overlay)
        self.assertIn('if (ImGui::SmallButton("X##ax_remove"))',overlay)
        self.assertIn('if (ImGui::Button("Remove##ax_confirm"))',overlay)
        self.assertIn('recentAssets_.erase(recentAssets_.begin()+axRemovalIndex_)',overlay)
        self.assertNotIn('RMB confirms',overlay)
    def test_stable_pins_and_variant(self):
        self.assertIn('AxRecentFilter::Touch(recentAssets_,index);',self.ui)
        self.assertIn('AxRecentFilter::Pin(recentAssets_,index,variant);',self.ui)
        self.assertIn('recentAssets_[recentAt].textureVariant!=selectedTextureVariant_',self.ui)
        self.assertIn('AX asset activated:',self.ui)
        self.assertIn('AxPaletteIdentityRegression',self.cmake)
if __name__=='__main__':unittest.main()
