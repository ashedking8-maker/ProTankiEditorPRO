# ProTanki Editor PRO 0.5.28 — Browse cache and native Obj. Editor validation

## Browse Library memory (session-only)

- Libraries are **never loaded automatically** on startup or map open. The user must manually select the external library at every launch. Its original files are untouched.
- No fixed thumbnail count. Render **one expensive visible 3DS thumbnail per frame**; reupload up to four previously compressed previews per frame. Hidden/collapsed groups do not generate previews. Cached failures display `Preview unavailable` rather than infinite retries during that opening.
- GPU previews are held while Browse is open. Adaptive, byte-based budget: 64 MiB on integrated/unknown GPUs; up to 256 MiB on dedicated GPUs (dedicated VRAM / 16, clamped to 64–256 MiB). Least recently used, non-visible GPU previews are evicted when above budget. Visible thumbnails are never evicted from the active frame's ImGui draw list.
- A separate **128 MiB compressed in-memory PNG cache** is used for reopening Browse during the same application session. GPU thumbnail resources are released on the first frame after Browse closes. CPU PNGs are cleared when the user selects/reloads a different library or exits the app. No thumbnails, indexes, or library paths are written to a disk cache.
- Original 3DS meshes/textures loaded temporarily to generate thumbnails are released immediately after each render. Existing map objects and their renderer resources are unaffected. Windows may keep released memory in the process allocator for reuse; freed GPU resources do not imply an immediate Task Manager graph drop.

## Obj. Editor native export hardening

- Preserves the existing `NativeTerrainDelta` algorithm and the previously user-tested Land01 136→138 collision-triangle fixture; **not a rewrite of that proven case**.
- Adds an end-to-end Windows CTest candidate: original Land01 → derive reference single elevated point → export new library/3DS/TARA → reimport and check native 138-triangle collision, plus non-overwrite/rejection checks.
- On exported terrain, re-reads actual helper vertex coordinates from 3DS and compares them to authored coordinates, rather than checking only triangle count.
- Explicit native collision policy: verified triangle-only terrain goes through `NativeTerrainDelta`; helperless/box-only originals require explicitly authored SOLID boxes. Original plane or mixed helper sources are **rejected** for physical native conversion instead of silently being replaced with unrelated boxes. Rotated source boxes are not claimed to be preserved.
- Draft-only additions/retopology, multi-vertex terrain changes, native GLB export and trigger/gameplay conversion remain unsupported and fail closed. Do not label the entire Obj. Editor 100% compatible until user tests pass in ProTLVK.

## Discord / distribution

- Bumps client report version to `0.5.28`. **Deploy the bundled updated `report_gateway/discord/worker.mjs`** before testing reports: the older Cloudflare Worker rejects new versions. Keep existing Secret `DISCORD_WEBHOOK_URL` and KV binding `REPORT_LIMITS` unchanged.
- Existing hidden persistent local cooldown and best-effort server KV per-installation/IP checks remain. KV is not an atomic lock for concurrent abuse traffic.
- Updated GitHub build, Windows resource, installer/portable filenames and release metadata for `0.5.28`. Source archive, not a Windows executable.

## Verification status

Local source preflight, Python source audits, offline Node Discord tests and portable C++ cache/collision-policy unit tests are available. Full MSVC compilation, Windows GPU PNG/VRAM behavior, end-to-end Land01 exporter CTest and in-game ProTLVK behavior **must be verified by GitHub Actions and the user's Windows tests** before public release.
