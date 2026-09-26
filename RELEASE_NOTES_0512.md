# ProTanki Editor PRO 0.5.12 — UX, grid and lighting preview

This is a **Windows x64 source candidate**, not a verified installer. It requires an
MSVC/GitHub Actions build and CTest before distribution. Preserve original maps
and libraries; test modifications on copies and verify game behavior in ProTLVK.

## Editing reliability

- The numeric toolbar **Step** now controls movement unconditionally. Mouse
  dragging, gameplay translations and keyboard movement use the selected grid;
  keypad 1..5 selects steps of 100..500. Shift + keyboard movement uses one
  tenth of the chosen step. The old Snap toggle no longer silently disables it.
- The Object Editor owns Ctrl+Z/Ctrl+Y/Ctrl+Shift+Z and Ctrl+S while open. Its
  independent, bounded in-memory history captures mesh edits and box edits;
  these shortcuts no longer undo or save the map behind the active workspace.
- Saving an isolated Object Editor draft requires explicit confirmation. Existing
  drafts and the original game library remain untouched.

## Workspace

- Toolbar: no Snap or Frame button; selected movement step remains visible.
  Grid, bounds, geometry view and map testing remain available. The Scene/Gameplay
  default dock locations are exchanged. Version-specific ImGui layout settings
  migrate the default arrangement without resetting it on each launch.
- Gameplay has a vertical quick-add list and a larger initially opened creation
  palette. Map version chatter and repeated instructional labels are removed.
- Custom theme supports separately selectable panel and text colors; a separate
  persisted viewport background color removes the fixed black void. It is not a
  real skybox.
- The main window title contains only the app name and 0.5.12. Opening Object
  Editor shows the existing splash art for approximately one second without
  sleeping/blocking the UI. Its help button links to Meshy for GLB generation.

## Editor-only lighting

- The mesh pixel shader receives up to eight local native omni lights. It shows
  approximate color/intensity/falloff on nearby rendered surfaces, including
  native map lights after move/add/delete in the current frame. Use the Lighting
  panel to turn the preview on/off and change an **editor-only reach multiplier**;
  this value never goes into the game XML. Native illumination, sprite pulse,
  shadows, occlusion, color mapping and skybox are NOT faithfully reproduced.
  ProTLVK remains authoritative for final appearance.

## Unfinished native conversion

- The Object Editor can import, modify, save and reopen its **isolated draft**.
  GLB-to-game 3DS + correct `library.xml`, texture references, collision helpers
  and game-compatible import are NOT implemented or promised. The original format
  has no established generic link between a visible triangle and native collision.
  The app deliberately cannot present a draft as a tested native asset.

## Verification

`python tools/check_source_consistency.py`

`python -m unittest discover -s tests -p "test_*audit.py" -v`

Windows GitHub Actions must run CMake + all CTest cases, including the new
`GridStepAlwaysOn`. After the installer exists, manually test mouse/grid drag,
Ctrl+Z isolation, custom theme/reach persistence, all native light types, and
Save As/reopen of map XML on a test copy.
