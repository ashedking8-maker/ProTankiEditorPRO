cmake_minimum_required(VERSION 3.24)
project(GTanksNextEditor VERSION 0.5.28 LANGUAGES CXX)

if(NOT WIN32)
  message(FATAL_ERROR "ProTanki Editor PRO currently targets Windows / Direct3D 11.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build all bundled dependencies statically" FORCE)

# Release builds are self-contained: the target PC does not need the Visual C++
# redistributable or any developer tools installed.
if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

include(FetchContent)

# XML parser
FetchContent_Declare(pugixml
  GIT_REPOSITORY https://github.com/zeux/pugixml.git
  GIT_TAG v1.15)
FetchContent_MakeAvailable(pugixml)

# Legacy .3ds importer
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
set(ASSIMP_WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)

# The editor only READS legacy .3ds meshes. Do not compile Assimp's exporters
# (especially glTF), which are unnecessary here and can break with newer MSVC.
set(ASSIMP_NO_EXPORT ON CACHE BOOL "Disable all Assimp exporters" FORCE)
set(ASSIMP_BUILD_ALL_EXPORTERS_BY_DEFAULT OFF CACHE BOOL "" FORCE)

# Keep the dependency small and deterministic: only the legacy 3DS importer.
set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_3DS_IMPORTER ON CACHE BOOL "" FORCE)
# GLB is imported ONLY in the isolated Object Editor preview.
set(ASSIMP_BUILD_GLTF_IMPORTER ON CACHE BOOL "" FORCE)
FetchContent_Declare(assimp
  GIT_REPOSITORY https://github.com/assimp/assimp.git
  GIT_TAG v5.4.3)
FetchContent_MakeAvailable(assimp)

# Dear ImGui: retained only as a lightweight immediate-mode desktop UI layer.
# All styling lives in Theme.cpp; the editor intentionally avoids the stock ImGui look.
FetchContent_Declare(imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG v1.91.9b-docking)
FetchContent_GetProperties(imgui)
if(NOT imgui_POPULATED)
  FetchContent_Populate(imgui)
endif()

add_library(imgui STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp)
target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_compile_definitions(imgui PUBLIC IMGUI_ENABLE_DOCKING)
target_link_libraries(imgui PUBLIC d3d11 dxgi)

# Public gateway URL; inbox address/API key must be server-side secrets only.
set(PTPRO_BUG_REPORT_ENDPOINT "" CACHE STRING "Public HTTPS bug-report gateway URL (no secrets)")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")
configure_file(src/BugReportConfig.h.in "${CMAKE_CURRENT_BINARY_DIR}/generated/BugReportConfig.h" @ONLY)

add_executable(GTanksNextEditor WIN32
  src/main.cpp
  src/SplashScreen.cpp src/SplashScreen.h
  src/Logger.cpp src/Logger.h src/BugReport.cpp src/BugReport.h
  src/App.cpp src/App.h
  src/Theme.cpp src/Theme.h
  src/MapDocument.cpp src/MapDocument.h
  src/EditHistory.cpp src/EditHistory.h
  src/AssetRegistry.cpp src/AssetRegistry.h
  src/Renderer.cpp src/Renderer.h
  src/SceneRenderer.cpp src/SceneRenderer.h
  src/LegacyTransform.h src/LegacyMeshImport.h src/VerifiedCollisionTemplates.h src/NativeCollisionImport.h src/VerifiedGroundCollision.h src/DraftMeshImport.h src/CollisionPreview.h src/ObjectDraft.h src/Native3DSWriter.h src/Native3DSVisualMetadata.h src/NativeTerrainDelta.h src/NativeExportCollisionPolicy.h src/NativeTaraWriter.h src/NativeObjectExport.h src/PreviewThumbnailCodec.h src/BrowseCachePolicy.h src/AxRecentFilter.h
  src/EditorUi.cpp src/EditorUi.h src/GuidanceState.h src/GameplayAuthoring.h src/GridStep.h src/GeometrySnap.h
  assets/GTanksNextEditor.rc assets/GTanksNextEditor.ico assets/startup-tank.png)

target_include_directories(GTanksNextEditor PRIVATE src "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(GTanksNextEditor PRIVATE
  imgui pugixml::pugixml assimp::assimp
  d3d11 dxgi d3dcompiler shlwapi windowscodecs ole32 shell32 comdlg32 gdiplus dbghelp winhttp bcrypt)
target_compile_definitions(GTanksNextEditor PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)

if(MSVC)
  target_compile_options(GTanksNextEditor PRIVATE /W4 /permissive- /utf-8)
  # The custom app manifest is already embedded by assets/GTanksNextEditor.rc.
  # Disable MSVC's automatically generated manifest to avoid duplicate RT_MANIFEST
  # resource 1 (CVT1100 / LNK1123).
  target_link_options(GTanksNextEditor PRIVATE /SUBSYSTEM:WINDOWS /MANIFEST:NO)
endif()

# ---------------- Distribution ----------------
# Keep the executable at the installation root. The original game `library`
# directory is intentionally NOT bundled; users select their own copy on first run.
install(TARGETS GTanksNextEditor RUNTIME DESTINATION . COMPONENT Runtime)
install(DIRECTORY assets/functional DESTINATION assets COMPONENT Runtime)
install(FILES
  README.md
  docs/LEGACY_OUTPUT_COMPATIBILITY.md
  docs/NATIVE_TEST_AUDIT.md
  docs/DEFENDER_AND_RELEASE.md
  docs/OBJECT_EXPORT_COMPATIBILITY_AUDIT.md
  docs/RELEASE_057_VERIFICATION.md
  RELEASE_NOTES_057.md
  RELEASE_NOTES_058.md
  RELEASE_NOTES_059.md
  RELEASE_NOTES_0510.md
  RELEASE_NOTES_0511.md
  RELEASE_NOTES_0512.md
  RELEASE_NOTES_0513.md
  RELEASE_NOTES_0514.md
  RELEASE_NOTES_0515.md
  RELEASE_NOTES_0520.md
  RELEASE_NOTES_0521.md
  RELEASE_NOTES_0522.md
  RELEASE_NOTES_0523.md
  RELEASE_NOTES_0524.md
  RELEASE_NOTES_0525.md
  RELEASE_NOTES_0526.md
  RELEASE_NOTES_0528.md
  BUILD_STATUS_0514.md
  BUILD_STATUS_0515.md
  BUILD_STATUS_0520.md
  BUILD_STATUS_0521.md
  BUILD_STATUS_0522.md
  BUILD_STATUS_0523.md
  BUILD_STATUS_0524.md
  BUILD_STATUS_0525.md
  BUILD_STATUS_0526.md
  BUILD_STATUS_0528.md
  PATCH_0522_APPLY.txt
  PATCH_0523_APPLY.txt
  PATCH_0524_APPLY.txt
  PATCH_0525_APPLY.txt
  PATCH_0526_APPLY.txt
  PATCH_0528_APPLY.txt
  DESTINATION docs
  COMPONENT Documentation)

# Portable distribution uses CPack ZIP. The normal Setup.exe is created by
# scripts/package-windows.ps1 using installer/SimpleInstaller.nsi, not CPack NSIS.
set(CPACK_GENERATOR "ZIP")
set(CPACK_PACKAGE_NAME "ProTanki Editor PRO")
set(CPACK_PACKAGE_VENDOR "ProTanki Editor PRO Project")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "GPU-accelerated ProTanki legacy map editor")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_FILE_NAME "ProTankiEditorPRO-0.5.28-Portable")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "ProTanki Editor PRO")

include(CTest)
if(BUILD_TESTING)
  add_executable(LibraryReloadRegression
    tests/library_reload_regression.cpp
    src/AssetRegistry.cpp src/AssetRegistry.h
    src/Logger.cpp src/Logger.h)
  target_include_directories(LibraryReloadRegression PRIVATE src)
  target_link_libraries(LibraryReloadRegression PRIVATE pugixml::pugixml shell32 ole32 dbghelp)
  target_compile_definitions(LibraryReloadRegression PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
  if(MSVC)
    target_compile_options(LibraryReloadRegression PRIVATE /W4 /permissive- /utf-8)
  endif()
  add_test(NAME TransactionalLibraryReload COMMAND LibraryReloadRegression)
  add_executable(Native3DSWriterRegression tests/native_3ds_writer_regression.cpp src/Native3DSWriter.h src/NativeCollisionImport.h)
  target_include_directories(Native3DSWriterRegression PRIVATE src)
  add_test(NAME NativeEditedObject3DSWriter COMMAND Native3DSWriterRegression)
  add_executable(NativeVisualMetadataRegression tests/native_visual_metadata_regression.cpp
    src/Native3DSWriter.h src/Native3DSVisualMetadata.h)
  target_include_directories(NativeVisualMetadataRegression PRIVATE src)
  add_test(NAME Native3DSMaterialSmoothingRoundTrip COMMAND NativeVisualMetadataRegression)
  add_executable(NativeTerrainDeltaRegression tests/native_terrain_delta_regression.cpp
    src/NativeTerrainDelta.h src/Native3DSWriter.h src/NativeTaraWriter.h src/NativeCollisionImport.h)
  target_include_directories(NativeTerrainDeltaRegression PRIVATE src)
  add_test(NAME NativeTerrainDelta136GameVerified COMMAND NativeTerrainDeltaRegression
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/native_helpers")
  add_executable(NativeObjectExportIntegration
    tests/native_object_export_integration.cpp
    src/AssetRegistry.cpp src/Logger.cpp)
  target_include_directories(NativeObjectExportIntegration PRIVATE src tests)
  target_link_libraries(NativeObjectExportIntegration PRIVATE pugixml::pugixml assimp::assimp shell32 ole32 dbghelp)
  target_compile_definitions(NativeObjectExportIntegration PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
  add_test(NAME NativeLand01EndToEndExport COMMAND NativeObjectExportIntegration
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/native_helpers")
  add_executable(BrowseCachePolicyRegression tests/browse_cache_policy_regression.cpp src/BrowseCachePolicy.h)
  target_include_directories(BrowseCachePolicyRegression PRIVATE src tests)
  add_test(NAME BrowseSessionCacheByteBudgets COMMAND BrowseCachePolicyRegression)
  add_executable(AxOnlyUsedWheelRegression tests/ax_only_used_wheel_regression.cpp src/AxRecentFilter.h)
  target_include_directories(AxOnlyUsedWheelRegression PRIVATE src tests)
  add_test(NAME AxOnlyUsedFiltersTabWheel COMMAND AxOnlyUsedWheelRegression)
  add_executable(NativeExportCollisionPolicyRegression tests/native_export_collision_policy_regression.cpp src/NativeExportCollisionPolicy.h)
  target_include_directories(NativeExportCollisionPolicyRegression PRIVATE src tests)
  add_test(NAME NativeExportCollisionSafety COMMAND NativeExportCollisionPolicyRegression)
  add_executable(ObjectDraftRegression tests/object_draft_regression.cpp src/ObjectDraft.h)
  target_include_directories(ObjectDraftRegression PRIVATE src)
  add_test(NAME IsolatedObjectDraftRoundTrip COMMAND ObjectDraftRegression)
  add_executable(GuidanceStateRegression tests/guidance_state_regression.cpp src/GuidanceState.h)
  target_include_directories(GuidanceStateRegression PRIVATE src)
  add_test(NAME FirstRunAndIndependentPaths COMMAND GuidanceStateRegression)
  add_executable(ImportRegression tests/import_regression.cpp)
  target_include_directories(ImportRegression PRIVATE src)
  target_link_libraries(ImportRegression PRIVATE assimp::assimp)
  add_test(NAME LegacyImportRegression COMMAND ImportRegression "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")

  add_executable(MapEditRegression
    tests/map_edit_regression.cpp
    src/MapDocument.cpp src/MapDocument.h
    src/Logger.cpp src/Logger.h)
  target_include_directories(MapEditRegression PRIVATE src)
  target_link_libraries(MapEditRegression PRIVATE pugixml::pugixml shell32 ole32 dbghelp)
  target_compile_definitions(MapEditRegression PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
  if(MSVC)
    target_compile_options(MapEditRegression PRIVATE /W4 /permissive- /utf-8)
  endif()
  add_test(NAME LegacyMapEditRegression COMMAND MapEditRegression)

  add_executable(LegacyFidelityRegression
    tests/legacy_fidelity_regression.cpp
    src/MapDocument.cpp src/MapDocument.h
    src/Logger.cpp src/Logger.h)
  target_include_directories(LegacyFidelityRegression PRIVATE src)
  target_link_libraries(LegacyFidelityRegression PRIVATE pugixml::pugixml shell32 ole32 dbghelp)
  target_compile_definitions(LegacyFidelityRegression PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
  if(MSVC)
    target_compile_options(LegacyFidelityRegression PRIVATE /W4 /permissive- /utf-8)
  endif()
  add_test(NAME OriginalMetadataAndClipboardRoundTrip COMMAND LegacyFidelityRegression)

  add_executable(FunctionalMapRegression
    tests/functional_map_regression.cpp
    src/MapDocument.cpp src/MapDocument.h
    src/EditHistory.cpp src/EditHistory.h
    src/Logger.cpp src/Logger.h)
  target_include_directories(FunctionalMapRegression PRIVATE src)
  target_link_libraries(FunctionalMapRegression PRIVATE pugixml::pugixml shell32 ole32 dbghelp)
  target_compile_definitions(FunctionalMapRegression PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
  if(MSVC)
    target_compile_options(FunctionalMapRegression PRIVATE /W4 /permissive- /utf-8)
  endif()
  add_test(NAME FunctionalXMLAndGroupHistory COMMAND FunctionalMapRegression)

  add_executable(CollisionDeleteRegression
    tests/collision_delete_regression.cpp
    src/MapDocument.cpp src/MapDocument.h
    src/EditHistory.cpp src/EditHistory.h
    src/Logger.cpp src/Logger.h)
  target_include_directories(CollisionDeleteRegression PRIVATE src)
  target_link_libraries(CollisionDeleteRegression PRIVATE pugixml::pugixml shell32 ole32 dbghelp)
  target_compile_definitions(CollisionDeleteRegression PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
  if(MSVC)
    target_compile_options(CollisionDeleteRegression PRIVATE /W4 /permissive- /utf-8)
  endif()
  add_test(NAME CollisionDeleteXMLAndHistory COMMAND CollisionDeleteRegression)

  add_executable(NativeCollisionAuthoringRegression
    tests/native_collision_authoring_regression.cpp
    src/MapDocument.cpp src/MapDocument.h src/VerifiedCollisionTemplates.h
    src/Logger.cpp src/Logger.h)
  target_include_directories(NativeCollisionAuthoringRegression PRIVATE src)
  target_link_libraries(NativeCollisionAuthoringRegression PRIVATE pugixml::pugixml shell32 ole32 dbghelp)
  target_compile_definitions(NativeCollisionAuthoringRegression PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
  if(MSVC)
    target_compile_options(NativeCollisionAuthoringRegression PRIVATE /W4 /permissive- /utf-8)
  endif()
  add_test(NAME NativeCollisionAuthoring COMMAND NativeCollisionAuthoringRegression
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/beach_wall_end2_reference.xml")


  add_executable(VerifiedGroundCollisionRegression
    tests/verified_ground_regression.cpp
    src/MapDocument.cpp src/MapDocument.h src/VerifiedGroundCollision.h
    src/Logger.cpp src/Logger.h)
  target_include_directories(VerifiedGroundCollisionRegression PRIVATE src)
  target_link_libraries(VerifiedGroundCollisionRegression PRIVATE pugixml::pugixml shell32 ole32 dbghelp)
  target_compile_definitions(VerifiedGroundCollisionRegression PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
  if(MSVC)
    target_compile_options(VerifiedGroundCollisionRegression PRIVATE /W4 /permissive- /utf-8)
  endif()
  add_test(NAME SourceVerifiedFogtownGroundAndRoundTrip COMMAND VerifiedGroundCollisionRegression
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/fogtown_ground"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/fogtown_ground/original_fogtown_ground_six.xml"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/fogtown_ground/TEST_MAP_AAA-original-repro.xml")

  add_executable(EsplanadeCopyRegression
    tests/esplanade_copy_regression.cpp
    src/MapDocument.cpp src/MapDocument.h src/NativeCollisionImport.h
    src/Logger.cpp src/Logger.h)
  target_include_directories(EsplanadeCopyRegression PRIVATE src)
  target_link_libraries(EsplanadeCopyRegression PRIVATE pugixml::pugixml shell32 ole32 dbghelp)
  target_compile_definitions(EsplanadeCopyRegression PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
  if(MSVC)
    target_compile_options(EsplanadeCopyRegression PRIVATE /W4 /permissive- /utf-8)
  endif()
  add_test(NAME EsplanadeSourceDuplicateAndNamedBoxAnchors COMMAND EsplanadeCopyRegression
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/esplanade")

  add_executable(LosslessNativeCloneRegression
    tests/lossless_native_clone_regression.cpp
    src/MapDocument.cpp src/MapDocument.h
    src/Logger.cpp src/Logger.h)
  target_include_directories(LosslessNativeCloneRegression PRIVATE src)
  target_link_libraries(LosslessNativeCloneRegression PRIVATE pugixml::pugixml shell32 ole32 dbghelp)
  target_compile_definitions(LosslessNativeCloneRegression PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
  if(MSVC)
    target_compile_options(LosslessNativeCloneRegression PRIVATE /W4 /permissive- /utf-8)
  endif()
  add_test(NAME LosslessFullStaticMapClone COMMAND LosslessNativeCloneRegression
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/lossless_clone")

  add_executable(GeometryEdgeSnapRegression tests/geometry_snap_regression.cpp src/GeometrySnap.h)
  target_include_directories(GeometryEdgeSnapRegression PRIVATE src)
  add_test(NAME GeometryEdgesClearanceAndFloor COMMAND GeometryEdgeSnapRegression)

  add_executable(Native3DSHelperRegression
    tests/native_3ds_helper_regression.cpp
    src/MapDocument.cpp src/MapDocument.h src/NativeCollisionImport.h
    src/Logger.cpp src/Logger.h)
  target_include_directories(Native3DSHelperRegression PRIVATE src)
  target_link_libraries(Native3DSHelperRegression PRIVATE pugixml::pugixml shell32 ole32 dbghelp)
  target_compile_definitions(Native3DSHelperRegression PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
  if(MSVC)
    target_compile_options(Native3DSHelperRegression PRIVATE /W4 /permissive- /utf-8)
  endif()
  add_test(NAME Original3DSHelpersNativeXMLRoundTrip COMMAND Native3DSHelperRegression
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/native_helpers")

  add_executable(CollisionPhase2Regression
    tests/collision_phase2_regression.cpp
    src/MapDocument.cpp src/MapDocument.h
    src/EditHistory.cpp src/EditHistory.h
    src/Logger.cpp src/Logger.h)
  target_include_directories(CollisionPhase2Regression PRIVATE src)
  target_link_libraries(CollisionPhase2Regression PRIVATE pugixml::pugixml shell32 ole32 dbghelp)
  target_compile_definitions(CollisionPhase2Regression PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
  if(MSVC)
    target_compile_options(CollisionPhase2Regression PRIVATE /W4 /permissive- /utf-8)
  endif()
  add_test(NAME SharedCollisionAndXMLOrdering COMMAND CollisionPhase2Regression)

  add_executable(CollisionPreviewRegression
    tests/collision_preview_regression.cpp
    src/MapDocument.cpp src/MapDocument.h
    src/Logger.cpp src/Logger.h)
  target_include_directories(CollisionPreviewRegression PRIVATE src)
  target_link_libraries(CollisionPreviewRegression PRIVATE pugixml::pugixml shell32 ole32 dbghelp)
  target_compile_definitions(CollisionPreviewRegression PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
  if(MSVC)
    target_compile_options(CollisionPreviewRegression PRIVATE /W4 /permissive- /utf-8)
  endif()
  add_test(NAME NativeCollisionPreview COMMAND CollisionPreviewRegression)

  # Native Fogtown/NY XML lighting regression; original light metadata must survive.
  add_executable(NativeLightRegression
    tests/native_light_regression.cpp
    src/MapDocument.cpp src/MapDocument.h
    src/EditHistory.cpp src/EditHistory.h
    src/Logger.cpp src/Logger.h)
  target_include_directories(NativeLightRegression PRIVATE src)
  target_link_libraries(NativeLightRegression PRIVATE pugixml::pugixml shell32 ole32 dbghelp)
  target_compile_definitions(NativeLightRegression PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
  if(MSVC)
    target_compile_options(NativeLightRegression PRIVATE /W4 /permissive- /utf-8)
  endif()
  add_test(NAME NativeLightXMLRoundTrip COMMAND NativeLightRegression)

  # Independently compilable native gameplay choices and eight-heading rotation.
  add_executable(GameplayAuthoringRegression tests/gameplay_authoring_regression.cpp src/GameplayAuthoring.h)
  target_include_directories(GameplayAuthoringRegression PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  add_test(NAME ExplicitBonusModesAndEightHeadings COMMAND GameplayAuthoringRegression)

  # Regression for the displayed movement grid: snap must not silently disable.
  add_executable(GridStepRegression tests/grid_step_regression.cpp src/GridStep.h)
  target_include_directories(GridStepRegression PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  add_test(NAME GridStepAlwaysOn COMMAND GridStepRegression)

  # Lightweight projected gameplay picking regression (portable CPU-only).
  add_executable(VolumePickingRegression tests/volume_picking_regression.cpp)
  target_include_directories(VolumePickingRegression PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  add_test(NAME ProjectedGameplayVolumePicking COMMAND VolumePickingRegression)
endif()
include(CPack)
