# ProTanki Editor PRO 0.5.23 — legacy handedness and orientation fidelity

Based on 0.5.22, **changed files only**. Windows source candidate; not a compiled installer.

## Main fix

0.5.22 changed the opening camera direction, but the supplied Silance comparison and 0.5.22 runtime log show the real problem was deeper: the renderer converted original GTanks map space with `(x,y,z) -> (x,z,-y)`. That negated legacy Y, mirroring the complete map in the viewport and reversing the apparent yaw of asymmetric props.

0.5.23 changes the display/import basis to the left-handed mapping `(x,y,z) -> (x,z,y)` consistently for:

- loaded prop positions,
- imported 3DS visual vertices,
- prop rotation/world matrices,
- collision preview transforms,
- sprites and gameplay markers,
- picking and camera-to-legacy conversion,
- geometry Edge Snap deltas.

The temporary 0.5.22 180-degree opening-camera workaround is removed; the reference opening yaw is restored to the 0.5.21 value. XML data itself is not rewritten merely because a map is opened.

## Important compatibility note

This patch intentionally changes only the **viewport/internal coordinate conversion**. Legacy XML positions and rotations remain stored in their original GTanks values. Saving an untouched legacy map should therefore not bulk-flip coordinates or inject 180-degree rotations.

The Bridge-rise test from earlier remains useful: its source XML is not forcibly rotated. The purpose of this fix is to make the viewport interpret the same legacy coordinates with the same handedness as the original editor / ProTLVK, rather than compensating with camera rotation.

## Preserved 0.5.22 work

The Outer Wall 1 near-rectangular helper support, scaled orthogonal helper boxes, placement rollback logging, opt-in Edge Snap and surface offset remain in place.

## Validation status

- Python test suite: 54 tests PASS in the preparation environment.
- `tools/check_source_consistency.py`: PASS.
- New handedness regression checks verify map positions, mesh basis, spawn heading, Edge Snap Y direction, and removal of the 0.5.22 camera workaround.
- Windows MSVC build/CTest and visual comparison in ProTLVK/original GTanks Editor still need to be run on Windows.

Apply this patch over a complete **0.5.22** repository.
