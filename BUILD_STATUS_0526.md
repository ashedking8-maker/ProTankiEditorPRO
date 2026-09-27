# 0.5.26 source candidate verification

Based on the complete user-uploaded 0.5.25 GitHub repository. This is **source**, not a rebuilt Windows installer.

Local checks on the candidate:
- `python tools/check_source_consistency.py`: PASS.
- `python -m unittest discover -s tests -p "test_*audit.py"`: all 54 PASS.
- Standalone C++ `NativeVisualMetadataRegression`: PASS (0x4150, raw AFFF, source face/material mapping and editable native material overrides).
- Standalone C++ `ObjectDraftRegression`: PASS (draft v5 save/load and v4/v3 backward compatibility).
- Original user-provided LandHills/land01.3ds analyzed without repackaging it: 71 visual vertices, 120 faces, all smoothing masks mapped, original 228-byte material preserved across a separate read/write test.
- `node report_gateway/test_gateway.mjs`: PASS.

**Still unverified:** native Windows/MSVC editor build, full Windows CTest suite, and ProTLVK rendering/gameplay of a newly exported 0.5.26 TARA. The user's original Beach was visibly faceted even without the edited TARA; do not claim smoothing preservation alone fixes all game shading. Do not publish as a game-verified final installer until those tests pass.
