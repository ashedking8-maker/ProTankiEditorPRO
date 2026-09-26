# Workflow 3 verification plan

Source preflight (`python tools/check_source_consistency.py`) validates complete expected translation units, matching release metadata, resource path and the single-line native splash. This is a static smoke test, not a Windows build.

Windows CI builds Release via MSVC, runs `LegacyImportRegression`, `LegacyMapEditRegression`, and `FunctionalXMLAndGroupHistory` tests, and packs Setup EXE plus portable ZIP. The new functional regression exercises editing/adding/deleting native CTF, spawns, DOM, bonuses and zones; preserving unknown collision/waypoint metadata; saving twice; and a two-prop group Undo/Redo transaction. For original maps use `tools/verify_legacy_xml.py` only on static-prop-only edits, because it deliberately rejects intended functional-section edits.

Interactive Windows smoke-test: check startup splash before editor shows; one changing text line; verify it stays until viewport renders. Load original external library and a COPY of Sandbox. Click a Library asset and ensure ghost immediately appears under mouse, Space stamps, RMB cancels. Rotate camera 90 degrees and verify camera-relative WASD. LMB move, selection rectangle, Ctrl+LMB additive selection, Ctrl+C/V ghost group and Space. Hold Tab, scroll recent history, release Tab. In Gameplay select/show/move flags, spawns and zones, Save As and reopen. Compare native XML tags and untouched collision data.

Known limitations: no local MSVC in source-generation environment, so the Windows CI/runtime are the authoritative tests. Functional Add/Delete has no dedicated undo transaction yet; ghosts/AX layout require graphics/human testing. Gameplay marker visuals are representative editor primitives, not a recovered original AIR mesh rendering.
