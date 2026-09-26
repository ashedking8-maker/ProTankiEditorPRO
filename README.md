# ProTanki Editor PRO 0.5.24 — experimental native 3DS object export

Changed-files patch over the COMPLETE 0.5.23 repository **including its GitHub CTest hotfix**. The 0.5.23 map/viewport handedness correction and XML export remain unchanged.

The Object Editor now has a separate **Export NEW 3DS library...** command. It accepts an original, byte-matching 3DS library template and a validated edited visual mesh with solid draft boxes. It creates a new self-contained `PTPRO_*` library directory with `ptpro_mesh.3ds`, copied named texture variants and `library.xml`; original game libraries and original model are not overwritten. It checks the generated 3DS by reimporting its visible geometry and collision boxes before publishing it.

**Experimental limitations:** the original template's plane and triangle helpers are replaced by explicitly authored solid boxes; their original gameplay collision is not preserved. A single box is not a driveable slope. GLB native export, trigger volumes, and arbitrary extended game properties remain unavailable. A saved isolated draft is still a draft, not an exported game object. Color tint is preview-only. Validate exported objects in ProTLVK before production use.

See `RELEASE_NOTES_0524.md`, `BUILD_STATUS_0524.md`, `PATCH_0524_APPLY.txt`.
