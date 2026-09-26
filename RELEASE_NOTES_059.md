# ProTanki Editor PRO 0.5.9 — source candidate

Changes include projected face picking for visible bonus and special zones,
stronger drop outlines, drag/resize and Undo, selectable bonus types/modes and
native free/parachute attributes, X/Shift+X spawn direction and direction arrow,
independently remembered Browse Library thumbnail size, darker silver theme,
clipboard assets added to AX recents, and a visual mesh vertex/triangle editing
workspace with coordinate-correct overlay, scaling and isolated v3 draft save/load.
The previous red collision box is not rendered unless explicitly enabled.

## Not yet certified

- Must pass Windows x64 MSVC compilation and all CTest tests in GitHub Actions.
- Native GLB / edited mesh 3DS + library.xml material/collision export remains
  unavailable. An authoring draft cannot be placed directly as a game-native prop.
- Validate each edited map in the original ProTLVK32.exe after saving a copy.
- Native collision helpers in legacy 3DS files are not inferred from visual triangles.
- Multi-material editing in source previews needs visual checking after topology edits.

## Build

Upload this complete source tree to the root of the existing GitHub repository,
including `.github/workflows`. On a passing workflow, retrieve the 0.5.9 Setup EXE
and Portable ZIP from its artifact/release. If MSVC or CTest reports an error,
use the actual compiler/test log; passing Python audits is not equivalent to a
Windows build or runtime test.
