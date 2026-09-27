# 0.5.27 source candidate verification

- Source base: full 0.5.26 POSTTEST package.
- Manual selection: no automatic library scan at startup/map open; no persisted last-library path.
- Thumbnail cache: memory-only, 2048 cap; zero persistent thumbnail cache files.
- Graded-alpha water: distinct sorted blend path implemented in editor renderer, not yet validated on Windows GPU.
- Cloudflare Discord gateway: worker source and isolated offline request/attachment/cooldown tests included.
- Native cooldown: LocalAppData timestamp, report ID kept separately, raw libraries never saved to app data.
- Python source consistency and audit suite: see local tests; MSVC/CTest and delivery remain pending.
