ProTanki Editor PRO 0.5.28 - urgent RMB placement cancellation hotfix

WHY: AX history removal confirmation can stay armed and the old viewport-only
RMB handler explicitly refused to cancel a ghost when axConfirmRemove_ was set.
The original log cannot prove the exact input state; this removes the vulnerable
routing and gives placement cancellation priority before any UI widget.

Changed files (extract the PATCH archive at the repository ROOT, preserving paths):
 src/EditorUi.cpp
 src/EditorUi.h
 tests/test_placement_rmb_0528_audit.py
 .github/workflows/build-windows-installer.yml (setup-node v4 -> v5)
 .github/workflows/release-windows.yml (setup-node v4 -> v5)

Behavior: any RMB click during active normal/gameplay/light placement cancels
the preview, clears pending commits, never places an object, never deletes an
AX history entry, never orbits the camera for the same press/release. Normal
RMB orbit is unaffected when not placing. Hidden AX confirmation is disarmed.

Build: Commit changed FILES to main; wait for a NEW GitHub Actions run. Download
the NEW Setup.exe or Portable.zip from that run/latest-build, not the old EXE.
Test normal, clipboard and functional placement, including a previously opened
AX removal confirmation; verify RMB cancels and LMB/Space commits normally.
The native Windows build/runtime were not run locally; GitHub must compile.
Cloudflare Worker and GitHub report endpoint are unchanged.
