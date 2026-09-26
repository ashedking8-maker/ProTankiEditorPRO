# ProTanki Editor PRO 0.5.25 — release notes

Base: full 0.5.24 source, including 0.5.23 CTest hotfix. This archive contains **changed/new source files only**, not a compiled Windows installer.

## Native game object export

- Exports a new independent `PTPRO_<name>` library directory **and a correctly formatted `.tara` file** alongside it. Preserves `library.xml` and copies the original texture variants, with no overwrite of original game files or previously exported custom assets.
- Checks the generated 3DS by the editor's own native helper importer before publishing; verifies triangle/box counts and visual mesh vertex/face counts, with additional box dimension checks.
- For verified original **triangle-only** heightfield collision (like Beach/Land01), preserves original helper triangles and transfers a single edited peak using a checked height-delta and a split into **138 triangles**. The regression compares all output coordinates against the manual Land01_AX file that was confirmed to work in ProTLVK.
- Unsupported multi-vertex terrain changes, incompatible frames, GLB, triggers, non-verified gameplay fields and arbitrary retopology do **not** silently create a pass-through or wall collider. Other 3DS props still export their explicitly authored solid boxes; this is not general original-helper preservation for every type.
- Restores the source library textures to the isolated draft *preview* when the source library is loaded, without modifying the original draft files; makes the native export confirmation dialog readable.

## Bug reporting and privacy

- Small **Report a Bug** tab immediately above/right-aligned to `Seek Help | ProTanki Discord`, subject/details modal, optional unchecked log-attachment checkbox.
- Explicit Send over Windows WinHTTP/HTTPS to a configurable PUBLIC gateway, with redirect disabled, network timeout and a truthful server acceptance result. UI send is disabled until configured. It does not quietly mail on startup or embed the maintainer's private inbox/credentials.
- Optional standalone Cloudflare Worker + Durable Object + Resend reference gateway (`report_gateway/`). Recipient, verified sender, Resend key and keyed-hash secret live exclusively on the server. Server persists 30-minute per-installation and per-public-IP throttles and accepts/groups duplicate reports without a second email. End-users are told that repeated reports may be grouped, not that every click was individually emailed.
- Only up to two recent `.log` excerpts when expressly checked. Client attempts to redact email addresses, local paths and credential-like lines; **automated redaction cannot guarantee the absence of all personal data**. No XML maps, models, screenshots or crash dumps are sent.

## Compatibility and status

- Legacy map XML export, 0.5.23 orientation fix and 0.5.24 explicit template validation remain in place.
- Standalone terrain helper regression and gateway mock test pass; source and Python checks pass. Full native Windows/MSVC build, all Windows CTest tests, real deployed reporting service and runtime tests of the new automatic 0.5.25 export still require external validation. Manual Land01_AX 138-triangle fixture is game-tested, **not** a blanket guarantee for other terrain meshes or a deployed web service.
