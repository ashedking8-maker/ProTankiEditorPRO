ProTanki Editor PRO 0.5.28 — AX Library ONLY USED / Tab+Wheel hotfix

APPLY TO: repository that already contains the 0.5.28 RMB CANCEL hotfix.
Extract and overlay these PATCH ZIP files into the ROOT of the GitHub repository,
keeping paths: src/AxRecentFilter.h, src/EditorUi.cpp, CMakeLists.txt,
tests/ax_only_used_wheel_regression.cpp, tests/test_ax_only_used_wheel_0528_audit.py.
Do NOT upload the ZIP itself as a source file or create src/src.
Commit to main; wait for a NEW Actions build (not Re-run of an older SHA).
Install/download the new Setup.exe or Portable.zip; existing EXE does not change.

Why: AX overlay filtered the displayed rows, but shortcut activation and the
native wheel handler indexed all 24 recently clicked assets. A hidden unused
object could therefore become the placement ghost while 'only used' was checked.

Fix: one shared filtered-position list for UI and keyboard/wheel. No double
wheel handler inside overlay. Reset row when filter toggles. Exact match by
library/group/name in current map. Empty used list cannot activate an unused
recent; without filter, all valid recents remain navigable. RMB hotfix retained.

No Cloudflare/Discord settings changed. No files are written for thumbnails.
Test after install: click many assets but place only two; turn on View > AX
Library: only used; hold Tab and scroll up/down; only those two may preview
and become placement ghosts, including on wraparound. Toggle off and verify
all recent assets become selectable again. Delete last used instance and
verify the now-unused recent is excluded. Test RMB cancelling the ghost.
Native CTest adds AxOnlyUsedFiltersTabWheel; full Windows build/runtime test
still must be confirmed by GitHub Actions and a real input device.
