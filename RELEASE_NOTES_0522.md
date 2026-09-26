# ProTanki Editor PRO 0.5.22 — camera orientation, source helpers and edge precision

Based on 0.5.21, **changed files only**. It is a Windows source candidate, not a compiled installer.

## Fixed and added

- The initial viewport camera now views the map from the other side (180° change from 0.5.21), matching the side of the supplied original-editor Silance comparison more closely. **No positions, mesh winding, rotations, physics, XML fields or map coordinates are flipped.** View menu provides "Reference-side camera direction" and "View from opposite side (180 degrees)" so the direction can be checked without altering a map. This change is camera-only and is NOT proof of exact original-editor camera parity for arbitrary saved viewpoints.
- Original Outer Wall 1 `ow_1.3ds` helper surfaces (Outer Walls / Lair / Winter / h25Death) now import as **two near-rectangular planes**. These originals have about 0.0586 source-unit edge discrepancy; conversion accepts only <0.1 absolute and <0.05% relative closure errors, with the existing orthogonality/face guards. The original 3DS is added as a regression fixture.
- Original `ComBuild/comb3.3ds` orthogonal, positively scaled box-helper frame now yields a box from actual source vertices. A sheared/mirrored box still fails closed. The original asset is a regression fixture.
- Placement rollback now explicitly logs the asset/reason and makes clear that an earlier "Prop added" line represented a staged transaction. A successful placement logs "Placement committed" separately.
- Tools → Placement settings adds **opt-in** single-object geometry edge snap for 0/90/180/270° props, using post-transform render bounds, with tolerance and optional horizontal gap. Existing maps are never automatically bulk-snapped; clipboard groups and diagonal models are excluded. This AABB edge mode is intended for rectangular tiles/walls/ramp bounds, not arbitrary polygon-to-polygon snapping.
- Tools → Placement settings adds **opt-in** new-object surface Z offset. It moves the entire placed object AND its imported collision geometry together; it does not create an independent texture-only decal. Default is OFF. Do not use it to raise a solid surface when physical height should stay unchanged.
- Adds C++ edge/floor/clearance checks and native-3DS import fixtures.

## Audit and limits

Against the **1,432 unique original 3DS files** extracted from the supplied source package: native helper reader reports 1,068 importable files, 319 without native collision helpers, and 45 rejected/unsupported files. This counts files, not the 1,493 separate library asset entries and not game-certified behavior. Previous 0.5.21 importer reported 1,052 / 319 / 61 on the same set. Exact per-model outcomes are in the included audit table.

This is **not** full support of all original library objects. Unknown visual anchors, genuinely nonrectangular planes and non-box-shaped helpers still fail closed. No general polygon edge optimizer or separate visual-only depth offset has been implemented. C++ source preflight, Python tests, portable geometry test and the original 3DS audit passed in the preparation environment. **Windows MSVC build/CTest and ProTLVK gameplay still require running on GitHub/Windows.**

## Installation and quick check

Apply over a **complete 0.5.21 repository**, keep directory structure and overwrite same-name files. Do not apply directly to 0.5.20. Use Save As for legacy maps. Build via the existing Windows GitHub Actions workflow and test Outer Wall 1 placement, Silance camera direction and an untouched legacy-map XML save comparison. Undo remains usable for authored geometry.
