# 0.5.20 verification — Linux source candidate

- Exact baseline: user-uploaded `ProTankiEditorPRO-main.zip` (0.5.19). Patch is diff against that ZIP, not an older synthesized project.
- C++20 standalone import compiled under `-Wall -Wextra -Werror` using a temporary local `DirectXMath` XMFLOAT3-only stub (NOT packaged).
- Source consistency preflight: PASS.
- Python `test_*audit.py`: 41/41 PASS, including original GTanks-authored Waffle Wall 1 and Billboard XML witnesses.
- 1,432 original 3DS files scanned with actual standalone importer: 1,048 accepted native collision, 319 no native collision helper, 65 other technical refusals. 0.5.19 baseline accepted 931, no baseline accept regressed. Counts refer to files, not library definitions or game-tested props.
- Native Windows CTest `Original3DSHelpersNativeXMLRoundTrip` extended to the original rotated fixtures; **NOT executed in Linux**.
- Full Windows application MSVC compile / game runtime / native GLB export: NOT VERIFIED.
