# ProTanki Editor PRO 0.5.14 — collision and workflow candidate

- Includes previous 0.5.13 grid phase, gameplay copying, spawn and drop-zone property handling, lighting controls, independent object drafts, theme and startup UX.
- Native collision for **Beach / sidewalls / Wall End 2** authored from original ProTLVK map reference: 6 planes + 10 triangles per new instance. Copy, position, yaw rotation, post-save/reload ownership and deletion use the same verified primitive set. Existing old visual-only Wall End 2 props can be selected and explicitly repaired from Properties, with duplicate/partial native-shape protection; existing maps are NOT silently mutated on load. For all other props, placement warns that collision is not validated and may be visual-only.
- Existing map collision order and unrelated legacy nodes remain unchanged on export; newly authored primitives are appended with native tags.
- Startup has a real initially clean blank map, so immediately placing objects after loading a library renders the placed mesh. No automatic unsaved-map prompt for an untouched startup workspace.
- Save draft and close draft confirmations are fixed-size centred dialogs.
- Native arbitrary GLB game export is not yet implemented. Reopen edited source mesh through Object Editor > Open saved draft rather than via standard library scan.
- Source candidate: run GitHub Actions MSVC/CTest and ProTLVK tests; neither is claimed to have run locally.
