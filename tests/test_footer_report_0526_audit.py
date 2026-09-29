"""Regression checks for the compact report UI, click isolation and saved Z offset."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
UI = (ROOT / 'src' / 'EditorUi.cpp').read_text(encoding='utf-8')
HEADER = (ROOT / 'src' / 'EditorUi.h').read_text(encoding='utf-8')

class FooterReport0526Audit(unittest.TestCase):
    def test_footer_tabs_touch_and_block_viewport_input(self):
        self.assertIn('helpPos.y-bugHeight', UI)
        self.assertNotIn('helpPos.y-bugHeight-3.f', UI)
        self.assertIn('const bool overSupportTab=', UI)
        self.assertIn('ImGui::IsItemHovered() && !overSupportTab', UI)
        self.assertIn('mouseInside && !overSupportTab', UI)

    def test_report_is_single_description_and_privacy_opt_in(self):
        self.assertIn('Describe the bug or feedback', UI)
        self.assertNotIn('Describe the bug and steps to reproduce', UI)
        self.assertNotIn('Reporting service is not configured in this build.', UI)
        self.assertIn('std::string subject=details.substr', UI)
        self.assertIn('!BugReport::Configured()', UI)
        self.assertIn('bool bugReportAttachLogs_{}', HEADER)

    def test_offset_default_and_preferences(self):
        self.assertIn('bool surfaceOffsetEnabled_{false}', HEADER)
        self.assertIn('float surfaceOffsetZ_{0.5f}', HEADER)
        self.assertIn('tag=="nativeSurfaceOffsetEnabled"', UI)
        self.assertIn('tag=="surfaceOffsetZ"', UI)
        self.assertIn('out<<"nativeSurfaceOffsetEnabled "', UI)
        self.assertIn('out<<"surfaceOffsetZ "', UI)
        self.assertIn('Native collision moves with the object', UI)

if __name__ == '__main__':
    unittest.main()