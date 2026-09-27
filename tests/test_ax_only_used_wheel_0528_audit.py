"""Prevent AX's visual only-used filter from being bypassed by native Tab+wheel."""
import unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
class AxOnlyUsedWheelAudit(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.ui=(ROOT/'src/EditorUi.cpp').read_text(encoding='utf-8')
        cls.cmake=(ROOT/'CMakeLists.txt').read_text(encoding='utf-8')
    def test_shared_filter_for_wheel_and_overlay(self):
        src=self.ui
        expression='AxRecentFilter::VisiblePositions(recentAssets_,assets.Assets(),map.Props(),axOnlyUsed_)'
        self.assertEqual(src.count(expression),2)
        wheel=src[src.index('    // AX only-used is a navigation filter'):src.index('    axTabWasHeld_=axTabHeld_;',src.index('    // AX only-used is a navigation filter'))]
        self.assertIn('ActivateRecent(visible[static_cast<size_t>(axCurrent_)],assets,previewScene);',wheel)
        self.assertIn('if (placementWheel_!=0.0f)',wheel)
        self.assertNotIn('ActivateRecent(static_cast<size_t>(axCurrent_)',wheel)
    def test_no_secondary_unfiltered_wheel_in_overlay(self):
        overlay=self.ui[self.ui.index('void EditorUi::DrawAxLibrary('):]
        self.assertNotIn('ImGui::GetIO().MouseWheel',overlay)
        self.assertNotIn('ActivateRecent(',overlay)
        self.assertIn('const auto visible=AxRecentFilter::VisiblePositions(',overlay)
    def test_filter_toggle_resets_row_and_tab_activation(self):
        self.assertIn('axCurrent_=0; axTabWasHeld_=false;',self.ui)
    def test_native_regression_is_built(self):
        self.assertIn('AxOnlyUsedWheelRegression',self.cmake)
        self.assertIn('AxOnlyUsedFiltersTabWheel',self.cmake)
if __name__=='__main__':unittest.main()
