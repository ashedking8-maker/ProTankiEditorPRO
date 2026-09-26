# 0.5.16 — Native map metadata fidelity / isolated object intent (source candidate)

This source checkpoint builds on 0.5.15's 3DS plane/triangle import and Bridge 1
rendering diagnostics. It is **not** a claim of full GTanks Editor parity or a
verified Windows executable. No original game library or personal map was modified.

## Independent native map audit

A read-only scan of 201 original ProTLVK map XML files found 289,730 static props:
281,883 omitted `<with_collision>`, 6,754 set it to `1`, and 1,093 set it to `0`.
The native `free` prop attribute occurs on 2,935 instances. We found no explicit
per-prop `surface="dirt"` / `surface="asphalt"` field in this map sample.
These counts establish **syntax**, not the game-engine semantics of dust, dirt,
friction or particle effects. Read `docs/ORIGINAL_MAP_FEATURE_AUDIT_0516.json`
for the source-archive inventory and `tools/audit_native_map_features.py` to run
an equivalent read-only scan against another map archive.

## Map editing and export safeguards

- Existing native `<prop>` nodes are edited **in place**, not removed and
  appended after unknown siblings. Unknown source attributes/child XML and
  sibling ordering stay intact for retained props; only deliberately changed
  transforms are rewritten. Unknown other top-level sections remain in the
  original master XML.
- The per-instance `<with_collision>` flag is now parsed as an **optional**
  three-state value (absent, 0, 1), not inferred from helper counts. Native
  `free` is also optional. Both values are shown read-only in Properties.
- Ctrl+C/V of an original prop keeps its library/group/name, exact texture
  variant, observed `free` and `<with_collision>` values. New catalog objects
  do **not** receive invented native flags; absent stays absent.
- When duplicating an observed `<with_collision>0</with_collision>` instance,
  the editor does not infer new colliders and requires explicit visual-only
  opt-in; the engine semantics of `0` are NOT asserted. An observed `1` cannot
  be copied as an unverified visual-only object. Unsupported
  or malformed source metadata on a copied prop blocks export rather than
  silently dropping fields. Existing original nodes remain editable/savable.
- The existing native 3DS helper importer still requires verified helper
  geometry; unsupported box helpers and unknown transform conventions fail
  closed. Do not interpret `with_collision` by itself as a tank collision test.
- A new Windows CTest `OriginalMetadataAndClipboardRoundTrip` checks original
  source metadata, unknown extensions, sibling order, duplicate flag and
  texture preservation, repeated saves, and fail-closed unknown copies.

## Object Editor

- Draft-only purpose: **Decoration/passable**, **Solid/obstacle**,
  **Driveable surface**, or **Trigger/region**. Decorations may have zero
  annotation boxes; other intents require at least one box. Switching modes
  updates authoring annotations with Undo support.
- The isolated draft file format is now v4 and records `purpose`. Existing
  v2/v3 drafts can still be opened. Saved drafts continue to explicitly say
  `native_export false`; they do not change `library.xml`, source 3DS, native
  collision primitives or original game material effects.

## Unverified / intentionally not implemented

Full 3DS box-helper support, arbitrary transforms, native GLB game export,
editable real-game material/ground-effect IDs and pixel-equivalent ProTLVK
lighting remain unverified. The game's mapping from textures/materials to
soil-vs-pavement effects is not demonstrated by this map/XML inventory; no
invented texture-name-to-particle rule is exported. Existing original texture
identities and untouched metadata are preserved. Always use Save As on a map
copy and check both geometry and surface effects in original ProTLVK.

## Build status

Local Linux checks: Python source preflight and 27 Python audit tests passed;
portable `ObjectDraftRegression` compiled and executed with g++ (including v3
backwards compatibility and v4 decorative/driveable round-trip). The new
Windows-native XML regression is **registered but not yet run**. Full Windows
MSVC build, CTest, NSIS packaging and live game testing require GitHub Actions
and the user's own ProTLVK check. See `BUILD_STATUS_0516.md`.
