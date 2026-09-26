# 0.5.15 — Native 3DS helper collision & texture diagnostics (source candidate)

## Native collision authoring

* A bounded binary 3DS parser reads explicit `plane*` and `tri*` helper objects,
  separately from the visual mesh. It recognizes the original GTanks Editor
  Wall End 1 (6 planes / 10 triangles), Broken Walls Hs_part06 (6 triangles),
  and Industrial Bridge Bridge 1 (2 planes / 2 triangles). It uses each source
  model's visual anchor/pivot and the prop's position and Z rotation.
* Each newly placed supported prop receives its native planes/triangles in the
  same map edit/history operation. The Add-at-view-focus and group placement
  paths both use this method. New colliders follow moves, rotation, copy,
  save/load and deletion. Only complete, unambiguous native XML sets are
  rebound after loading; comparisons use transformed world geometry, because
  the original editor can serialize equivalent Euler/vertex conventions.
* The existing independently verified Beach/sidewalls/Wall End 2 template
  remains intact. No visual-triangle or blanket-bounding-box guesswork is
  introduced. Unverified nonidentity mesh pivots, box helpers, missing assets,
  damaged files and overlapping duplicates fail closed. A user must **explicitly**
  opt in to place a VISUAL-ONLY item with no tank collision.
* Geometry view blocks individual deletion of a collider bound to a static prop;
  delete the owning prop to remove its full set. Existing unbound/ambiguous
  legacy collider edits remain separate and conservative.

## Object Editor integration

The same 3DS parser is used in the Object Editor as a read-only inspector,
reporting original plane/triangle helpers. The already editable custom
solid/trigger boxes remain part of isolated GLB/3DS drafts. This release
**does not** convert a GLB draft to a native ProTLVK-compatible 3DS/library.xml
object; it does not silently modify the user's original game library.

## Bridge 1 and other alpha-textured meshes

* The mesh shader now lights linearized sRGB texels, then converts back to sRGB
  for the existing UNORM render target. Its opaque depth-writing path uses
  alpha cutout rather than painting almost fully transparent texels opaquely.
* New Effects → **Unlit texture (diagnostic)** shows source texture RGB without
  editor lighting/effects for comparing an imported model with its texture.
  Full sorted translucency, exact legacy ProTLVK lighting and normal equivalence
  remain separate tasks; no claim is made that Bridge 1 is now pixel-identical.

## Test inputs and release status

The regression fixtures include the three original 3DS files provided by the
user and a one-wall native XML reference extracted from the original editor's
map_polygon_TEST.xml. The user confirmed in ProTLVK that the original editor's
Wall End 1 blocks tank movement. The test suite checks original helper counts,
geometry/normals, native XML save/reload, owner binding, move/rotate, and
whole-prop deletion. See BUILD_STATUS_0515.md.

**Do not overwrite a personal map for the first test.** Use Save As and verify
new 3DS collisions in ProTLVK. Actual 0.5.15 Windows/MSVC build, CTest, NSIS
installer and live game testing remain pending until GitHub Actions/user test.
