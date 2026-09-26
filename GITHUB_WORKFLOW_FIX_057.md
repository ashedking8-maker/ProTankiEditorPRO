# 0.5.7 GitHub preflight workflow fix

The 2026-09-24 Windows Actions log stops at `python tools/check_source_consistency.py`:
`FAIL: GitHub workflow release filename mismatch`.

The complete source archive was checked locally, and both workflows inside it reference
`ProTankiEditorPRO-0.5.7-Setup.exe` and `ProTankiEditorPRO-0.5.7-Portable.zip`.
Therefore the GitHub checkout appears to contain at least one stale or mixed-version
workflow (the error does not identify which one). The build never reached MSVC, CTest,
or package creation.

To apply the focused patch, replace these files AT THE SAME PATHS in the repository:

- `.github/workflows/build-windows-installer.yml`
- `.github/workflows/release-windows.yml`
- `scripts/package-windows.ps1`
- `tools/check_source_consistency.py`

Do not upload them inside an extra folder, and do not retain another old workflow
that uses version 0.5.6 filenames. Commit changes and rerun GitHub Actions.
The updated preflight names the stale workflow path, if there is another mismatch.

This packaging/preflight patch does not assert that the MSVC build or game validation
has passed; those steps have not run in the failing Actions log.
