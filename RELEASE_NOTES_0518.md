# 0.5.18 — Compact UI and 3DS opposite-face atlas fix (source candidate)

Base: **original 0.5.17 source candidate**, including the prior five-file 0.5.17 Release CTest repair. This archive contains only changed/new files, with no unchanged assets or full source.

- Library top-row **Settings...** popup contains the previously inline visual-only and one-shot opaque-copy opt-ins. Safety defaults and authoring guards remain unchanged.
- Asset preview ends after its interactive image. Asset selection still starts cursor-following placement; map keyboard shortcuts remain. No Placement Z / Follow cursor / Add at view focus row. No redundant placement controls are shown under preview or in Library settings; existing map placement inputs are retained.
- Short factual status in Properties with contextual hover tooltips; sprite decorations are not mislabeled as unsupported 3DS solids. Original XML attributes remain intact. Original embedded 3DS diffuse filename replaces <default> when available.
- Explanatory prose in editing/lighting panels moved into short-delay hover tooltips. Important error/validation messages remain visible.
- Save object changes modal uses content-based height; lighting action is Add Light.
- 3DS visual meshes with co-planar opposite-winding UV-atlas face pairs receive a selective D3D11 backface-culling rasterizer state. All other assets retain two-sided rendering. No change to source 3DS, diffuse textures, native collision geometry or map XML. The user-reported brid_1.3ds 12 triangles form six such face pairs. Exact game screenshot parity requires Windows/ProTLVK comparison.

Source preflight/Python audits runnable without Windows. Actual Windows CTest, rendering and installer require GitHub Actions and user game verification.
