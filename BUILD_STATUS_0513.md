# 0.5.13 candidate build status

- Source candidate, **not** an already compiled Windows EXE/installer.
- Local preflight, Python audit, independent portable C++ tests and ZIP validation are recorded in the response only after actually run.
- Full MSVC build / CTest / NSIS / ProTLVK runtime not available in this Linux container. GitHub Actions is required to validate this candidate.
- Native editable ObjectDraft storage exists; native game-compatible modified GLB/3DS export remains disabled until helper geometry, material conventions and client validation are complete.
