# Coordinate pipeline — Import Fix 8

XML map positions and rotations stay in legacy X/Y-horizontal, Z-up coordinates.
Assimp 5.4.3 imports each 3DS mesh in node-local space. We intentionally do not
request `aiProcess_PreTransformVertices`, because baking the 3DS scene graph back
into the vertices restores the artist's original placement offsets.

`LegacyMeshImport` enumerates mesh-bearing nodes and chooses the visible prop
anchor. A case-insensitive node name matching the 3DS filename stem wins. When
legacy naming differs (for example `tunnel_1.3ds` -> `Box01`), fallback selection
prefers a node carrying a diffuse texture/non-default material and then face count,
while penalizing typical helper names (`Box*`, `Plane*`, `Tri*`, `occl*`).

Only meshes attached to that selected visual anchor node are emitted to the render
model. Additional 3DS nodes are legacy collision/occlusion helpers and are not
rendered. Existing map collision continues to come from the collision section of
the map XML; future placement tooling may parse helper nodes separately.

The selected node-local visual vertices are converted from legacy Z-up into the
D3D render basis exactly once:

    renderLocal = basis * nodeLocal
    basis: (x, y, z) -> (x, z, -y)

Normals use inverse-transpose and mirrored transforms reverse triangle winding.
Map transforms are subsequently converted by `LegacyTransform::World`.

Material index ranges attached to the visual anchor are retained. XML texture
variants override the 3DS diffuse texture; otherwise the imported material texture
is used, resolving legacy filename case and stripping original artist-machine path
components. Untextured visual materials use their diffuse color. Missing texture
files are logged.

Bounds, picking and transform edits use the same corrected visual geometry as
rendering. Map XML serialization is unchanged by this patch.
