# Milestone 2 — GPU viewport interaction

This milestone turns the Direct3D 11 viewport from a passive renderer into the first interactive editor surface.

## Added

- Hardware D3D11 rendering of legacy mesh and sprite batches retained from M1.
- Per-prop world-space picking proxies generated while the GPU scene is built.
- Left-click viewport selection using a camera ray against prop bounds.
- Selection is synchronized with the Scene tree.
- Selected prop is drawn with a thin blue 3D bounding-box outline; no glow or modern card-like overlay.
- Double-click or `F` frames the current selection.
- `Esc` clears viewport selection.
- Existing camera controls remain: RMB orbit, Shift+RMB/MMB pan, mouse wheel zoom.

## Implementation notes

Picking currently uses world-space AABBs. This is intentional for the first interactive milestone: it is inexpensive and gives predictable selection on thousands of objects. A later milestone can add mesh-level triangle picking only for the small candidate set returned by the broad-phase structure.

The next performance/editor milestone should add:

1. BVH / spatial index over picking proxies and scene instances.
2. Frustum culling with visible-instance upload buffers.
3. Editable transform commands (move/rotate) with undo/redo.
4. Selection gizmo and snap-to-grid.
5. Collision visualization and picking.
6. Map serialization back to the original ProTanki XML format.
