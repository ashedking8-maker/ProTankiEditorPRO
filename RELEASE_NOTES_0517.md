# ProTanki Editor PRO 0.5.17 — Lossless Object Snapshots (SOURCE CANDIDATE)

This version extends 0.5.16 without replacing the existing map serializer,
verified 3DS collision importer, 3D renderer, object-draft workflow or controls.
It is not yet a validated Windows build or a claim of complete original GTanks
Editor/ProTLVK game equivalence.

## Full native source metadata, separated by ownership

- `AssetRegistry` retains the **complete original bytes** of each `library.xml`
  in read-only shared memory, plus a complete serialized native `<prop>`
  definition per indexed asset. A successful rescan swaps the new index as a
  unit; failed rescans leave old definitions and the selected root intact.
  Original libraries are never rewritten.
- `MapDocument` retains the source map XML and complete per-instance `<prop>`
  subtrees. Shared read-only pointers keep Undo/Redo copies inexpensive.
  Unmodified maps follow the existing byte-identical no-op export path.
- A new map instance references its library/group/prop identity. **Library
  definitions are not blindly pasted into map XML**; the original game expects
  its object definitions in the library and instance data in the map.
- Duplication of an original instance uses `append_copy` on its full XML
  subtree, including unknown attributes, nested children and texture variant.
  It updates known instance transform fields, and refreshes each snapshot after
  a successful map save. Source identity is checked before cloning.

## Fail-closed compatibility / explicit advanced choice

- Unknown per-instance metadata is preserved when editing the original prop.
  Its duplication is **blocked by default**. An advanced checkbox authorizes
  copying the complete opaque XML subtree for the NEXT placement only; approval
  is automatically reset and is never stored as a native gameplay property.
  This option is visible in Library and selected-object Properties.
- The editor cannot infer whether unknown fields represent unique IDs, linked
  objects or transform-dependent values. Advanced copying is preservation of
  bytes/structure, **not proof that the copied metadata behaves correctly in
  ProTLVK**. Only use on a disposable map and validate in the game.
- Malformed or repeated known native fields (e.g. invalid `with_collision`,
  invalid `free`, repeated position/rotation fields) cannot be duplicated even
  with advanced approval. Original source data is left intact.
- Copying `<with_collision>1</with_collision>` still requires newly authored,
  owned native collision primitives; advanced XML approval cannot override it.
  Unknown 3DS collision helpers remain explicitly unsupported, not converted
  into approximated obstacles.

## Object Editor

- Selecting a model from the native library also attaches the entire source
  library/prop definition as a **read-only template**. The separate "Attach
  native library reference" list attaches a template without changing the
  model, so a custom GLB can reference an existing native definition. Template
  data can be cleared deliberately.
- The isolated draft now optionally stores exact original `library.xml` bytes,
  one complete native prop definition, and its library/group/prop identity in
  three sidecar files (`library-source.xml`, `library-prop-template.xml`,
  `library-template-origin.txt`). Saving and reloading retain those values;
  missing/incomplete sidecars fail without altering the active draft. Existing
  draft versions 2–4 still load. The original game library is never overwritten.
- This is **a reference template, not automatic inheritance of unverified
  gameplay semantics**. `native_export false` stays mandatory: custom GLB and
  custom box/mesh edits do not yet become a game-loadable 3DS + library.xml
  object. Ground dirt/dust material mapping is not invented.

## Verification and remaining limitations

- Source preflight: pass. Python source/audit tests: 32 pass. Portable C++
  ObjectDraft regression (including full template-sidecar roundtrip and damaged
  template handling): compile and run pass on Linux.
- Windows `TransactionalLibraryReload` and
  `OriginalMetadataAndClipboardRoundTrip` C++ regressions were expanded to
  check exact library bytes, full instance subtree duplication, save/reload,
  per-copy approval reset, malformed-native guard and collision guard. They
  are REGISTERED but NOT EXECUTED in this environment; run them on GitHub Actions.
- Remaining: game-semantic classification of opaque properties; unknown fields
  that depend on an object's location/identity cannot be automatically
  recalculated. Full 3DS box-helper support, custom native GLB/3DS output and
  precise original game surface effects are not established.

Always use **Save As** on a copy of a map, keep original game libraries read-only,
run Windows CTest, and verify tank collisions/other effects in the original game.
