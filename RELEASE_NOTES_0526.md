# ProTanki Editor PRO 0.5.26 — native 3DS visual metadata

Source candidate. Not a precompiled Windows installer and not yet confirmed in ProTLVK.

- New isolated 3DS exporter reads source 0x4150 per-face smoothing masks and remaps them by original triangle geometry after Assimp reordering, with explicit refusal on unsafe or ambiguous matches. Original group masks (including 0 for flat) are preserved by default. No geometry subdivision is implied.
- Source 0xAFFF material chunks are carried to the new 3DS without reconstructing/replacing their shader/ambient/diffuse/specular/shininess/transparency/texture or unknown extensions. The original material name and mapped face assignments are preserved. Refuse a wrong/missing source texture or unsafe face mapping.
- Obj. Editor > Materials & Shading: per-face Source, smooth all, flat, or common group (1..32). Optional explicit native-material override (ambient, diffuse, specular, shininess, transparency, two-sided, shading 1/2/3), with original values loaded on opt-in. Defaults are preserve source. Draft format 5 stores the choices and loads older formats 2–4.
- Experimental export readback checks 3DS smoothing groups and visible vertex/face counts. Unrecognized source visual layouts fail closed, not silently flattened.
- Report a Bug footer tab now fits text and shares Discord's right edge.
- Added Native3DSMaterialSmoothingRoundTrip CTest.

Limitations: smoothing groups change shading, not mesh detail, and do not guarantee that ProTLVK displays the old Beach material smoothly. ProTLVK game test with 3 separate terrain assets remains mandatory. No new GLB native export or general multi-vertex collision algorithm. Override applies to all original visual materials; additional shader settings remain preserved in raw chunks unless explicitly overridden. Game preview is not an authoritative game renderer.
