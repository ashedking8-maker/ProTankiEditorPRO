"""Catch repeated Object Editor declarations before the Windows compiler."""
import re
import unittest
from pathlib import Path

HEADER = Path(__file__).resolve().parents[1] / "src" / "EditorUi.h"
METHODS = ("PushObjectUndo", "UndoObject", "RedoObject", "ResetObjectHistory")

class EditorUiHeaderAudit(unittest.TestCase):
    def test_object_history_has_one_declaration_per_method(self):
        source = HEADER.read_text(encoding="utf-8")
        for method in METHODS:
            with self.subTest(method=method):
                self.assertEqual(len(re.findall(r'^\s*void\s+' + method + r'\s*\(\s*\)\s*;', source, re.MULTILINE)), 1)

if __name__ == "__main__":
    unittest.main()
