# 0.5.25 verification status

- Changed-files source patch over complete 0.5.24. Contains no signed executable or hosted reporting backend.
- Portable C++ tests: `NativeTerrainDelta136GameVerified` fixture compares 138 output triangles to the user-tested source and checks importer readback, unsupported multi-edit refusal and binary TARA encode/decode. Existing `Native3DSWriterRegression` checks remain.
- JavaScript mock service test validates 30-minute duplicate suppression by both installation ID and public IP, private destination, and expected 202 acceptance. This is not an actual Cloudflare/Resend deployment.
- Python source consistency and 54 Python regression tests passed before ZIP assembly; verify the GitHub workflow independently. No local Windows/MSVC environment or ProTLVK runtime is available; only the earlier **manual reference fixture** was game-tested by the user.
- A Windows build without a public `PTPRO_BUG_REPORT_ENDPOINT` compiles the dialog but disables Send with an explicit explanation. Set endpoint and deploy the backend before expecting reports in the maintainer inbox.
