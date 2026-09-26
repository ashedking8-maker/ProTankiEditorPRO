# 0.5.6 manual Windows acceptance checks

## Packaging / Defender

- Upload the complete source, including `.github`, to a fresh commit. Wait for both MSVC build and CTest (including `CollisionDeleteXMLAndHistory`).
- Download release `0.5.6` Setup and portable ZIP. Cross-check SHA-256 manifest in the same release.
- Do not disable Defender. Check Protection History for precise detection (name, file, timestamp) and submit suspect sample to Microsoft for analysis. Unsigned development binaries are not pre-cleared by this change.

## Test (external)

1. First click Test... and cancel: nothing runs or changes in the map or game files.
2. First click and choose an EXE with a different name: show error; do not persist.
3. Select the original ProTLVK32.exe, confirm launched with its own installation directory and no args; close it and click Test again: same target, no new file chooser.
4. Move/remove that EXE: next click must open a chooser. Verify no `maps.json` edits, no tank preview and no changes to active XML.

## Gameplay / zones

- Launch editor and open sandbox map: Gameplay tab active, mode None, no overlay. Collapsed Functional objects and Geometry sections.
- Enable overlays, enable a layer, then disable master. Re-enable master: all sublayers must still be off.
- Add a kill zone and select it. Drag it: full 3D cuboid follows cursor and no circular zone ghost; save/reopen, verify min/max values, Undo/Redo and Delete.

## Visual vs physical geometry

- Back up a test XML and perform Save As. Delete a uniquely co-located floor and collider, save/reopen. Verify both static prop and collision entries disappeared. Drive through in original tester.
- Delete a prop without unique exact-center collider: operation must refuse. Inspect Scene > Collision to remove specific primitives deliberately; repeat Save As and test. Off-origin/triangle collision may require this review.
- Move/rotate uniquely paired plane/box prop, save/reopen and verify both visual and physical transforms changed. Re-test in original game; no claim of universal prop/collision pairing.

## Object Editor (draft)

- Import a valid glTF2 GLB: edit XY bounds by dragging the rectangle; set Z min/max numerically. Save draft under library/__ObjectDrafts/name.
- Confirm neither original library.xml nor current map XML changed. The isolated draft is **not game-loadable** until the separate native 3DS/material/collision export is implemented.
