# ProTanki Editor PRO 0.5.14 — native collision phase (source candidate)

This package is NOT a precompiled or ProTLVK-validated EXE. It includes the full 0.5.13 feature set.

The supplied edited map_polygon had three newly placed Beach / sidewalls / Wall End 2 visual props, and zero new collision records. An independent original map_beachfort.xml supplied with ProTLVK provided a source example with six native collision planes and ten native collision triangles for this exact model. The template was position-matched against all ten source placements; nine had only the sixteen expected local items within the search radius, and one had four unrelated nearby triangles and one extra plane. Every one of the 160 expected primitive positions had exactly one match in the original map.

Only that exact asset identity currently receives native collision on new placement. For existing walls already saved without collision in 0.5.12, select each original Beach / sidewalls / Wall End 2 prop, choose Properties > Repair saved wall collision, and Save As a COPY. The repair does not run automatically on load and refuses a match to a partial/duplicate shape. Other static meshes retain prior visual-only placement and now show an explicit warning. The code does not invent general collision by converting every render triangle or bounding box. Source-index rewriting preserves original unknown XML nodes and only appends authored primitive records.

Explicit in-memory ownership covers the verified native shape during move, rotate and delete, and is reconstructed conservatively after map reload only when the complete exact template matches. All unsupported or ambiguous ownership remains unbound. Normal legacy maps are not bulk-rewritten.

Also fixed startup empty-map ghost placement (clean blank document), added native collision to 'Add at view focus' path, and repaired centred 540px save-draft modal and close-draft modal. Original draft folders remain drafts, NOT automatic game-library models. Automatic arbitrary GLB -> playable native 3DS export is still gated.

The separate original XML regression fixture contains all ten source placements and 160 native collision records. Local source/Python checks and targeted portable C++ tests are not Windows/MSVC builds; GitHub Actions and in-game ProTLVK collision checks remain required before declaring verified runtime support. Use backups of your original maps.
