# NativeCollisionPreview regression fix (0.5.7)

The 2026-09-24 Windows Actions run compiled the editor EXE and 8/9 CTests passed.
NativeCollisionPreview failed because collision plane/box/triangle local vertices
are authored in legacy XML Z-up coordinates, but the diagnostic builder used
`LegacyTransform::World`, which first assumes local coordinates already in the
internal Y-up basis. That double basis treatment displaced the preview.

`WorldFromLegacyLocal` now applies the legacy rotations and translation to XML
vertices and then converts the resulting coordinates to internal Y-up. The
existing `World` method remains unchanged for already-converted model meshes.
The test expects legacy plane center (100,200,300), width=200,length=100,
first local vertex (-100,-50,0) to render as (0,300,-150). The preview remains
read-only; map persistence and physics are unaffected.

Windows MSVC build and CTest must be rerun on GitHub; local source-preflight
checks are not a substitute for the native regression.
