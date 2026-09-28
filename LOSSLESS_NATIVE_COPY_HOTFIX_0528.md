# ProTanki Editor PRO 0.5.28 — Lossless Native Copy hotfix

This patch follows the Esplanade copy guard and the verified Fogtown ground fix.

## Why this exists

A full Ctrl+A / Ctrl+C / Ctrl+V of `map_silence.xml` stopped at source prop 78 (`Outer Walls/default/Tunnel 2`). The old placement pipeline tried to reinterpret every copied object through its 3DS collision helpers. `tunnel_2.3ds` uses `Box02` as its *visual* pivot while other `Box*` nodes are real collision helpers, so the native helper reader reported `3DS visual anchor/pivot is not available` and rolled the whole transaction back.

More importantly, a full-map copy should not have to reverse-engineer data the source map already contains.

## New full-map clipboard path

When **every static prop is selected**, Ctrl+C now captures:

- every static prop (including its complete original `<prop>` subtree),
- every native collision plane,
- every native collision box,
- every native collision triangle,
- complete raw XML for each copied collision primitive so unknown attributes/children survive.

Ctrl+V then transforms and appends that native bundle directly. It does **not** regenerate the full map's collision from 3DS helpers. Existing in-memory collider ownership is remapped when known; unbound legacy collision is still copied as native data instead of being discarded.

New collider IDs are generated only to keep IDs unique. All other unknown source attributes/children on the copied prop/collider nodes are preserved.

A lossless clone batch is kept together in memory: partial deletion of that batch is blocked so opaque collision cannot be orphaned. Deleting the complete copied group removes its complete cloned collision bundle.

## Tunnel 2

Individual placement/copy of the original `OuterWalls/tunnel_2.3ds` is also recognized explicitly from its verified source topology:

- visual pivot: `Box02` (21 vertices / 12 faces),
- box helpers: `Box10`, `Box11`, `Box09`,
- plane helpers: `Plane06`, `plane03`, `plane04`.

The regression expects 3 planes + 3 boxes and no triangles.

## Safety boundary

The opaque native bundle is used only when the selection covers the **entire static map**. For a partial selection the legacy XML has no universal prop-to-collider relation, so the existing ownership/safety checks remain in place rather than guessing and copying unrelated collision.

Functional elements (spawns, flags, DOM points, bonuses, special zones, lights) are not silently duplicated by Ctrl+A static-map copy; they keep their existing dedicated editing/copy paths.

## Verification in this source package

- Python audit suite: 80 passed (+ 4 subtests).
- `tools/check_source_consistency.py`: PASS.
- `NativeCollisionImport` was compiled in an isolated CPU test against the supplied original `tunnel_2.3ds`; result: visual `Box02`, 3 planes, 3 boxes, 0 triangles.
- New Windows CTest target: `LosslessFullStaticMapClone` checks opaque prop/collider XML preservation, transformed positions, unique collider IDs, save/reload counts.
- Existing Esplanade CTest now also checks `tunnel_2.3ds`.

A full MSVC/GitHub Actions build and ProTLVK gameplay test still need to run on the Windows build produced from this patch.
