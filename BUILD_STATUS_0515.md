# ProTanki Editor PRO 0.5.15 build / compatibility status

This is a source release **candidate**, not an already-tested Windows executable.

* Portable C++ 3DS helper reader compiled with g++ -std=c++20 -Wall -Wextra -Werror
  against a minimal local DirectXMath stub. Original models were read and
  returned Wall End 1: 6 planes/10 triangles; Hs_part06: 0/6; Bridge 1: 2/2.
* Original Wall End 1 helper positions, triangles, oriented face normals and
  plane corners have been compared with the independent original editor XML.
* Python static checks and data audits run locally. The new Windows-native
  `Original3DSHelpersNativeXMLRoundTrip` CTest is registered, but must run on a
  Windows runner with MSVC/pugixml. Passing a Python source preflight is **not**
  equivalent to compiling the app.
* Fixed asset helpers only: `plane*` and `tri*`; `box*` helpers and nonidentity
  pivot transforms intentionally fail closed pending real-game validation.
* Object Editor GLB save remains draft-only; it does not promise working native
  game object export. Texture gamma/alpha changes need screenshot comparison.

For a Windows build, upload the **contents** of the complete source ZIP to
GitHub (not the ZIP as one blob); ensure old deleted files are removed and both
workflows are updated. Run source preflight, Python audits, CMake/MSVC build,
CTest, NSIS packaging; then test a new Hs_part06 placement in a COPY of a map,
including a save/reload and tank collision. Retain the original 0.5.14 zip as
rollback. Do not disable Windows Defender for testing.
