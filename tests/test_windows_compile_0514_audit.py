"""Preflight guards for two compilation defects from the 2026-09-25 MSVC log.

These tests are source checks, not a replacement for an MSVC Windows build.
"""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parent.parent


class WindowsCompile0514Audit(unittest.TestCase):
    def test_windows_near_macro_cannot_replace_collision_predicate(self):
        source = (ROOT / 'src/MapDocument.cpp').read_text(encoding='utf-8')
        repair = source.split('bool MapDocument::AddVerifiedCollisionForProp(size_t index) {', 1)[1].split(
            'bool MapDocument::DeleteProp(size_t index) {', 1)[0]
        self.assertNotRegex(repair, r'\b(?:auto|bool)\s+(?:near|far)\s*=')
        self.assertIn('auto nearVec3=[]', repair)
        self.assertEqual(len(re.findall(r'\bnearVec3\s*\(', repair)), 5)
        self.assertIn('sameYaw(c.rotation.z,t.rotation.z+p.rotation.z)', repair)

    def test_clipboard_bonus_does_not_redeclare_existing_vector(self):
        source = (ROOT / 'tests/functional_map_regression.cpp').read_text(encoding='utf-8')
        self.assertIn('const auto& pastedBonus=pasted.Bonuses().back();', source)
        self.assertNotIn('const auto& b=pasted.Bonuses().back();', source)
        self.assertIn('pastedBonus.bonusType!=copiedBonus.bonusType', source)
        self.assertIn('pastedBonus.max.x,copiedBonus.max.x', source)


if __name__ == '__main__':
    unittest.main()
