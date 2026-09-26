# 0.5.7 verification gate

1. Upload all files, including hidden `.github/workflows`, to the repository root (do not nest the extracted folder). Push to main.
2. Run GitHub Actions > Build ProTanki Editor PRO for Windows. Inspect preflight, 5 read-only audit tests, MSVC editor compilation, every CTest, setup/portable packaging and artifact upload. A red workflow after an artifact upload can indicate Release publication failure; inspect the exact failed step.
3. Download the build artifact, unpack it, run Portable on Windows 10/11; verify GLB/3DS preview and Geometry View, user choices and panel theme.
4. Make a separate backup of the source map. The first in-place save also creates a non-overwritten `.original.bak`. Use Save As to a different XML path, reopen and compare count and nodes. Test deleting an exact-origin prop, a decorative prop, a shared-origin prop and bulk deletion. Confirm collision behavior in the unchanged ProTLVK.
5. Verify installer register/unregister under Windows Settings > Apps and that originals stay untouched. Record Defender detection name and SHA256 if a warning occurs.
6. For game-native export, obtain original-editor examples of before/after map changes and a genuine native library object, then validate material, texture, helper-node and collision semantics. Until then, GLB Object Editor output stays a draft.

Automated checks cannot establish point 3–6 from a Linux environment.
