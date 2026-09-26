# Windows Defender and Windows release verification

Windows Defender quarantining an installer cannot be diagnosed solely from our map editor log. Ask for Protection History's **exact detection name**, original file path, detection time and the matching build's SHA-256. A new, unsigned NSIS package can receive reputation or heuristic warnings, but that does not prove the detection is false. Do not turn Defender off or add blanket antivirus exclusions.

1. Build from reviewed repository code using the published GitHub workflow; distribute a documented release and checksum manifest.
2. Submit the detected installer/EXE to Microsoft's [security sample submission portal](https://www.microsoft.com/en-us/wdsi/filesubmission) for a false-positive review if appropriate.
3. For release distribution, obtain a publisher code-signing certificate and sign the installer and executable; verify the signature in Windows. Signing cannot guarantee zero warnings.
4. Prefer the standalone ZIP for diagnostic comparison only; check that an unsigned EXE is not being replaced unexpectedly, and inspect the exact Defender verdict.
5. The `Test...` button only starts the user's chosen `ProTLVK32.exe`, with no game configuration or map edits.
