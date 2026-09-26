# ProTanki Editor PRO 0.5.20 — Original 3DS frame transforms (source candidate)

This is a **changed-files-only** patch for the user's exact uploaded 0.5.19 GitHub repository. Preserve the directory structure when applying it. It is not a prebuilt installer.

## What changed

- `NativeCollisionImport` now validates an orthonormal right-handed 3DS `0x4160` matrix instead of rejecting all nonidentity matrices. The complete source vertex geometry is converted from 3DS authoring coordinates into the **visual object's local frame**. Rotated plane and triangle helper coordinates are not replaced by guessed visual bounds.
- `box*` vertices are projected onto the box helper's own axes, so an originally rotated box retains its X/Y/Z dimensions and orientation. The source is not collapsed to a world-axis AABB. The helper's frame is expressed relative to the visual pivot before map yaw is applied.
- The original map's box owners are rebound by all eight transformed world corners, rather than by one particular Euler decomposition. Overlapping ambiguous matches remain unbound.
- A 3DS containing **no plane/box/triangle helper at all** may be placed as a visual-only original asset without enabling the override intended for *unsupported* helper geometry. Existing per-instance `<with_collision>1</with_collision>` still blocks that fallback. Models with present but invalid helper geometry still fail closed.
- Added original-source `Waffle Fence / Wall 1` (rotated visual pivot) and `Promotion / Billboard` (rotated boxes) 3DS and original game-map XML fixtures, plus regression assertions for bind, move and delete. Prior Wall End 1 / Hs_part06 / Bridge 1 / NuBu 3 tests remain.

## Evidence, scope and remaining work

Static C++20 importer scan of the 1,432 original 3DS files: **1,048** accepted with native primitives, compared with **931** in 0.5.19 (117 additional, zero previous accepts regressed). **319** return *no native collision helpers*; those may be deliberate decorations and are not reported as supported colliders. **65** still fail technical validation (missing anchor, nonrectangular plane, non-box helper, invalid frames, malformed models and special cases). Acceptance here means importer geometry accepted, **not that the game was tested**.

Unknown `library.xml` fields and original map XML continue to be retained by the pre-existing lossless snapshot path. Their semantics and safe creation of arbitrary new per-instance fields are not guaranteed. New GLB drafts still have **no validated 3DS/library.xml native game exporter**. No changes were made to Bridge 1 rendering in this patch.

Portable importer compilation and independent Python original-map fixture comparisons pass. Full Windows/MSVC, CTest, installer and ProTLVK tank tests must be run from GitHub Actions and on a backup map. **Do not treat this as a final universal replacement of the original editor.**
