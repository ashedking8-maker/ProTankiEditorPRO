# Post-test UI patch for ProTanki Editor PRO 0.5.26

Source-only patch based on `ProTankiEditorPRO-0.5.26-FULL-SOURCE-BUILDFIX.zip` (includes the previous MSVC `FloatRgbChunk` fix).

Changes:
- "Report a Bug" tab touches the Discord tab and its hit rectangle blocks click-through into the 3D viewport.
- Report modal has one text area, "Describe the bug or feedback". Subject is derived from the first line. Optional logs remain off by default; network submission stays disabled when no endpoint is configured.
- "Z-offset protection (new objects)" defaults to on, at 0.5 units, and persists to controls settings. This moves the **whole newly placed object and its collision**, not only the surface/material. It does not modify existing objects/maps; it does not apply to clipboard copies. Disable it for exact original-map comparisons or when physical contact/stacking matters.
- Removes redundant "Open gameplay inspector..." button from Scene. Gameplay dock remains; View menu still focuses it.
- Updates original audit for the intentional default change and adds UI regression source audit.

Validation performed here: `python tools/check_source_consistency.py` PASS; `python -m unittest discover -s tests -p "test_*audit.py" -q` PASS (57 tests). These are source/static Python checks; the modified UI has **not** been compiled with MSVC or tested in ProTLVK here. Confirm Windows GitHub Actions build and CTest before installing or treating this as a release.

To apply: extract the PATCH archive at the **root of the existing repository**, preserving directories, and commit the files. Do not make a new repository.
