# Native object export — evidence and validation gate (0.5.7 phase 5B)

**Status: audit only. No game-compatible GLB→3DS exporter has been implemented.**
The Object Editor's solid/trigger cuboids and color tint are draft annotations; they must not be serialized into a guessed native library or map format.

## What can be established from bundled real 3DS fixture bytes

`tests/fixtures/LandTiles/tile_01.3ds` has two named mesh objects: `tile_01` and `Plane02`.
`tests/fixtures/Stuffs/chest02.3ds` has five named mesh objects: `Chest02`, `Box141`–`Box144`.
`tests/fixtures/IndustrialBridge/brid_7.3ds` has seven named mesh objects, including `plane*` and `tri*` names. Each has keyframe/object-node chunks. These are structural observations only: object names alone do not prove how the game classifies or transforms a collider, or how the original map editor emits its collision XML.

## Read-only inspection

Run `python tools/audit_native_library.py PATH_TO_ORIGINAL_LIBRARY --output PATH_OUTSIDE_LIBRARY/report.json`.
The report inventories library/group/prop names, mesh paths, declared textures, missing files, named 3DS objects and chunk identifiers. It does not mutate the original library. Missing/invalid entries are reported rather than silently replaced. Do not upload private whole libraries merely for a report if a minimal paired sample can demonstrate the format.

## Required proof before enabling native export

1. Obtain a legally shareable sample of `library.xml`, the referenced 3DS and textures, plus an original-editor-created map containing that prop.
2. Obtain a second original-editor export of the same map with exactly one prop deleted and a third with it translated/rotated; compare visual, plane, box and triangle XML sections.
3. Determine mesh node naming conventions, transforms, local axes, material/texture naming, native units and collision primitive serialization from actual bytes and before/after XML.
4. Implement 3DS writing with deterministic round-trip inspection, valid `library.xml`, and importer parity; reject any unsupported source or flag instead of guessing.
5. Import in our editor, save/reload, and verify gameplay in ProTLVK on a copy. Only then expose a `Game-compatible export` action.

The independent draft-output folder and originals remain untouched throughout this process.
