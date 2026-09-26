# ProTanki Editor PRO 0.5.8 — source candidate status

Basis: 0.5.7 ZIP whose Windows GitHub run successfully built GTanksNextEditor.exe and passed 9/9 native CTest tests. This 0.5.8 source tree includes subsequent changes and has **NOT** yet been compiled by MSVC, nor tested in ProTLVK. Do not label it a validated Windows release until GitHub Actions and an actual editor/game smoke test finish.

Known source-log findings:
- The provided run passed 9/9 tests, made a Setup EXE and portable ZIP, and uploaded a GitHub artifact. NSIS emitted warnings for the uninstaller command quoting (corrected here).
- The user's editor log reports maps with tree props and successful sprite instances (e.g. 177 sprites in one scene), so the libraries were not universally failing to index trees. Critical shader issue: sprite VS calls Texture2D.GetDimensions, while only the pixel-shader stage had its texture bound. Both normal and ghost sprite passes now bind it to VS as well.
- Fogtown/forest.png was reported missing twice, and Minecraft/library.xml failed parsing. These are original-library input issues until compared with source files; no fake replacement asset is generated.
- Existing 3DS helper mesh nodes have not been proven to map automatically to native collision primitives; this candidate shows true imported visual triangle edges alongside independently editable draft boxes, but does not claim game-export parity.

Build by uploading the archive **contents** into an existing repo root (including .github), committing, and waiting for Actions. Test only on copies of game maps. Read RELEASE_NOTES_058.md.
