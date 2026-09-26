# ProTanki Editor PRO 0.5.22 — camera and native helper fidelity (source candidate)

Apply the **changed-files** patch over a complete 0.5.21 repository, not over 0.5.20. This update includes camera-only view reversal, geometry-checked original Outer Wall 1 helpers, scaled orthogonal box helpers, explicit placement rollback logging and opt-in edge/height placement tools. The legacy XML coordinate basis remains untouched.

See [RELEASE_NOTES_0522.md](RELEASE_NOTES_0522.md), [BUILD_STATUS_0522.md](BUILD_STATUS_0522.md) and [PATCH_0522_APPLY.txt](PATCH_0522_APPLY.txt). Windows build and game validation are still required. The 45 remaining unsupported original 3DS files are documented rather than silently authored with guessed collision.

---


# ProTanki Editor PRO 0.5.19 — Native Box Helpers and Source Properties (source candidate)

Patch 0.5.19 follows 0.5.18. It adds source-derived `box*` collision XML authoring and
read-only XML property inspectors. Original library snapshots are preserved; Obj.
Editor draft field selections are recorded separately and are **not** native game
export. Windows build and original game behavior still require testing.

See [RELEASE_NOTES_0519.md](RELEASE_NOTES_0519.md) and
[BUILD_STATUS_0519.md](BUILD_STATUS_0519.md). Install with
[PATCH_0519_APPLY.txt](PATCH_0519_APPLY.txt).

---

# ProTanki Editor PRO 0.5.18 — Compact UI and Bridge 1 rendering source candidate

Windows x64 / Direct3D 11 C++20. This is a source candidate. Windows MSVC compile,
CTest, installer packaging and runtime/game compatibility have NOT been tested for
these 0.5.18 changes. Upload the contents, including `.github/`, to your existing
repository, then inspect GitHub Actions before distributing the generated installer.

Read **[RELEASE_NOTES_0518.md](RELEASE_NOTES_0518.md)** and **[BUILD_STATUS_0518.md](BUILD_STATUS_0518.md)** for the current patch and its exact validation status. The patch also includes the prior 0.5.17 Release CTest repair. Earlier release notes remain available for rollback.

## Current changes

Full read-only `library.xml` and per-object source snapshots, guarded original
prop-subtree duplication, and optional original-library XML reference sidecars
in isolated Object Editor drafts. Advanced opaque copy is OFF by default and
valid for the next placement only. It does not validate unknown game semantics.
No native export of GLB authoring drafts has been enabled.

## Prior changes

- Native Lighting dock for existing `<lights>/<light type="omni">`: add at selected prop or click to place, change color/intensity/fade/position, duplicate/delete/undo. Light markers in the viewport are diagnostic only. Original lamps and lights remain separate map XML items; there is no invented `glow` or pulse tag. Unknown existing light types pass through without editable controls.
- A new native light round-trip CTest guards original metadata, independent lights, undo, repeated save and blank map creation.
- Visible gameplay bonus/drop regions have thicker screen-space outlines and their
  projected faces can be clicked, not only the exact line/center. LMB drag moves a
  selected volume; Ctrl+drag a top handle resizes X/Y. Native XML min/max is updated.
- X / Shift+X rotates an existing or pending spawn by +45/-45 degrees (eight headings). A selected spawn has a direction
  arrow, and its rotation and type/team are serialized to native map XML.
- Choose bonus contents (armorup, damageup, nitro, crystal, crystal_100, medkit)
  and explicitly selected game modes before placement. Free/parachute are selectable
  before placement and editable afterward. Zero selected modes block creation rather
  than silently adding DM. Additional native type tokens from this map can be reused.
- Browse Library offers a remembered thumbnail size. Silver-grey theme is darker
  with light text. Copying placed library props adds their source identity to AX recents.
- Object Editor uses source mesh positions in the correct coordinate basis; shows
  editable vertices and triangle faces, supports adding points and triangles,
  dragging points in camera plane, editing point coordinates, scale, and saving
  a v4 isolated draft (v2/v3 load supported). The separate placeholder collision box is hidden by default.
- NSIS uninstall registry commands use correctly quoted paths.

## Safety and limitations

**The Object Editor draft is not a game-ready asset.** Vertex edits, custom faces,
mesh scale, color tint and collision boxes are stored in its separate draft document;
this is NOT a native 3DS/library.xml export. Original game libraries are never
silently rewritten. Read [RELEASE_NOTES_0510.md](RELEASE_NOTES_0510.md) for the
verification checklist and unresolved game-compatibility work.

Use *Save As* on copies of legacy maps; verify saved gameplay and native light changes with your
unmodified original ProTLVK32.exe. The game XML has no explicit prop-to-collider
ownership link; deletion with ambiguous shared collision remains intentionally
conservative.
