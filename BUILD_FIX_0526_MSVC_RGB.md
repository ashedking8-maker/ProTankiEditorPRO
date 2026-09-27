ProTanki Editor PRO 0.5.26 - MSVC RGB macro build hotfix

Cause: Windows SDK defines RGB(red, green, blue) as a function-like preprocessor macro. The new Native3DSWriter.h declared RGB(array<float,3>), causing errors C4003/C2143/C2440 in the Windows application target. The 54 Python audits passed but they could not detect this Windows-specific compilation failure.

Fix: Renamed the 3DS helper to FloatRgbChunk and updated all its call sites. The native visual metadata regression now also compiles with an RGB macro defined before including the header, preventing recurrence.

Apply: Unzip the small HOTFIX zip in the ROOT of the existing GitHub repository, replacing src/Native3DSWriter.h and tests/native_visual_metadata_regression.cpp. Keep every other file. Commit and rerun the Windows workflow. Do not install/use this as a binary installer.

Verification in this environment: Python source consistency PASS, 54 Python audits PASS, native visual smoothing/material regression compiled and passed on Linux, RGB-macro compatibility probe compiled and passed. Full Windows MSVC compilation, all CTest tests and ProTLVK gameplay still need to pass on GitHub/Windows.
