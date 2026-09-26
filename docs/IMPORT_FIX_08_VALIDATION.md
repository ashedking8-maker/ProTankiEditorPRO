# Import Fix 8 validation

Import Fix 7 correctly removed authoring placement offsets, but its visual importer
merged every mesh node from the 3DS file. Inspection of the supplied AXC corpus
shows that legacy props commonly store collision/occlusion helpers as additional
nodes named `Box*`, `Plane*`, `Tri*`, or `occl*`. Rendering those nodes explains
the solid grey/white overlays and z-fighting visible in the Polygon screenshots.

Detailed examples from the supplied AXC corpus are recorded in `IMPORT_FIX_08_ASSET_EVIDENCE.md`.

The visual importer now emits only the selected visual anchor node. Helper nodes
remain excluded from render geometry; the already-open map continues to use its
legacy collision section from XML. Future prop-placement work can parse helper
nodes separately for collision generation.

Regression fixtures now verify both local bounds and exact visible triangle counts:

| Model | Visible triangles | Ignored helper mesh nodes | Expected bounds |
|---|---:|---:|---|
| tile_01 | 2 | 1 | (-250,0,-250) .. (250,0,250) |
| brid_7 | 8 | 6 | (-250,250,-250) .. (250,600,250) |
| chest02 | 43 | 4 | (-250,0,-140) .. (140,260,250) |

Windows/D3D11 screenshot parity must still be validated by the GitHub build on the
user's machine. A strong runtime signal is a large drop from Import Fix 7's 342
draw calls while scene bounds remain approximately in the same sane range.
