# Workflow 2 validation and limitations

This release retains the Import Fix 8 mesh anchor/helper filtering and legacy material resolution, and changes editor interaction and gameplay inspection. It does not rewrite map sections when changing the game-mode view.

## Map modes

* All: displays every type of gameplay element when its overlay is enabled.
* DM: `spawn-point type=dm`, bonus regions whose `game-mode` includes `dm`.
* TDM: `spawn-point type=red/blue`, bonuses with `game-mode=tdm`.
* CTF: red/blue spawns, the `flag-red` and `flag-blue` positions, bonuses with `game-mode=ctf`.
* DOM/CP/CTP: `spawn-point type=dom`, `dom-keypoints`, bonuses with `game-mode=dom`.
* `special-geometry` kill/kick zones are an independent toggle, not part of the game-mode filter.

These filters are editor views only. They are not separate XML versions or converters. The original map may contain multiple mode sections concurrently. CTF flags are editor-only flag wire models drawn from `<ctf-flags>`, never false `<prop>` records.

## Custom navigation and shortcuts

`%LOCALAPPDATA%/GTanksNextEditor/controls.ini` stores the selected mode, optional shortcut labels, four navigation sensitivity profiles, Custom mouse gestures and all Custom keyboard bindings. The manual is initially closed. Custom starts from Simple focus defaults; the Keyboard tab can rebind actions and their Ctrl/Shift/Alt modifiers. Duplicate combos are warned about. File operations and gameplay/XML transformations remain explicit. The editor does not synthesize or edit special gameplay entities in this release.

## Automated regression

`ImportRegression` checks known original `.3ds` visual parts and ignored helper nodes. `MapEditRegression` verifies basic add/delete and preserved collision, spawn, bonus, DOM and CTF sections. Run `ctest --test-dir build -C Release --output-on-failure` in Windows Actions. For comparison of exported maps after Add/Delete, use `python tools/verify_legacy_xml.py --allow-prop-add-delete original.xml exported.xml`. Without the option, the tool strictly verifies same prop identities/order and changed transforms only.

Limitations: full Windows/D3D11 runtime requires GitHub Windows runner and visual verification; moving gameplay entities is not implemented. Broken Roof black pixels were not globally removed because they are present in the source texture atlas and require reference comparison.
