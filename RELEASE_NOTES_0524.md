# 0.5.24 – Native Object Export (experimental 3DS first slice)

- Separate explicit object-export command in Obj. Editor, gated to a complete, byte-matching original 3DS template and solid collision boxes.
- Creates `PTPRO_<draft slug>/ptpro_mesh.3ds`, a new `library.xml`, and copies all named original diffuse variants, never overwriting the game library.
- Serializes edited visual vertices/faces, UV/materials and 3DS Box helper geometry with named keyframe nodes; checks its own output with existing native visual and helper importers before publishing. Failure cleans the staging directory.
- Reloads Library on the next frame after a successful export.
- Retains original 0.5.23 map handedness/XML behavior. Updates Windows packaging version, dedicated 3DS writer regression test and source audit.

## Compatibility boundary

This does not claim full GLB-to-3DS export or independent ProTLVK game verification. Trigger boxes/semantics and nontrivial original gameplay metadata are intentionally rejected. All ORIGINAL plane/triangle collision helpers are discarded in the new object: an authored solid box is a different collision model. `Beach/Land01` is an irregular terrain object whose original 135 triangle helpers are not equivalent to the saved `Land01_AX` single box. Expect the tank to interact differently and do not use that draft as a verified drivable terrain replacement. The source model, XML and texture files remain intact.

To validate: load the supplied saved `test_AX/Land01_AX/object-draft.txt` with Beach as the selected library, click **Export NEW 3DS library...**, find `PTPRO_Land01_AX` in Library, place it, save map XML, verify visuals/texture/box in ProTLVK and send resulting logs if a step fails.
