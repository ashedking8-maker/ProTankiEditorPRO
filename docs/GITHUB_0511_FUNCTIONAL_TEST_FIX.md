# 0.5.11 GitHub CTest hotfix – FunctionalXMLAndGroupHistory

The 2026-09-24 GitHub log shows a successful Windows/MSVC application build,
with NativeLightXMLRoundTrip passing, but 11/12 CTest cases passed.
The only failed test was FunctionalXMLAndGroupHistory. The CI log did not
expose the test process's return code.

A **deterministic error** was found in that test's `unknown-mode-input.xml`
fixture: it omitted `<static-geometry>`, even though the existing legacy
serializer explicitly requires this section. The test then attempted to save
the incomplete fixture and returned checkpoint 106 without an error message.
The fixture now includes `<static-geometry/>` and `<collision-geometry/>`.
It still deliberately omits all `<game-mode>` nodes, which is the behavior the
test intends to cover. No production serialization or gameplay semantics were
changed to accommodate malformed test input.

All nonzero native test return checkpoints now print a diagnostic to stderr,
so another failure will identify the exact assertion in GitHub Actions.
A dedicated Python audit catches regression of this fixture before MSVC.

Local verification: source preflight and Python audits. Windows/MSVC and
full CTest must be rerun by GitHub Actions; they cannot be certified here.
