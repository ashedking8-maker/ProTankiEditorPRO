# 0.5.28 source candidate status

Base: complete 0.5.27 source archive. Patch modifies only listed source/build/docs paths; historical 0.5.27 fixtures and safety checks are preserved.

Local verification: source consistency preflight; Python `test_*audit.py`; isolated Node Discord worker; portable C++ tests for byte budget and collision route. This Linux environment has no Windows SDK / MSVC / Direct3D, so GPU WIC codec, Windows build, CTest integration and 3DS/TARA gameplay acceptance are **pending**, not falsely called passing.

Required acceptance tests: open program with no library auto-loaded; select library, scroll 100+ objects and variants, close Browse and reopen within same launch; check faster previews and memory release; reopen app and verify library must be selected again; edit and export Land01 single peak from original file and compare in ProTLVK against user-verified reference; try unsupported plane/mixed source and confirm export is rejected; test separate Discord Worker 0.5.28 deployment and opt-in log attachment after cooldown.
