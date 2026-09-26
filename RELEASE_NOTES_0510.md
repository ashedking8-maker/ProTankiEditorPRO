# ProTanki Editor PRO 0.5.10 — native gameplay authoring update

Building on the 0.5.9 source candidate, this update changes only the native gameplay authoring workflow and coordinated package version.

- Selected and pending spawns rotate by 45° per X; Shift+X reverses. Eight key presses complete a turn independently of static-prop rotation snap. Exact Z angle can still be entered in radians.
- The drop-creation palette now exposes `free` and `parachute` before placement. These and the explicit `game-mode` nodes are serialized to native map XML.
- Known token choices: `armorup`, `damageup`, `nitro`, `crystal`, `crystal_100`, `medkit`. This is **not an exhaustive game-wide list**: the UI also offers exact type names from existing regions in the opened map and an advanced native-token field. Arbitrary names are not promised to work in ProTLVK.
- If no game mode is checked, creation is **disabled**, rather than silently reassigning DM. An existing region cannot have its final explicitly selected mode removed through Properties; XML with originally missing mode nodes is preserved and labelled unspecified, not interpreted as proof of universal game availability.
- A Windows CTest target covers eight headings, reverse rotation, mode bitmasks and native token handling. The functional map CTest also checks TDM+CTF only, false free/parachute and 45° spawn serialization and reload.
- Setup EXE, portable ZIP, both GitHub workflows, NSIS display version, CMake, Windows resource, logger title and source preflight all use 0.5.10.

## Release status

Windows x64 MSVC, CTest, packaging and original ProTLVK functional test **must be run for 0.5.10**. Local source checks or portable single-file C++ tests do not establish a successful Windows build. Prefer Save As on an XML map copy for compatibility testing. Full native GLB/modified mesh -> 3DS+library.xml export remains unavailable, as in 0.5.9.
