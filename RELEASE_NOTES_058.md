# ProTanki Editor PRO 0.5.8 — Windows source candidate

## Changes against 0.5.7
- Fix billboard/sprite geometry: the vertex shader samples image dimensions and now receives the actual sprite texture SRV, including placement ghosts. This specifically addresses trees that were counted by the scene but could appear invisible.
- G toggles native collision geometry; H toggles grid. Existing unmodified custom G-grid binding migrates to H.
- Persistent X-closeable Gameplay creation palette for existing native spawns, flags, control points, bonus/drop regions and special kill/kick volumes.
- Ctrl + upper corner dragging resizes selected kill/kick and bonus region horizontal bounds; normal dragging moves them. Existing min/max Z remains editable in Properties. Regenerated XML retains the source types and non-geometry sections.
- Bonus/drop region name, bonus-type, and game-mode editing, with correct XML round-trip.
- Wider, visible selected volume outlines and corner handles.
- Object Editor can show imported real visual mesh triangle edges, and fit an optional box to mesh bounds; a box is no longer the only visual geometry cue.
- Routine successful library selections/indexing no longer generate status toasts, but independent remembered paths and logged failures remain.
- Correct NSIS uninstaller command-line quoting.

## Not yet represented as finished
Native GLB to 3DS, material and collision helper export is NOT implemented or game-verified. Imported visual mesh wireframe is NOT the same as original native collision helper geometry. Valid bonus-type names beyond the verified fixtures require comparison with original game libraries and maps. Windows MSVC build and a ProTLVK gameplay smoke test must be re-run for 0.5.8. The current fixes do not retroactively validate older map changes. Test edits on copies.
