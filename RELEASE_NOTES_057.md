# ProTanki Editor PRO 0.5.7 — Windows build candidate

This is the complete 0.5.7 **source** package for GitHub Actions, not a compiled or game-certified Windows binary. Builds must pass MSVC, CTest, packaging and manual ProTLVK verification.

## Implemented
- Short right click clears selected prop or gameplay volume; drag still orbits.
- Gameplay precedes Properties; Contact dialog removes extra heading; panel-only theme tint and lighter silver.
- Read-only Geometry View renders legacy plane, box and triangle colliders (diagnostic colors, NOT proven passability classification).
- Independent first-run explanations and last selected map/library/tester paths; welcome controls and persisted theme.
- Current-user Windows uninstall registration; restored GitHub first-release creation.
- Separate 3D Object Editor GLB/3DS model preview, multiple authoring collision boxes, pan/orbit/zoom, tint preview and isolated saved drafts.
- Edit > Select all static props (Ctrl+A) selects the complete static map, independent of viewport picking. The first overwrite creates a non-overwritten `.original.bak` next to the map.
- Deletion permits a co-located visual prop to be removed while retaining collision for a surviving prop. Exact-origin matching colliders may be removed when no co-located visual prop survives. Deleting *all* static props clears static colliders including triangles; individual triangles can be reviewed/deleted in the Collision inspector. Original XML collision nodes/unknown siblings retain their relative order. Undo supported.
- Read-only audit tools: tools/audit_native_library.py and tools/audit_map_delta.py.

## Known limits / safety
A GLB draft is NOT a native game asset and is not installable into the original library. No verified GLB-to-3DS plus library.xml/material/collision conversion is available, so the exporter remains intentionally disabled. Embedded GLB textures are not rendered in the draft preview. No automatic ownership inference for arbitrary collision triangles/offset physics; for partial deletes inspect Geometry in ProTLVK on **Save As** copies. No claim of Windows runtime or original game compatibility until the user-run build and game test. Do not overwrite original maps or the library. The original ProTLVK32.exe is only launched, never modified. No code signing / Defender false-positive certification.

The preflight verifies source structure, not C++ compilation. GitHub Actions uses Windows/MSVC for that. See docs/RELEASE_057_VERIFICATION.md.
