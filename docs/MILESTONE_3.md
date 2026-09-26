# Milestone 3 — transform editing + safe legacy save

## Added

- Editable static-prop transforms.
- Viewport Move mode: left-drag moves a prop on its current legacy Z plane.
- Viewport Rotate mode: horizontal left-drag rotates around legacy Z.
- Position snap/grid step, default 500 legacy units.
- Rotation snap, default 90 degrees.
- Familiar legacy hotkeys: `W/S` height and `Z/X` Z rotation; Shift applies a fine 0.1 step multiplier.
- `1 / 2 / 3` tool switching: Select / Move / Rotate.
- Editable Position and Rotation Z values in Properties.
- Undo/redo transform command history (`Ctrl+Z`, `Ctrl+Y`).
- `Ctrl+S` Save and `Ctrl+Shift+S` Save As.
- Dynamic D3D11 instance buffers and per-prop render bindings so editing one object does not rebuild the scene.
- Picking bounds update immediately after a transform.
- Dirty-document indicator in the status bar.
- Legacy master XML preservation and compatibility guard.
- Atomic map writes.
- Developer compatibility verifier in `tools/verify_legacy_xml.py`.

## Legacy output rule

The map format is not being replaced. A saved file remains a ProTanki legacy XML map. See `LEGACY_OUTPUT_COMPATIBILITY.md`.

Milestone 3 deliberately changes only static prop transforms. Asset identity/material and all unsupported gameplay/collision sections remain sourced from the loaded legacy XML. This minimizes the chance of producing a map that looks correct in the editor but cannot be used by the game.

## Still next

- Visible 3-axis move/rotate handles with per-axis drag constraints.
- Copy/paste/duplicate/delete with serializer support and compatibility tests.
- Exact mesh-ray picking after broad-phase AABB hit.
- Collision/gameplay entity rendering and editing.
- Map validator that reports unresolved libraries/textures and invalid gameplay references before export.
- Windows-native build/run validation and profiling on a real large map.
