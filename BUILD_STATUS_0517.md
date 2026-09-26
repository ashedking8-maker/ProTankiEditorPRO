# ProTanki Editor PRO 0.5.17 — validation status

| Stage | Result |
|---|---|
| Python source consistency preflight | PASS |
| Python source / audit tests | 32/32 PASS |
| Portable C++ `ObjectDraftRegression` (`-std=c++20 -Wall -Wextra -Werror`) | Compiled and PASS |
| C++ `TransactionalLibraryReload` Windows CTest | REGISTERED, NOT RUN HERE |
| C++ `OriginalMetadataAndClipboardRoundTrip` Windows CTest | REGISTERED, NOT RUN HERE |
| Full Windows MSVC build and D3D11 runtime | NOT VERIFIED |
| GitHub Actions NSIS/portable packaging | NOT VERIFIED |
| ProTLVK game compatibility | NOT VERIFIED |

No original library or user map was edited. Keep 0.5.16 and 0.5.14 source ZIPs
as rollback checkpoints. This is a candidate source patch, not a verified binary.

## Manual validation after successful Windows build

1. On a copy of `map_polygon.xml`, open and Save As with no edits, then compare
   files (the no-op path must be byte-identical).
2. Copy a normal prop with a specific texture; verify its texture, transform,
   collision primitives and map `with_collision`/`free` values after reloading.
3. Check a prop with extra XML extensions: regular copy must be refused.
   Enable **opaque XML copy for NEXT placement** and verify every unknown field
   in the copied XML; confirm approval automatically returns to OFF.
4. Ensure a malformed native flag stays blocked, and `<with_collision>1` without
   newly owned collision cannot be exported by using advanced copy.
5. In Object Editor choose an original library template, switch to an external
   GLB, save a separate draft and reload it. Check the three source-template
   sidecars and `native_export false`. Do not place draft sidecars into the
   original game library or claim they are functional ProTLVK objects.
6. Run `Hs_part06`/Wall End and Bridge 1 visual/collision regressions in
   original ProTLVK on copies. Ground material particle effects remain a
   separate engine compatibility test.
