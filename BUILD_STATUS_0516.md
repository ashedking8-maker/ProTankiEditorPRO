# ProTanki Editor PRO 0.5.16 — validation status

- **Source candidate, not a compiled Windows release.** The 0.5.15 Windows
  compile/runtime status was not independently confirmed here.
- `python tools/check_source_consistency.py`: PASS.
- `python -m unittest discover -s tests -p "test_*audit.py" -v`: 27 tests PASS.
- `g++ -std=c++20 -Wall -Wextra -Werror -Isrc tests/object_draft_regression.cpp`: compiled; executable PASS.
- `tools/audit_native_map_features.py` scanned 201 supplied original-game maps
  without modifying them. Results stored in docs JSON.
- `OriginalMetadataAndClipboardRoundTrip` (C++/pugixml/Win32) is registered in
  CTest but is **PENDING** until a GitHub Windows runner builds and runs it.
- Windows D3D11 app compilation, CTest suite, packaging and game compatibility:
  **PENDING**. Static checks do not prove game parity.
- First test must use Save As on a copy of the map; inspect new `Hs_part06`
  colliders, test tank movement, verify `with_collision`/`free` and texture
  variants after reload, then compare surface particle effects in ProTLVK.
- Preserve 0.5.14 and 0.5.15 source ZIPs as rollback checkpoints. Do not turn
  off Defender to run untrusted installers.
