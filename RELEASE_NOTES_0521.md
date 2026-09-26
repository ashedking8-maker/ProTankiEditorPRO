# ProTanki Editor PRO 0.5.21 — Library and placement fidelity (source candidate)

- AX recent-asset wheel switching requires holding TAB. Ordinary wheel input over Library and viewport is no longer intercepted by placement/history selection.
- Browse Library flattens internal `default` group visually (XML identity unchanged), and displays one card per texture variant, each selectable for actual placement. GPU thumbnail rendering remains limited to one newly visible card per frame with a bounded cache.
- Optional **Tools → Absolute grid snap** (on by default) quantizes final world position on keyboard moves. Shift uses one-tenth grid step; maps are never bulk-rounded.
- Scene collision list displays ownership instead of offering a nonfunctional Remove button for object-linked native helper primitives. Map Undo logs actual collision totals.
- Original Fabr Tower and Broken Fabr Tower scaled plane-helper 3DS matrices are supported: they are reconstructed from existing world-space rectangular vertices, while non-rigid box helper and visual pivot matrices remain fail-closed. Original 3DS fixtures added to the C++ regression suite.

## Limits and test obligations

This patch does NOT claim all original objects are supported. Geometry-aware Edge Snap and optional surface clearance are not implemented in this version. The original Bridge rise at 0 yaw had matching collider geometry in the supplied paired maps, so its export orientation is intentionally unchanged. The different wall yaw values in those maps reflect different saved rotations, not evidence for a global inversion. Windows GitHub build and ProTLVK gameplay validation remain required.
