# ProTanki Editor PRO 0.5.23 — legacy handedness and orientation fidelity

Changed-files source patch over **0.5.22**.

The main correction is the legacy GTanks coordinate basis used by the viewport. 0.5.22 tried to compensate with a 180° camera change, but the Silance reference showed that the map itself was being interpreted through a mirrored Y conversion. 0.5.23 uses the same left-handed `(x,z,y)` basis for map positions, 3DS visuals, rotations, collision preview, gameplay overlays, picking and Edge Snap, and removes the camera workaround.

Legacy XML values are not automatically flipped or rotated on load/save.

See `RELEASE_NOTES_0523.md`, `BUILD_STATUS_0523.md` and `PATCH_0523_APPLY.txt`.
Windows/MSVC and ProTLVK/original-editor visual validation are still required.
