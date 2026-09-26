# 0.5.10 build status

- Source preflight: PASS (Linux authoring environment).
- Nine Python audit tests: PASS.
- Portable C++ gameplay authoring regression (eight spawn headings, reverse, known and custom tokens, explicit mode masks): PASS via g++ C++20.
- Portable projected gameplay picking regression: PASS via g++ C++20.
- Existing native XML save/load regression was extended to test TDM+CTF-only drop, false free/parachute, 45-degree spawn roundtrip, and preservation of an originally unspecified mode list. It is registered for Windows CTest but was **NOT RUN** locally (the MapDocument executable requires Windows/DirectX dependencies).
- Windows MSVC build, complete CTest, NSIS packaging, runtime, and ProTLVK gameplay compatibility: **NOT RUN for 0.5.10**.
- Full game-native GLB/mesh export remains disabled.
