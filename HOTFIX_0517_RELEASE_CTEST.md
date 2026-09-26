# 0.5.17 / Release CTest hotfix (same application version)

GitHub Actions commit d9f7983: source preflight PASS, 32 Python audit tests PASS,
MSVC x64 Release editor build PASS; CTest 15/16, failure only in
`IsolatedObjectDraftRoundTrip` (0xc0000409).

Root cause: `tests/object_draft_regression.cpp` used `assert(SaveNew(...))`
and `assert(Load(...))` for operations with side effects. Release sets `NDEBUG`,
so these statements are compiled out. Files are not created, the subsequent
manifest edit accesses missing data, and the test terminates. The same build
blind spot affected assertions in `volume_picking_regression.cpp`.

Fix: both tests now use `PT_REQUIRE` (`tests/ReleaseTestCheck.h`), which always
evaluates the expression, reports the failing line, and returns nonzero. Added
a Python audit guarding against reintroducing Release-disabled `assert` in
these regressions. No application, map-format, ObjectDraft implementation,
project version, or workflow settings were changed.

Verification: run `python tools/check_source_consistency.py`,
`python -m unittest discover -s tests -p "test_*audit.py" -v`, then Windows
`ctest --test-dir build -C Release --output-on-failure`. Portable g++ -DNDEBUG
checks were also run on the two modified tests. Only GitHub can confirm MSVC
CTest and packaging on Windows. Do not mistake a green build for verified
ProTLVK gameplay compatibility.
