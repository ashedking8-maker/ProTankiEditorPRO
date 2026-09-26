# Import Fix 7 validation

Executed against the production CPU importer, compiled with GCC C++20 and
Assimp 5.4.3 (3DS importer enabled, static build):

| Model | Internal minimum | Internal maximum | Result |
|---|---|---|---|
| tile_01 | (-250, 0, -250) | (250, 0, 250) | PASS |
| brid_7 | (-250, 250, -250) | (250, 600, 250) | PASS |
| chest02 | (-250, -0.00244, -140) | (140, 260, 250) | PASS |

Tests check bounds against independent known fixture values, all part index
ranges, vertex references, material texture existence (including upper/lower
case resolution), and failure for missing input. Chest02 verifies a multi-node
assembly without separately centering its five parts. Fixtures are samples from
the supplied AXC archive; only six small model/texture files are included.

A separate corpus check loaded all 34 unique mesh paths referenced by the supplied
976-prop map_polygon XML. All imported successfully. Five use the documented
first-mesh-node anchor fallback: st_br02, contain, nubu_3, tunnel_1, tunnel_2.
Two embedded texture references are absent (tower and bilboard), but the map's
library.xml variants supply the textures used for these instances.

Windows compilation, D3D11 rendering, screenshot parity, and interactive
move/undo are pending. The Windows workflows run the CPU regression test before
packaging. This report does not claim a completed Windows build.

All 897 XML texture assignments in map_polygon resolved to existing files.
