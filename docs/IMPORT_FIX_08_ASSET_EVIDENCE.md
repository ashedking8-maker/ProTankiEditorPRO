# Import Fix 8 — AXC asset evidence

The supplied AXC library was inspected directly before this patch. Legacy 3DS
files frequently contain one textured visible prop object plus extra helper mesh
objects used by the old editor/game tooling.

Representative examples:

| 3DS file | Visible object | Visible faces | Extra helper objects |
|---|---|---:|---|
| `LandTiles/tile_01.3ds` | `tile_01` | 2 | `Plane02` |
| `IndustrialBridge/brid_7.3ds` | `brid_7` | 8 | 6 `tri*/plane*` helpers |
| `Stuffs/chest02.3ds` | `Chest02` | 43 | `Box141`..`Box144` |
| `DesertHouses/smhouse2.3ds` | `SmHouse2` | 110 | `Box121` |
| `CityBuildings/smhouse5.3ds` | `SmHouse5` | 38 | `Box109`, `Box110`, `Box122`, `Box123` |
| `NuBu9/nubu_9.3ds` | `nubu_9` | 38 | `Box40`, `occl` |
| `NuBu3/nubu_3.3ds` | `nubu_03` | 55 | `Box07`, `Box08`, `occl`, `occl0` |
| `OuterWalls/ow_t.3ds` | `ow_t` | 44 | `Box41` |
| `Promotion/bilboard.3ds` | `bilboard` | 58 | `Box113`, `Box114` |

Some legitimate visible objects have helper-looking names because old asset
naming was inconsistent. Examples are `tunnel_1.3ds -> Box01`,
`tunnel_2.3ds -> Box02`, and `IndustrialElements/contain.3ds -> Box04`.
Therefore Fix 8 does not simply blacklist all `Box*` names. Filename-stem match
is preferred; otherwise the fallback scores material texture/non-default material
and face count, with helper-like naming used only as a penalty.

Import Fix 7 rendered the whole 3DS mesh-node assembly. Polygon consequently
reported 342 draw calls. Fix 8 renders only meshes attached to the selected visual
anchor while preserving multiple materials on that visible node.
