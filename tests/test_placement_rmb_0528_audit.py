"""Emergency regression guards: active placement owns RMB ahead of AX or viewport hover."""
import unittest
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
class RmbPlacement0528Audit(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.ui=(ROOT/'src/EditorUi.cpp').read_text(encoding='utf-8')
        cls.hdr=(ROOT/'src/EditorUi.h').read_text(encoding='utf-8')

    def test_global_priority_precedes_docked_widgets(self):
        ui=self.ui
        self.assertLess(ui.index('const bool cancelPlacementByRmb'),ui.index('HandleEditorShortcuts(map, scene, assets);'))
        self.assertLess(ui.index('const bool cancelPlacementByRmb'),ui.index('DrawViewport(map, assets, scene);'))
        self.assertIn('placementActive_ || lightPlacementActive_ || functionalPlacement_!=FunctionalPlacement::None',ui)
        self.assertIn('rmbPlacementCancelPendingRelease_=true;',ui)
        self.assertIn('bool rmbPlacementCancelPendingRelease_',self.hdr)

    def test_cancellation_clears_all_ghost_modes_and_pending_commit(self):
        ui=self.ui; start=ui.index('if (cancelPlacementByRmb) {'); end=ui.index('HandleEditorShortcuts(map, scene, assets);',start)
        region=ui[start:end]
        for token in ('placementActive_=false;', 'placementCommitRequested_=false;',
                      'ghostValid_=false;', 'ghostProps_.clear();', 'scene.ClearGhost();',
                      'functionalPlacement_=FunctionalPlacement::None;',
                      'functionalCommitRequested_=false;', 'functionalPasteActive_=false;',
                      'scene.SetFunctionalGhost({},0,false);', 'lightPlacementActive_=false;',
                      'axConfirmRemove_=false;', 'axRemovalIndex_=-1;'):
            self.assertIn(token,region,token)

    def test_no_second_rmb_route_or_ghost_camera_or_selection_side_effect(self):
        ui=self.ui
        self.assertNotIn('ImGui::IsMouseClicked(ImGuiMouseButton_Right) && !axConfirmRemove_',ui)
        self.assertIn('hovered && !rmbPlacementCancelPendingRelease_ && ImGui::IsMouseReleased',ui)
        self.assertIn('if (!rmbPlacementCancelPendingRelease_ && !placementActive_ &&',ui)
        self.assertIn('if (!ImGui::IsMouseDown(ImGuiMouseButton_Right)) rmbPlacementCancelPendingRelease_=false;',ui)
        self.assertIn('if (!targetVisible && axConfirmRemove_)',ui)
        self.assertIn('ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows)',ui)

    def test_actual_installer_workflow_uses_non_deprecated_action(self):
        workflow=(ROOT/'.github/workflows/build-windows-installer.yml').read_text(encoding='utf-8')
        self.assertIn('actions/setup-node@v5', workflow)
        self.assertNotIn('actions/setup-node@v4', workflow)
        release=(ROOT/'.github/workflows/release-windows.yml').read_text(encoding='utf-8')
        self.assertIn('actions/setup-node@v5', release)
        self.assertNotIn('actions/setup-node@v4', release)

if __name__=='__main__': unittest.main()
