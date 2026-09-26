# 0.5.23 source candidate verification

- `python -m unittest discover -s tests -p 'test_*.py'`: 54 tests PASS.
- `python tools/check_source_consistency.py`: PASS.
- Legacy map display basis changed from mirrored `(x,z,-y)` to left-handed `(x,z,y)`.
- 3DS visual import basis changed to the same `(x,z,y)` convention.
- Spawn heading and Edge Snap horizontal Y conversion updated to match the new basis.
- 0.5.22 camera-only 180° workaround removed; reference yaw restored to `-0.75`.
- XML serialization code was not changed by this patch.
- Windows MSVC compile, full CTest, installer and runtime comparison against original GTanks Editor / ProTLVK: NOT run here.
