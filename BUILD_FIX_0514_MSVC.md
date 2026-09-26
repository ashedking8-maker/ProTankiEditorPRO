# ProTanki Editor PRO 0.5.14 — Windows MSVC compile fix

Input: the uploaded `0.5.14-VERIFIED-WALL-COLLISION-SOURCE-CANDIDATE.zip` and GitHub Actions log for commit `5fdd667` (2026-09-25). This is a build repair within **0.5.14**, not a new feature release.

## Changes

- `src/MapDocument.cpp`: rename the local vector proximity predicate `near` to `nearVec3`. `<windows.h>` can define the legacy `near` macro, invalidating the lambda declaration and turning its uses into erroneous vector conditions. The numerical tolerances, shape comparisons, rotation checks, duplicate prevention and native collider creation remain unchanged.
- `tests/functional_map_regression.cpp`: rename the pasted bonus reference `b` to `pastedBonus`; `b` was already declared earlier in the same `main` scope as `DirectX::XMFLOAT3`. Preserve the complete bonus-property round-trip regression.
- `tests/test_windows_compile_0514_audit.py`: add two source guards for these build errors.

## Verify on GitHub Actions

Commit the full extracted source tree, preserving the `.github` directory and deleting files removed from the archive if necessary. Run `Build ProTanki Editor PRO for Windows` on `main`. A successful run must complete `cmake --build build --config Release --parallel`, all `ctest` cases, and packaging, creating both `ProTankiEditorPRO-0.5.14-Setup.exe` and `ProTankiEditorPRO-0.5.14-Portable.zip`.

Local Python preflight and source audits do **not** constitute a Windows/MSVC compile, a Windows Defender assurance, or validation of wall collision in ProTLVK. Test exported maps only on copies and verify collisions in the original game.
