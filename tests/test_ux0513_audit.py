from pathlib import Path
import re
import unittest
ROOT=Path(__file__).resolve().parent.parent

def content(path):return (ROOT/path).read_text(encoding="utf-8")

class Ux0513Audit(unittest.TestCase):
    def test_grid_phase_and_drag_copy(self):
        ui=content("src/EditorUi.cpp")
        self.assertIn("clipboardAnchor_=center",ui)
        self.assertIn("GridStep::QuantizeAroundAnchor(target.x",ui)
        self.assertIn("gridSize_,clipboardAnchor_.y",ui)
        self.assertIn("clipboardPlacement_=false;",ui)
        self.assertIn("if(GridStep::QuantizeAroundAnchor(749.f,100.f,250.f)!=750.f)",content("tests/grid_step_regression.cpp"))

    def test_gameplay_paste_fields(self):
        ui=content("src/EditorUi.cpp")
        self.assertIn("functionalClipboardBonus_=map.Bonuses()[index]",ui)
        self.assertIn("auto item=functionalClipboardBonus_",ui)
        self.assertIn("map.AddBonusRegion(item)",ui)
        self.assertIn("auto item=functionalClipboardSpawn_",ui)
        self.assertIn("map.AddSpawn(item)",ui)
        self.assertIn("functionalPasteActive_=true;placementZ_=functionalClipboardAnchor_.z",ui)

    def test_ux_modals_and_persistence(self):
        ui=content("src/EditorUi.cpp")
        self.assertIn('"Grid line RGB"',ui)
        self.assertIn('"viewportGridColor "',ui)
        self.assertIn('"smoothCameraFocus "',ui)
        self.assertIn('"Save object changes?##objclose"',ui)
        self.assertIn('"Fullscreen (F11)"',ui)
        self.assertNotIn('ImGui::TextUnformatted("Unsaved map changes")',content("src/App.cpp"))

    def test_no_duplicate_object_methods(self):
        hdr=content("src/EditorUi.h")
        for method in ("PushObjectUndo","UndoObject","RedoObject","ResetObjectHistory"):
            self.assertEqual(len(re.findall(r"^\s*void\s+"+method+r"\s*\(\s*\)\s*;",hdr,re.M)),1)

if __name__=="__main__":unittest.main()
