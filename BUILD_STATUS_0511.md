# 0.5.11 — Windows build status and manual light validation

This is a complete source candidate, **not** a compiled Windows installer. Source preflight, nine Python audit tests and four portable C++ tests ran locally. The new `NativeLightXMLRoundTrip` CTest is registered but cannot run here because the application requires Win32/MSVC and DirectXMath. GitHub Actions must successfully compile the new `NativeLightRegression` target and execute CTest before distributing the generated setup.

Use a **copy** of an existing original Fogtown map. Open Lighting, enable markers, select an existing omni light, change color/intensity/attenuation and position and Save As. Reopen saved XML and compare `<lights>` with the original, including unrelated waypoints/collisions. Add an independent light at a selected lamp prop, adjust native Z, duplicate, delete, undo/redo and Save As twice. Open a blank map and add a light. In the original ProTLVK32 tester, verify that the lamp and new native omni record are illuminated as intended. The editor itself renders symbolic light markers, not runtime illumination.

A lamp prop is **not** assigned an automatic light: original map format stores them as separate records without a verified owner link. Animated holiday sprite pulse and in-game GLB asset export are not implemented or claimed. Keep original NY/Fogtown map and game assets untouched.
