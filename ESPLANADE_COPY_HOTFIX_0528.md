# ProTanki Editor PRO 0.5.28 — Esplanade copy candidate

This source patch BUILDS ON the 0.5.28 Fogtown Ground Collision Safety Hotfix. It does not remove or replace the Fogtown fix. The Windows build and the full 1879-object Ctrl+C / Ctrl+V operation remain to be validated in GitHub Actions / the user's editor.

## Why the old build rolled back

The original `map_esplanade.xml` stores three pairs of exact coincident `Outer Walls/default/WTile 1` props. Each pair is accompanied by two physically identical native `collision-plane` records. The editor's blanket duplicate guard rejected the second valid copy, discarding the entire paste. Earlier attempts also encountered `Industrial Elements/default/Container`, whose visual 3DS mesh is named `Box04` (and was mistaken for a collision helper).

## Source changes

- `src/MapDocument.cpp/.h`: permit an otherwise forbidden coincident WTile clone only when the copied source proves the exact multiplicity of native colliders and the duplicate is from the current paste transaction; perform one-to-one original source binding for verified matching planes. Unverified duplicate objects remain blocked.
- `src/EditorUi.cpp/.h`: confirm source WTile cardinality and exact world plane geometry before enabling the narrow exception. Failed transactions log candidate number, original XML index, asset identity, and target position.
- `src/NativeCollisionImport.h`: topology-checked visual anchors `Box04` for `contain.3ds` and `Box01` for `tunnel_1.3ds`; other Box-prefixed models are not reclassified automatically.
- `src/Logger.cpp`: unique build log identifier `0.5.28-esplanade-copy-guard`.
- `tests/esplanade_copy_regression.cpp` and `tests/fixtures/esplanade/`: original source excerpts and 3DS fixtures covering duplicate wall binding, Container and Tunnel 3DS parsing, save/reload and owned collision updates. CTest configured in CMake.

## Installation and test

Apply the SMALL ZIP to the ROOT of the same GitHub repository that already contains the Fogtown safety hotfix, preserving all folders, and commit the changes. Alternatively use the FULL SOURCE ZIP instead of the small patch (not both). Wait for GitHub Actions Windows compilation and CTest. Install the newly built editor and confirm the startup log says `0.5.28-esplanade-copy-guard`.

Make a backup of Esplanade. Open it, select the whole static map and Ctrl+C then Ctrl+V, position the copy away from the original, confirm placement, Save As under a NEW filename. Inspect the prop / collision counts and test in ProTLVK. Separately recheck original TEST_MAP_AAA.xml Fogtown Save As; do not regress its floor physics. If the paste still rolls back, send the NEW session log: the failed item/source XML index and its coordinates are now recorded.

## Verification limits

Static source preflight and 77 Python unit tests passed in this environment. A Linux CPU probe compiled and read the original three affected 3DS assets; source XML was checked for three exact WTile pairs and two matching game planes for each. This is not a claim that the entire Windows editor or game has been built or tested here. The fix does not silently skip failed objects or suppress rollback.
