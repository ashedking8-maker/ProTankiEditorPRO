# ProTanki Editor PRO 0.5.27 – Discord reports and session-only asset previews

## Manual library selection

- No startup scan of a `library` folder. Opening an XML map no longer auto-selects an adjacent library.
- A user must explicitly select the external library **every time the application starts**. Existing `lastLibraryDirectory` is not read and is removed from preferences on startup. The loaded index is memory-only and is freed on exit. The original user library files are not modified or copied into the installation.
- Browse thumbnails use only session RAM/GPU memory, no persistent thumbnail directory and no disk-based library cache. LRU limit increased from **96 to 2048** 194×146 thumbnail textures (approximately 222 MiB of raw RGBA pixels at the cap). Generated previews survive scrolling a large 80-object library, but may be evicted after viewing many thousands of variants; they are always cleared when a different library is selected and on exit.

## Water and other graded-alpha PNG materials

- Detects fractional alpha in WIC-loaded textures, splits opaque/cutout and graded-transparency rendering.
- Transparent 3DS parts are depth-tested without depth writes and drawn back-to-front at instance granularity using alpha blending. Opaque functional models draw first. These changes affect **editor viewport and thumbnail previews**, not source textures, collision XML, 3DS/TARA export or the ProTLVK renderer.
- Simplified sorting cannot exactly reproduce the game's rendering for overlapping transparent meshes; validate the result in the Windows build.

## Report a Bug

- New Cloudflare Discord Worker source: `report_gateway/discord/worker.mjs` and independent offline Node regression test.
- Public Worker URL remains build-time `PTPRO_BUG_REPORT_ENDPOINT`; secret Discord webhook stays in Cloudflare only.
- Existing opt-in log excerpts are sent as a Discord file attachment when selected.
- After a successful HTTP 202, native client stores a **30-minute timestamp only** in LocalAppData and silently ignores repeated clicks, even after relaunch. Send continues to look enabled; it never collects logs or calls the network during local cooldown.
- Server applies separate *hashed* 30-minute KV checks for installation ID and public IP. Duplicates get the same 202 without sending to Discord. KV is eventually consistent: not a strict atomic protection against simultaneous requests. For a public release with abuse resistance, use a Durable Object or Cloudflare rate-limit rules.

## Build and status

Updated version strings / Windows resources / package and workflow names to 0.5.27. The source preflight, read-only audit suite and offline Node Worker tests are available. Full MSVC compile, CTest, actual Windows water visualization and real Discord delivery with log attachment still require a GitHub Actions build and user acceptance test. **No Windows installer is included with this source release.**
