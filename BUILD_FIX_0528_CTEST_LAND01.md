# 0.5.28 — Windows CTest Land01 integration fix (second hotfix)

This hotfix is based on 0.5.28-FULL-SOURCE-MSVC-FIX. It keeps the earlier aiVector3D/MSVC compile correction.

GitHub Actions compiled the Windows executable and the integration-test executable, then failed at NativeLand01EndToEndExport (`tests/native_object_export_integration.cpp`, line 89, `elevated`). The reference's single raised peak moves horizontally too, whereas the test wrongly required its X/Z coordinates to remain unchanged. Original Land01 also has a nonzero 3DS pivot; the imported local vertex coordinates already account for that pivot.

The revised integration test compares full XYZ positions in both directions, requires exactly one unmatched vertex on each side and confirms that its height increases. It keeps the original importer's vertex order, the native export and 138-collision-triangle readback checks, untouched-original protection and unsupported-vertex-count rejection. No exporter, collision algorithm, Browse cache or Discord Worker logic is changed.

Upload `tests/native_object_export_integration.cpp` to the same path in the existing GitHub repository, replacing the older file, then run the Windows workflow again. Use the full-source alternative only if a full repository replacement is intended. Windows CTest and ProTLVK gameplay validation remain pending.

Separate issue: the supplied workflow log passes `-DPTPRO_BUG_REPORT_ENDPOINT=""`. Configure the repository Actions variable `PTPRO_BUG_REPORT_ENDPOINT` to the public Worker `/api/report` URL before testing Report a Bug in the desktop build. Do not put the Discord webhook in the repository.
