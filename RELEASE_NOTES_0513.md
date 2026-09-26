# ProTanki Editor PRO 0.5.13 – grid, gameplay clipboard and UX candidate

Based on the previously patched 0.5.12 full source (including the MSVC C2535 header fix).

- Viewport background dialog now edits a persistent D3D grid-line color. All independent previews use it.
- Copied static props retain the original first prop as an anchor; cursor movement snaps *deltas* on the selected step, keeping a 250-centred legacy tile on the same grid phase. Group-relative offsets and rotations are retained. Original collider ownership is still ambiguous and this does NOT claim to clone game collision automatically.
- Ctrl+C/V of selected native gameplay spawns, flags, control points, bonus regions and special zones uses a mouse-following ghost and preserves the native parsed properties, including bonus type, game modes, free/parachute, spawn heading and zone action. Native flags remain unique by team. Space/LMB stamps; RMB cancels. Copying original XML unknown extension nodes into newly authored gameplay items is not guaranteed.
- Selected bonus displays its native type on its own bracketed line. Audit of ProTLVK map XML also found `crystal_500` and bonus mode `as`; both are supported as exact native tokens in creation/Properties (the latter under the All filter, with no assumed mechanics).
- Object Editor first-use window is larger and centred. Redundant instructions and Undo/Redo buttons removed; Ctrl+Z/Y remain functional. Closing an unsaved draft prompts Save as new draft, Discard or Cancel. Native 3DS/library.xml game export remains gated.
- Duplicate Unsaved map changes heading removed. On first run start maximized, welcome dialog is more translucent and opens viewport background dialog when dismissed. Subsequent launches keep normal OS show preference. F11 appears in View menu.
- Optional smooth focus on static-prop selection, in navigation controls; can be switched off and persisted. Manual camera navigation cancels pending focus animation.

## Test scope and limitations

Python source preflight and audit tests, standalone GridStep / GameplayAuthoring tests and archive-integrity checks can be run locally. The Windows MSVC GUI build, full CTest, NSIS packaging and in-game ProTLVK compatibility must still be verified via GitHub Actions and a local Windows run.

The supplied 308 MB ProTLVK archive contains runtime/map data, not the original map-editor source. A scan of its map XML found 201 maps, 400 `omni` lights, 4,246 bonus regions, two maps with `<asl-point>`/`<asl-flags>`, and native `as` mode and `crystal_500` type. ASL nodes and waypoints are preserved as legacy XML but do not yet have dedicated editing tools. See `docs/ORIGINAL_FORMAT_0513.md`.
