ProTanki Editor PRO v0.5.28 - verified native ground XML collision safety hotfix
============================================================================
Apply this PATCH into the existing repository root, preserving src/, tests/,
CMakeLists.txt and this note; commit to main and wait for a NEW GitHub CI run.
The FULL-SOURCE ZIP is a separate alternative: do not layer two copies.

WHAT WAS VERIFIED
From the user-supplied original map_fogtown.xml, the following Fogtown/l
models have an exact collision-plane per every source-map instance. These six
source 3DS meshes were included as small test fixtures. Original model geometry
is compared with the known native XML footprint at runtime before authoring:
  t11 (500 x 500, local center 0,0)         7,211/7,211 source instances
  t21 (500 x 1000, local center 0,-250)       94/94
  t22 (1000 x 1000, local center 0,0)        218/218
  t32 (1000 x 1500, local center 250,0)    1,091/1,091
  t33 (1500 x 1500, local center -250,-250)   104/104
  t55 (2500 x 2500, local center -250,250)   108/108
  TOTAL 8,826 model instances checked against original source-map planes.
The six reference plane+prop pairs are independently recorded in
  tests/fixtures/fogtown_ground/original_fogtown_ground_six.xml
And the exact user 15-prop repro is included as TEST_MAP_AAA-original-repro.xml.
Its native CTest checks 11 added floor planes, 13 original colliders preserved,
NuBu 2 unchanged and save/load idempotency.

WHAT CHANGES
- New placement / Ctrl+C -> Ctrl+V: attach exact original-game plane when
  the matching source 3DS is a verified complete flat rectangle.
- On Save / Save As: scan current map in linear time and auto-repair missing
  known ground planes from older visual-only saves, without duplicating
  matching pre-existing native planes. Undo snapshot is recorded on success;
  a failed save restores the preflight state.
- A different flat helperless model with NO source-map-verified template no
  longer silently passes as a safe floor. User must explicitly opt into
  visual-only placement or supply a verified source-map template.
- Exact ownership is rebound from XML only when one prop and one plane match.
  Shared/ambiguous original planes cannot be moved/deleted by guesswork.
- New colliders use fresh numeric IDs, preserving all legacy source IDs/fields.
- Properties warn when a known native ground plane is missing; a button can
  repair a selected verified tile (source 3DS required).
- Retains previous RMB cancel and AX only-used fixes.

IMPORTANT LIMITS
This is NOT a generic solid box for all 310 helperless models. Roofs, bridges,
slopes, holes, decals and props without an original source-map collision
reference remain unverified. Collision against every ProTLVK game version
requires in-game confirmation. A deliberate visual-only override remains
available for unsupported flat graphics. Always save a copy before testing.

CHECKS
  python tools/check_source_consistency.py
  python -m unittest discover -s tests -p 'test_*audit.py' -q
  ctest --test-dir build -C Release --output-on-failure (GitHub Windows CI)
  In game: original TEST_MAP_AAA vs re-saved TEST_MAP_AAA: ground must stop tank.
  Also place and copy all six tile shapes and test move/delete and reload.

No Cloudflare/Discord changes. Version remains 0.5.28 (safety hotfix).
