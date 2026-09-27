# 0.5.28 Windows MSVC compile hotfix

GitHub Actions: NativeObjectExportIntegration fails at tests/native_object_export_integration.cpp:86 with MSVC C2440: cannot convert const aiVector3D* to const DirectX::XMFLOAT3*.

Fix: match the pointer type to LegacyMeshImport::Vertex::position (aiVector3D), keeping coordinate comparison and copy unchanged.

Install: copy tests/native_object_export_integration.cpp into the same path in the existing 0.5.28 GitHub repository, replacing the file, commit, and rerun the Windows workflow. No editor, library cache, collision, or Discord Worker source files are changed.

Note: the uploaded Actions log shows -DPTPRO_BUG_REPORT_ENDPOINT="". Configure GitHub Actions variable PTPRO_BUG_REPORT_ENDPOINT separately before generating a report-enabled build.

This hotfix has been checked with the source preflight, 64 Python audits and the isolated Node Discord gateway test in a Linux workspace, but it has not been built with Windows MSVC here. Windows build and CTest still require a new GitHub Actions run.
