# ProTanki Editor PRO 0.5.7 - first MSVC compilation fix

This update repairs the two compilation blockers from the 2026-09-24 GitHub
Actions run at commit `f488a3c` (the first run that reached MSVC).

- `src/MapDocument.cpp`: call the declared `Log::Warning` API rather than the
  nonexistent `Log::Warn` API in both invalid-delete branches.
- `tests/library_reload_regression.cpp`: replace an accidental literal newline
  inside the final `std::cout` string with the C++ `\n` escape and return 0.
- `tools/check_source_consistency.py`: validate all `Log::...()` calls against
  `Logger.h`, and check that the library reload regression has no malformed
  regular string/char literals. These two historical errors are now caught
  before lengthy CMake/MSVC compilation.

Local checks performed: source preflight; five Python audit tests; GCC syntax
checks of the library reload, guidance, and object-draft test source files;
standalone execution of the guidance and object-draft regression binaries;
negative preflight checks for both defects. These checks are **not** a verified
MSVC full build or a ProTLVK game validation. GitHub Actions must run the full
Windows compile, CTest, installer packaging and publishing again.

Apply the full source archive to the repository root, including `.github`.
Alternatively replace the relative paths in the narrow patch archive. Do not
create another repository or place the archive contents in a nested folder.
