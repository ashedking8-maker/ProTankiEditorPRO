# 0.5.9 verification status

Source preflight: PASS on the authoring Linux container.
Read-only Python native/map delta audits: 9 PASS on the authoring Linux container.
Portable g++ smoke tests: ObjectDraft roundtrip including four edited vertices and two
triangles, projected gameplay volume hit testing, first-run path settings: PASS.

Windows MSVC compile and Windows CTest: NOT RUN for 0.5.9. GitHub Actions will
compile the editor and run ten CTest targets (including ProjectedGameplayVolumePicking).
NSIS installer warnings: quotes were adjusted; NOT YET verified by Windows NSIS.
ProTLVK game-compatibility check for gameplay XML and imported/editable meshes: NOT RUN.
GLB / edited draft -> game-native 3DS/library.xml exporter: NOT IMPLEMENTED.
