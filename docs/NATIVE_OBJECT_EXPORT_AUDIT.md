# Native object export — evidence and unknowns

The source tree's AssetRegistry.cpp currently reads a legacy library as:
`<library name="...">`, nested `<prop-group name="...">`, `<prop name="...">`, `<mesh file="...">`, and optional `<texture name="..." diffuse-map="...">`.
The legacy 3DS visual import currently excludes helper-like mesh nodes named Box*, Plane*, Tri*, Occl* from visual rendering. These are evidence of extra geometry in the source asset, NOT a verified specification for collision ownership or a sufficient recipe for a game-ready object.

Before enabling native export:
1. Obtain a real original editor library folder and a minimal authored object with known collision; include related source material/texture files and a map placing exactly one such object.
2. Compare 3DS node names, local transforms, winding, units, material names, texture paths, and helper polygon formats with native object examples.
3. Compare map XML before/after placement and deletion using the original editor, including all collision plane/box/triangle elements and map prop identity. Verify any offset collision helpers.
4. Implement deterministic 3DS + library.xml creation to a NEW user-selected library, not a guessed serializer modifying original files.
5. Add integration tests for re-import, placement, deletion, saving/reloading XML, and in-game physical behavior in ProTLVK.

Until verified, an object-draft.txt is a self-contained authoring document and `native_export false` is mandatory. The source 3DS/GLB files remain included only for further editing; source 3DS external textures may need to be supplied separately. No auto-placement into game maps is allowed.
