# GitHub Actions 0.5.5 packaging correction

The 2026-09-23 Windows Actions log shows that the MSVC build and both CPack
commands completed. CPack generated:

- `build/ProTankiEditorPRO-0.5.5-Portable.exe` (CPack NSIS; NOT our desired installer)
- `build/ProTankiEditorPRO-0.5.5-Portable.zip`

The checked-out `.github/workflows/build-windows-installer.yml` was still the
older workflow expecting `build/GTanksNextEditor-Setup.exe` and `.zip`. The job
failed in **Verify packages**, after compilation. This is a stale workflow / output
name mismatch, not a C++ compile error.

For 0.5.5, use **only** `scripts/package-windows.ps1` for packaging. It stages the
app, runs our custom `installer/SimpleInstaller.nsi` to produce the normal
`ProTankiEditorPRO-0.5.5-Setup.exe`, and uses CPack ZIP for the portable package.
The custom installer handles the current-user desktop shortcut and optional
finish-page launch; the generic CPack NSIS output does not establish that.

## Applying the fix

1. Replace `.github/workflows/build-windows-installer.yml` in the **main** branch
   with the copy in this archive. GitHub's web editor can edit the existing
   workflow directly; uploading only non-hidden folders will NOT replace `.github`.
2. Replace `.github/workflows/release-windows.yml` for the matching tag workflow.
3. Commit the changes; a push to main starts a new Windows Actions run. Check
   the new run contains a `Package setup EXE and portable ZIP` step, rather than
   `Create Windows installer` running `cpack -G NSIS`.
4. Check both outputs are present and download the Setup EXE from the Actions
   artifact or the latest-build prerelease.

The source preflight now checks the workflows and package script for mismatched
filenames to prevent this same issue from recurring. This static check does not
replace the GitHub Windows compilation, CTest run, or actual install test.
