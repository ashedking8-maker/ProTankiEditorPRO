# ProTanki Editor PRO 0.5.19 — Native Box Helpers + Complete Source Properties

**Changed/new-files patch for the already working 0.5.18 repository.** Extract the ZIP and upload its directory contents, including `.github/workflows`, preserving folder names. Do not upload the ZIP itself as a single repository file. The patch does not contain the unchanged assets from earlier releases.

## Native collision

- Adds original `box*` 3DS helper import alongside existing `plane*` and `tri*` collision. The importer uses validated helper vertex bounds and visual anchor, **not** the visible house's bounding box. For each placed object, author corresponding `collision-box` XML, attach an owner for move/rotate/delete, and rebind the complete original collider set after save/reload when there is exactly one safe match.
- Original `NuBu 3 / NuBu 3` fixture has two source box helpers. An independent fixture extracted from the original editor-authored map matches **both** boxes' size and world placement. Original Wall End 1 / Hs_part06 / Bridge 1 fixtures stay in the CTest.
- Unsupported helper geometry, nonidentity source transforms, incomplete original collision ownership, or unsafe duplicates still fail closed; this is **not** a promise that all 1,493 library assets have now been game-validated.
- Original source XML including unknown attributes remains intact. New object instances use their source library definitions and now author supported box collision primitives. New GLB drafts **still cannot export native playable 3DS/game assets**.

## User interface and preview

- `Tools > Placement settings` rather than a Library button. The source image preview stays uncluttered; a short placement hint appears only when an object is actively held. The Obj. Editor opens near the available workspace size on each appearance.
- Bridge 1's selective paired-UV-atlas rasterizer now culls the opposite facing used by 0.5.18: user comparison showed that 0.5.18 had bright underside/dark top. Other models remain two-sided. Verify from **above and below** on Windows: pixel parity with ProTLVK is **not** claimed.
- Properties no longer highlights the blanket `Native geometry: unsupported` status. It shows a neutral collision summary plus a collapsible **All imported object properties** inspector: original library `<prop>` XML (including unknown children/attributes), original map `<prop>` XML, source 3DS helper counts and geometry, bound map collision coordinates, and known transform/texture information.
- Obj. Editor shows complete original selected template XML read-only. `Choose draft-inherited fields` provides per-node/per-attribute checkboxes; exclusions persist as the **draft-only** `library-prop-selection.txt`. Complete source sidecars remain unchanged. These choices are *not* applied to native ProTLVK export until a validated custom game exporter exists.
- Removed redundant status prose; details belong in hover tooltips. Original game properties are not modified by toggling display/selection flags.

## Verification and limitations

Portable 3DS helper reading compiled under C++20 with a minimal local DirectX math type stub. Portable Release/NDEBUG `ObjectDraftRegression` executed. Python audit independently compared original NuBu 3 box helpers with the original XML and checks UI/source guards. Source preflight passed. Windows MSVC build, native CTest including `Original3DSHelpersNativeXMLRoundTrip`, installer, Bridge 1 rasterization, and live ProTLVK tank collisions **must still be tested in GitHub Actions and on a backup map**.
