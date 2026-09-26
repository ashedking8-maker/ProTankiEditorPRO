"""Preflight the C++ functional test fixture before invoking an MSVC build.

FunctionalMapRegression's serializer requires a static-geometry section. A
historical test fixture omitted it and silently returned 106 under CTest.
"""
from pathlib import Path
import re
import unittest
import xml.etree.ElementTree as ET


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "tests" / "functional_map_regression.cpp"


class FunctionalFixtureAudit(unittest.TestCase):
    def test_unspecified_modes_fixture_is_a_saveable_map(self):
        text = SOURCE.read_text(encoding="utf-8")
        matches = re.findall(r'R"XML\((<map version="1\.0\.Light"[^\n]*<bonus-type>nitro</bonus-type>[^\n]*</map>)\)XML"', text)
        self.assertEqual(len(matches), 1)
        root = ET.fromstring(matches[0])
        self.assertIsNotNone(root.find("static-geometry"), "Serializer requires <static-geometry>")
        self.assertIsNotNone(root.find("collision-geometry"))
        bonus = root.find("bonus-regions/bonus-region")
        self.assertIsNotNone(bonus)
        self.assertEqual(bonus.findall("game-mode"), [], "Unspecified modes must remain absent")

    def test_failure_checkpoint_is_printed_by_native_test(self):
        text = SOURCE.read_text(encoding="utf-8")
        self.assertIn("FAIL FunctionalMapRegression checkpoint", text)
        self.assertIn("return Fail(106)", text)


if __name__ == "__main__":
    unittest.main()
