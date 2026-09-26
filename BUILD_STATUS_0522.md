# 0.5.22 source candidate verification

- `tools/check_source_consistency.py`: PASS.
- `python -m unittest discover -s tests -p 'test_*.py'`: 50 tests PASS.
- Original 3DS sweep: 1,432 files; 1,068 with supported native collision helpers, 319 with no native helpers, 45 rejected/unsupported; details in `tools/original_3ds_audit_0522.tsv`.
- Portable C++ geometry snap test: PASS.
- Original Outer Wall 1 (2 planes), ComBuild box (1 box) and prior Fabr Tower (6 planes) standalone reader: PASS. Original 3DS CTest source fixture assertions included for Windows.
- Windows MSVC compile, full CTest, installer and ProTLVK behavior: NOT run here.
