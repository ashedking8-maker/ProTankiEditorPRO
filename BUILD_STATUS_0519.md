# 0.5.19 verification status — source candidate

- Linux source preflight: PASS.
- Python audit suite: 38 tests PASS, including independent original NuBu 3 two-box XML geometry comparison.
- Portable ObjectDraftRegression, C++20, -DNDEBUG, -Wall -Wextra -Werror: PASS.
- Standalone 3DS importer compiled with C++20 and strict warnings using local temporary DirectXMath stub; original NuBu 3: 2 boxes; Wall End 1: 6 planes + 10 triangles; Hs_part06: 6 triangles; Bridge 1: 2 planes + 2 triangles.
- Windows complete build/CTest: NOT RUN here (project is Windows/D3D11 only). The native multi-model CTest is registered, not executed in this environment.
- Windows installer + original game screenshot/pixel parity + game tank collision: NOT VERIFIED.

The new native box behavior has a real original-editor reference fixture. Unknown attributes and nonidentity helper transforms are not declared supported unless validated. The Object Editor is still a draft authoring workspace and does not produce game-ready 3DS assets.
