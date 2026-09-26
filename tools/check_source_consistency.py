#!/usr/bin/env python3
"""Cheap preflight for incomplete GitHub uploads and missing C++ implementations.

Not a replacement for the Windows compiler or D3D runtime test. It catches the
historical error where a copy/paste upload truncated EditorUi.cpp around 150
lines, plus missing entry points, absent CMake sources and version drift.
"""
from __future__ import annotations
from pathlib import Path
import re
import sys

ROOT=Path(__file__).resolve().parent.parent

def text(path: str) -> str:
    return (ROOT/path).read_text(encoding='utf-8')

def require(ok: bool, message: str) -> None:
    if not ok: raise ValueError(message)

def brace_check(src: str, *, strict_literals: bool = False) -> bool:
    # Balance meaningful delimiters while skipping comments/quoted strings.
    stack=[]; i=0; mode='normal'; pairs={')':'(',']':'[','}':'{'}
    while i<len(src):
        c=src[i]; nxt=src[i+1:i+2]
        if mode=='normal':
            if c=='/' and nxt=='/':mode='line';i+=1
            elif c=='/' and nxt=='*':mode='block';i+=1
            elif c=='"':mode='str'
            elif c=="'":mode='char'
            elif c in '([{':stack.append(c)
            elif c in ')]}':
                if not stack or stack.pop()!=pairs[c]:return False
        elif mode=='line' and c=='\n':mode='normal'
        elif mode=='block' and c=='*' and nxt=='/':mode='normal';i+=1
        elif mode in ('str','char'):
            if strict_literals and c in '\r\n': return False  # C++ normal string/char literals cannot contain raw newlines.
            if c=='\\':i+=1
            elif c==('"' if mode=='str' else "'"):mode='normal'
        i+=1
    return not stack and mode in ('normal','line')

def main() -> int:
    try:
        ui=text('src/EditorUi.cpp'); hdr=text('src/EditorUi.h')
        scene=text('src/SceneRenderer.cpp'); scene_hdr=text('src/SceneRenderer.h')
        doc=text('src/MapDocument.cpp'); cmake=text('CMakeLists.txt')
        require(len(ui.splitlines())>=950, f'EditorUi.cpp truncated ({len(ui.splitlines())} lines)')
        # Duplicate Object Editor declarations previously passed source preflight
        # but failed MSVC with C2535 (EditorUi.h lines 109-116).
        for method in ('PushObjectUndo', 'UndoObject', 'RedoObject', 'ResetObjectHistory'):
            declarations = re.findall(r'^\s*void\s+' + method + r'\s*\(\s*\)\s*;', hdr, re.MULTILINE)
            require(len(declarations)==1,
                    f'src/EditorUi.h: {method}: expected 1 declaration, found {len(declarations)}')
        require('#include <shellapi.h>' in ui and 'ShellExecuteW' in ui, 'Discord URL launcher needs the proper Win32 shell header')
        needed=('Draw','DrawMenu','DrawToolbar','DrawScene','DrawLibrary','DrawProperties',
                'DrawViewport','DrawGameplay','DrawControlHelp','HandleEditorShortcuts',
                'DefaultBinding','LoadControls','SaveControls')
        for name in needed:
            count=len(re.findall(r'\bEditorUi::'+name+r'\s*\(',ui))
            require(count==1, f'EditorUi::{name}: expected 1 definition, found {count}')
        require(ui.count('case A::')==31,'Expected 31 default keyboard mappings')
        for name in ('src/EditorUi.cpp','src/SceneRenderer.cpp','src/MapDocument.cpp'):
            require(brace_check(text(name)),f'{name}: unbalanced source delimiters')
        require('unsigned overlayMask = 31u, unsigned modeMask = 15u' in scene_hdr, 'Overlay API mismatch in header')
        require('RenderDebugOverlay(showBounds, showGameplay, showZones, overlayMask, modeMask, showLights)' in scene,'Overlay API mismatch in implementation')
        require('map.child("spawn-points")' in doc and 'map.child("dom-keypoints")' in doc,'Missing gameplay parser')
        require('src/EditorUi.cpp src/EditorUi.h' in cmake,'CMake does not compile the full EditorUi.cpp')
        # Win32 headers may macro-expand `near`/`far`, so do not use them as local names.
        require(not re.search(r'\b(?:const\s+)?bool\s+(?:near|far)\s*=', ui),
                'Windows near/far macro collision in EditorUi.cpp')
        # The 0.5.18 native collider exporter must be compiled and tested against
        # an independent original ProTLVK XML fixture, not just a self-made map.
        require('src/VerifiedCollisionTemplates.h' in cmake and
                'NativeCollisionAuthoringRegression' in cmake and
                'beach_wall_end2_reference.xml' in cmake and
                (ROOT/'tests/fixtures/beach_wall_end2_reference.xml').is_file(),
                'Verified Wall End 2 collision source/CTest/fixture incomplete')
        require('BindVerifiedCollisionOwners()' in doc and
                'sameRotation(c.rotation,templ.rotation,p.rotation.z)' in doc and
                'v.authoredOwnerIndex=static_cast<int>(index)' in doc and
                'else if(t->transformDirty)' in doc and
                't.legacySourceIndex<0' in doc and
                'map.AddVerifiedCollisionForProp(index)' in ui and
                'AuthorCollisionForPlacement(map,assets,index,info)' in ui and
                'map.AddImportedCollisionForProp(index,imported)' in ui and
                'Repair saved wall collision' in ui and
                'HasVerifiedCollisionForProp' in doc,
                'Verified original native collider ownership/transform/export path missing')
        require('map_.CreateBlank("1.0.Light", false);' in text('src/App.cpp') and
                'ImGui::BeginPopupModal("Confirm object draft save",nullptr,ImGuiWindowFlags_AlwaysAutoResize)' in ui,
                'Clean startup map or centered draft confirmation missing')
        require('VERSION 0.5.20' in cmake and '0.5.20' in text('src/App.cpp')
                and '0.5.20-native-frame-transforms' in text('src/Logger.cpp'),'Version drift')
        asset_header=text('src/AssetRegistry.h')
        draft=text('src/ObjectDraft.h')
        require('originalLibraryXml' in asset_header and 'originalPropXml' in asset_header and
                'originalPropXml' in text('src/MapDocument.h') and
                'geometry.append_copy(source)' in doc and
                'allowOpaqueMetadataCopy' in ui and
                'library-source.xml' in draft and
                'library-prop-template.xml' in draft,
                'Complete native object snapshot / explicit opaque copy / draft template support missing')
        require('LegacyFidelityRegression' in cmake and 'OriginalMetadataAndClipboardRoundTrip' in cmake and
                (ROOT/'tests/legacy_fidelity_regression.cpp').is_file() and
                'Native3DSHelperRegression' in cmake and
                'Original3DSHelpersNativeXMLRoundTrip' in cmake and
                'src/NativeCollisionImport.h' in cmake and
                (ROOT/'tests/fixtures/native_helpers/wall_e1.3ds').is_file() and
                (ROOT/'tests/fixtures/native_helpers/hs_part6.3ds').is_file() and
                (ROOT/'tests/fixtures/native_helpers/brid_1.3ds').is_file() and
                (ROOT/'tests/fixtures/native_helpers/concrete_wall_end1_original_map.xml').is_file() and
                'BindImportedCollisionForProp' in doc and
                'AuthorCollisionForPlacement(map,assets,index,info)' in ui and
                'Allow visual-only props' in ui and
                'inspectedHelpers=NativeCollisionImport::Read(inspectedSource)' in ui and
                'Placement settings##tools' in ui and 'HoverHelp(' in ui,
                'Shared 3DS helper/Obj. Editor authoring and native roundtrip tests incomplete')
        require('const auto toLocal=' in text('src/NativeCollisionImport.h') and
                'out.rotation=Euler(bx,by,bz)' in text('src/NativeCollisionImport.h') and
                'sameBoxCorners' in doc and
                'NativeCollisionImport::NoNativeHelpersError' in ui and
                all((ROOT/'tests/fixtures/native_helpers'/f).is_file() for f in (
                    'waffle_wall_1.3ds', 'waffle_wall1_original_map.xml',
                    'promotion_bilboard.3ds', 'billboard_original_map.xml',
                    'invalid_scaled_visual_frame.3ds')) and
                (ROOT/'tests/test_native_frames_0520_audit.py').is_file() and
                'RELEASE_NOTES_0520.md' in cmake and 'BUILD_STATUS_0520.md' in cmake,
                '0.5.20 rotated original 3DS frame support, safety guards or fixtures incomplete')
        require('float3 linearAlbedo = pow(max(albedo.rgb, 0.0), 2.2)' in scene and
                'clip(albedo.a - 0.5)' in scene and
                'if (mode > 5.5) return float4(albedo.rgb, 1.0)' in scene and
                'Unlit texture (diagnostic)' in ui and
                'mode > 6 ? 6 : mode' in text('src/SceneRenderer.h'),
                'Bridge 1 sRGB/cutout diagnostic rendering path missing')
        require('oneSidedPairedAtlas' in text('src/LegacyMeshImport.h') and
                'rasterizerPairedAtlas_' in scene and 'D3D11_CULL_FRONT' in scene and
                'oppositeFaceAtlas' in text('src/SceneRenderer.h'),
                'Bridge 1 paired-front/back texture draw fix absent')
        require('src/SplashScreen.cpp src/SplashScreen.h' in cmake
                and 'assets/startup-tank.png' in cmake and 'gdiplus' in cmake,'Native splash missing from build')
        splash=text('src/SplashScreen.cpp')
        require('FindResourceW(instance, MAKEINTRESOURCEW(201), RT_RCDATA)' in splash
                and 'g.DrawString(status.c_str()' in splash
                and splash.count('g.DrawString(status.c_str()')==1, 'Splash must show only one changing status line')
        require((ROOT/'assets/startup-tank.png').stat().st_size>100_000,'Original user splash art not included')
        require('SplashScreen::Open(instance);' in text('src/main.cpp')
                and 'splashCompleteAt_=GetTickCount64(); firstFrame_=false' in text('src/App.cpp')
                and 'SplashScreen::Close(); splashCompleteAt_=0' in text('src/App.cpp'), 'Splash lifecycle incomplete')
        for name in ('SelectOnly','UpdatePlacementGhost','CommitPlacement','StartClipboardPlacement',
                     'CaptureRecentThumbnail','ActivateRecent','DrawAxLibrary','BeginFunctionalPlacement',
                     'CommitFunctionalPlacement','MoveFunctional','DeleteFunctional'):
            require(len(re.findall(r'\bEditorUi::'+name+r'\s*\(',ui))==1,f'EditorUi::{name} missing or duplicated')
        for name in ('SetGhost','RenderGhost','SelectInScreenRect','CameraMoveBasisLegacy',
                     'BuildFunctionalPads','RenderFunctionalPads','SetFunctionalGhost','RenderFunctionalGhost'):
            require(len(re.findall(r'\bSceneRenderer::'+name+r'\s*\(',scene))==1,f'SceneRenderer::{name} missing or duplicated')
        require('src/EditHistory.cpp src/EditHistory.h' in cmake and 'FunctionalXMLAndGroupHistory' in cmake,
                'Functional XML/group regression missing')
        require('SetFlagPosition' in doc and 'AddSpecialBox' in doc and 'flagsDirty_' in doc,
                'Native gameplay serializer implementation missing')
        require(splash.index('#include <objidl.h>') < splash.index('#include <gdiplus.h>')
                and splash.index('#include <propidl.h>') < splash.index('#include <gdiplus.h>'),
                'GDI+ needs COM type headers before gdiplus.h')
        require('raw.As(&original)' in ui and 'raw.As(original.GetAddressOf())' not in ui,
                'WRL ComPtr::As should receive a ComPtr reference, not a raw GetAddressOf pointer')
        require(brace_check(text('src/SplashScreen.cpp')) and brace_check(text('src/EditHistory.cpp')),
                'Unbalanced splash/edit history source delimiters')
        require('src/TestSession.cpp' not in cmake and not (ROOT/'src/TestSession.cpp').exists(),
                'Obsolete native tester must be completely removed')
        require('LoadTestTank' not in scene and 'RenderTestTank' not in scene and 'PSTankPreview' not in scene,
                'Obsolete native tank renderer remains in code')
        require('LaunchExternalTester()' in text('src/App.cpp') and 'CreateProcessW(executable.c_str()' in ui
                and 'external-tester-path.txt' in ui and 'GetOpenFileNameW(&ofn)' in ui,
                'External tester launcher missing or path is not persisted')
        require('DrawObjectEditor(assets,scene)' in ui and 'object-draft.txt' in text('src/ObjectDraft.h'),
                'GLB object draft editor missing')
        # Phase 5 authoring safety: model preview and draft exporter are isolated.
        require('ASSIMP_BUILD_GLTF_IMPORTER ON' in cmake and
                'DraftMeshImport::Load(file)' in scene and
                'objectScene_.BuildAssetPreview' in ui and
                'ObjectDraft::SaveNew' in ui and
                'IsolatedObjectDraftRoundTrip' in cmake and
                'native_export false' in text('src/ObjectDraft.h'),
                'Isolated 3D object drafting or its non-native export guard is missing')
        require('SelectAllStaticProps(map,scene)' in ui and 'ImGuiKey_A,false' in ui and
                'original.bak' in doc, 'Select All or original map backup guard is missing')
        require('CollisionPhase2Regression' in cmake and 'SharedCollisionAndXMLOrdering' in cmake and
                'tests/collision_phase2_regression.cpp' in cmake,
                'Phase 2 shared collision/source-index ordering CTest is not registered')
        require('size_t sharedOrigins=0' in doc and 'triangles[triangleIndex++]' in doc,
                'Shared-collision preservation or in-place XML serialization missing')
        require('DeletePropWithCollision' in doc and 'ExactCollidersAt' in doc and
                'CollisionDeleteXMLAndHistory' in cmake and (ROOT/'tests/collision_delete_regression.cpp').exists(),
                'Collision lifecycle and regression test missing')
        require('ProcessPendingOperations(); // Previous frame' in text('src/App.cpp'), 'Scene transition must be deferred to before next ImGui frame')
        require('functionalModelBatches_' in scene and 'BuildFunctionalModels(map)' in scene, 'Functional 3DS model path missing')
        for asset in ('ctf_red/object.3ds','ctf_red/fs_red.jpg','ctf_blue/object.3ds','ctf_blue/fs_blue.jpg','cp/object.3ds','cp/pedestal.jpg'):
            require((ROOT/'assets/functional'/asset).stat().st_size>100,'Missing user-supplied functional model: '+asset)
        require('0.30' in ui and 'ImGuiConfigFlags_NavEnableKeyboard' not in text('src/App.cpp'), 'Simple select/AX keyboard isolation absent')
        require('DrawBrowseLibrary(map, assets, scene, previewScene)' in ui and 'ImGuiListClipper clipper' in ui
                and 'ImGuiTreeNodeFlags_SpanAvailWidth' in ui and 'constexpr size_t budget=96' in ui
                and 'browseRenderedThisFrame_' in ui, 'Lazy full-workspace library browser missing')
        require('SnapDelta(current.x-drag_.planeStart.x)' in ui and 'SnapDelta(current.y-drag_.planeStart.y)' in ui,
                'Existing props must snap relative movement, not absolute XML positions')
        require('PushSnapshot' in text('src/EditHistory.cpp') and 'DrawUnsavedChangesDialog' in text('src/App.cpp')
                and 'CreateBlank' in doc, 'Structural Undo / New Map / unsaved changes guard missing')
        require('ImGuiKey_Keypad0' in ui and 'ImGuiKey_Keypad5' in ui,
                'Numpad grid bindings are missing')
        require('structuralHistory.PushSnapshot' in text('tests/functional_map_regression.cpp') and
                'blank.SaveLegacyAs' in text('tests/functional_map_regression.cpp'),
                'Structural Undo/Redo and blank map CTest regression missing')
        require('assets/GTanksNextEditor.ico' in cmake and '101 ICON "GTanksNextEditor.ico"' in text('assets/GTanksNextEditor.rc'), 'Windows icon must be embedded as a resource')
        require((ROOT/'assets/GTanksNextEditor.ico').stat().st_size>30000, 'Windows multi-size icon is missing')
        require('CPACK_GENERATOR "ZIP"' in cmake and 'SimpleInstaller.nsi' in text('scripts/package-windows.ps1') and
                'MUI_PAGE_STARTMENU' not in text('installer/SimpleInstaller.nsi'), 'Simple NSIS packaging / no Start-menu folder selection')
        require('BrowseKind' in ui and 'browseRenderedThisFrame_' in ui, 'Lazy browser/type filter incomplete')
        # Packaging contract: the two workflows must invoke the custom NSIS script,
        # not the stale CPack NSIS route with the previous GTanksNextEditor-Setup names.
        workflow_paths=(
            '.github/workflows/build-windows-installer.yml',
            '.github/workflows/release-windows.yml',
        )
        packaging=text('scripts/package-windows.ps1')
        for workflow_path in workflow_paths:
            workflow=text(workflow_path)
            require(r'.\scripts\package-windows.ps1 -BuildDir build' in workflow,
                    f'{workflow_path}: custom NSIS packaging invocation missing')
            require('cpack -C Release -G NSIS' not in workflow and 'GTanksNextEditor-Setup' not in workflow,
                    f'{workflow_path}: stale installer packaging route')
            require('ProTankiEditorPRO-0.5.20-Setup.exe' in workflow and
                    'ProTankiEditorPRO-0.5.20-Portable.zip' in workflow and
                    'ProTankiEditorPRO-0.5.6-' not in workflow,
                    f'{workflow_path}: release filename mismatch; replace this file with the 0.5.20 workflow')
        require('SimpleInstaller.nsi' in packaging and
                'ProTankiEditorPRO-0.5.20-Setup.exe' in packaging and
                'ProTankiEditorPRO-0.5.20-Portable.zip' in packaging and
                'ProTankiEditorPRO-0.5.6-' not in packaging,
                'scripts/package-windows.ps1: output filename mismatch; replace with 0.5.18 packaging script')
        require('0.5.20' in text('assets/GTanksNextEditor.rc') and
                'FILEVERSION 0,5,20,0' in text('assets/GTanksNextEditor.rc'), 'Resource version drift')
        require('GridStep::Quantize(v,gridSize_)' in ui and 'GridStep::KeyboardStep(gridSize_,io.KeyShift)' in ui
                and 'GridStepAlwaysOn' in cmake, 'Movement grid regression: grid must snap by default')
        require('objectUndo_.push_back(CaptureObjectSnapshot())' in ui and 'if (objectEditorOpen_)' in ui,
                'Object authoring undo must remain separate from the map')
        require('NativeLightPreview : register(b1)' in scene and 'scene.SetNativeLights(map.Lights())' in ui,
                'Native light preview shader / per-frame data binding absent')
        require('viewportBackground_' in ui and 'Custom editor theme' in ui,
                'Custom preview background/theme settings missing')
        require('context_->PSSetConstantBuffers(0, 1, &pixelCamera)' in scene,
                'Effects must bind lighting constants to the pixel shader')
        require('CreateShortcut "$DESKTOP\\ProTanki Editor PRO.lnk"' in text('installer/SimpleInstaller.nsi'),
                'The Desktop shortcut was removed')
        installer=text('installer/SimpleInstaller.nsi')
        require('MUI_FINISHPAGE_RUN' in installer and 'MUI_FINISHPAGE_RUN_TEXT' in installer,
                'Installer finish page does not launch the editor')
        require('DrawFunctionalProperties' in ui and 'functionalPropertyBefore_' in hdr and
                'MoveFunctional(map,at)' in ui and 'DeletePropsWithCollision' in ui,
                'Editing / collision safety checkpoint is incomplete')
        require('Zone editing is not just a viewport visual' in text('tests/functional_map_regression.cpp'),
                'Zone XML/undo regression missing')
        require('std::shared_ptr<const std::string> sourceXml_' in text('src/MapDocument.h') and
                'sourceXml_ = std::make_shared<const std::string>(std::move(xml));' in doc and
                'PushZone' in text('src/EditHistory.cpp') and
                'zoneHistory.PushZone' in text('tests/functional_map_regression.cpp'),
                'Large-map source sharing / compact zone Undo is incomplete')
        require('NativeLightRegression' in cmake and
                'bool MapDocument::SetLight' in doc and 'bool MapDocument::DeleteLight' in doc and
                'childCount(verifyMap.child("lights"), "light") == stats_.lights' in doc and
                'void EditorUi::DrawLighting' in ui and 'showLights_' in ui and
                'const auto& light:map.Lights()' in scene and
                'NativeLightXMLRoundTrip' in cmake,
                'Native light XML editing/preview regression')
        # 0.5.18 copies must retain source-grid phase and native gameplay properties.
        require('QuantizeAroundAnchor' in text('src/GridStep.h') and
                ui.count('GridStep::QuantizeAroundAnchor(')>=4 and
                'functionalClipboardBonus_' in hdr and 'item.modes' not in ui and
                'auto item=functionalClipboardBonus_' in ui and 'map.AddBonusRegion(item)' in ui,
                'Gameplay/static paste needs source-aligned native clipboard')
        require('SetGridColor' in scene_hdr and 'void SceneRenderer::SetGridColor(' in scene and
                'viewportGridColor ' in ui and 'Grid line RGB' in ui,
                'Persistent runtime grid color missing')
        require('Save object changes?##objclose' in ui and
                'if(!windowOpen) {if(objectDirty_)objectCloseRequested_=true;else objectEditorOpen_=false;}' in ui and
                'bool EditorUi::SaveObjectDraft(' in ui and
                'ImGui::TextUnformatted("Unsaved map changes")' not in text('src/App.cpp'),
                'Object dirty close or duplicate unsaved modal title')
        require('smoothCameraFocus ' in ui and 'SceneRenderer::AdvanceCameraFocus(' in scene and
                'Fullscreen (F11)' in ui and 'ui_.FirstRun()?SW_SHOWMAXIMIZED:show' in text('src/App.cpp'),
                'Camera preference, fullscreen label, or first-run maximization missing')
        require('crystal_500' in text('src/GameplayAuthoring.h') and '"as"' in text('src/GameplayAuthoring.h'),
                'Original map bonus tokens not represented')
        # 0.5.18 gameplay authoring: never silently re-enable DM when unchecked.
        authoring=text('src/GameplayAuthoring.h')
        require('GameplayAuthoringRegression' in cmake and
                'ExplicitBonusModesAndEightHeadings' in cmake and
                'GameplayAuthoring::RotateSpawn' in ui and
                'GameplayAuthoring::HasExplicitModes(newBonusModes_)' in ui and
                'GameplayAuthoring::SelectedModes(newBonusModes_)' in ui and
                'p.free=newBonusFree_' in ui and 'p.parachute=newBonusParachute_' in ui and
                'if(p.modes.empty())p.modes.push_back("dm")' not in ui and
                'SpawnTurnRadians = 0.78539816339744830962f' in authoring,
                '0.5.18 explicit game-mode or 45-degree spawn regression')
        # 0.5.18 gameplay selection and isolated mesh-authoring contract.
        require('VolumePicking::Score' in ui and '#include "VolumePicking.h"' in ui and
                'VolumePickingRegression' in cmake and
                'selected?3.4f:2.5f' in ui and 'browseThumbScale' in ui,
                'Projected gameplay volume selection / thumbnail controls missing')
        require('SceneRenderer::ProjectWorld' in scene and
                'SceneRenderer::ScreenToWorldViewPlane' in scene and
                'SceneRenderer::UpdatePreviewMeshGeometry' in scene and
                'objectScene_.ProjectWorld' in ui and
                'mesh_vertices' in text('src/ObjectDraft.h') and
                'mesh_indices' in text('src/ObjectDraft.h') and
                'bool objectShowBoxes_{}' in hdr,
                'Aligned editable mesh authoring or optional collision overlay missing')
        require('newSpawnYaw_' in ui and 'GameplayAuthoring::RotateSpawn(spawn.rotationZ,io.KeyShift)' in ui and
                '"parachute",region.parachute' in doc and
                '"free",region.free' in doc,
                'Spawn direction or native bonus attributes not preserved')
        # 0.5.18 work-in-progress safeguards. Diagnostic rendering must be read-only:
        # do not regress into a visual-only colored prop display or silently omit
        # collision triangles (the source of the reported invisible-floor issue).
        preview=text('src/CollisionPreview.h')
        preview_test=text('tests/collision_preview_regression.cpp')
        require('CollisionPreview::Build(map)' in scene and
                'RenderCollisionGeometry()' in scene and
                'CollisionPreviewRegression' in cmake and
                'NativeCollisionPreview' in cmake,
                'Native collision diagnostic / CTest registration missing')
        require(preview.count('LegacyTransform::WorldFromLegacyLocal(')==3 and
                'WorldFromLegacyLocal' in text('src/LegacyTransform.h'),
                'Collision preview incorrectly treats XML Z-up vertices as internal Y-up vertices')
        require('map.CollisionPlanes()' in preview and
                'map.CollisionBoxes()' in preview and
                'map.CollisionTriangles()' in preview and
                'const MapDocument& map' in preview and
                'map.Dirty()' in preview_test,
                'Collision preview must use native XML and remain read-only')
        require('showGrid_ && !showCollision_' in ui and
                'showCollision_' in hdr and
                'bool collisionView = false' in scene_hdr,
                'Geometry view toggle / grid isolation incomplete')
        installer=text('installer/SimpleInstaller.nsi')
        require('WriteRegStr HKCU "${UNINSTALL_KEY}" "UninstallString"' in installer and
                'DeleteRegKey HKCU "${UNINSTALL_KEY}"' in installer,
                'Windows current-user uninstall registration missing')
        # Stage 4: a cancelled/invalid native picker must never be recorded as
        # successful, and a failed library scan must preserve the working index.
        guide=text('src/GuidanceState.h')
        assets=text('src/AssetRegistry.cpp')
        app=text('src/App.cpp')
        require('FirstRunAndIndependentPaths' in cmake and
                'GuidanceStateRegression' in cmake and
                'guidance_.RecordSuccess(GuideTarget::Map)' in ui and
                'guidance_.RecordSuccess(GuideTarget::Library)' in ui and
                'guidance_.RecordSuccess(GuideTarget::Tester)' in ui and
                'bool NeedsExplanation(GuideTarget target) const' in guide,
                'Independent first-use guidance and persistence missing')
        require('ProcessPendingNativeDialogs();' in app and
                'nativePickerRequest_=kind' in ui and
                'ui_.OnMapLoaded(map_.Path())' in app and
                'ui_.OnLibraryLoaded(previewScene_,assets_.Root())' in app,
                'Native picker must run between frames and only successful loads count')
        # Catch misspelled Logger API calls before the expensive MSVC build.
        logger_api=set(re.findall(
            r'^\s*(?:bool|void|std::string|const std::filesystem::path&)\s+(\w+)\s*\(',
            text('src/Logger.h'), re.MULTILINE))
        logger_calls=set()
        for source in list((ROOT/'src').glob('*.cpp'))+list((ROOT/'tests').glob('*.cpp')):
            logger_calls.update(re.findall(r'\bLog::(\w+)\s*\(',source.read_text(encoding='utf-8')))
        require(not (logger_calls-logger_api),
                'Undefined Log API call(s): '+', '.join(sorted(logger_calls-logger_api)))
        require(brace_check(text('tests/library_reload_regression.cpp'),strict_literals=True),
                'library_reload_regression.cpp: malformed string/char literal or delimiters')
        require('TransactionalLibraryReload' in cmake and
                'candidateAssets' in assets and
                'assets_=std::move(candidateAssets)' in assets and
                'tests/library_reload_regression.cpp' in cmake,
                'Bad library folders must not discard the existing library')
        require('ApplyPreferredTheme()' in app and 'uiTheme ' in ui,
                'Selected editor theme is not persisted')
        require('def inspect_3ds(' in text('tools/audit_native_library.py') and
                'native_export false' in text('src/ObjectDraft.h') and
                (ROOT/'docs/OBJECT_EXPORT_COMPATIBILITY_AUDIT.md').is_file(),
                'Phase 5B must keep native export gated and include a read-only 3DS audit')
        require('def compare(before: Path, after: Path)' in text('tools/audit_map_delta.py') and
                'test_map_delta_audit.py' in '\n'.join(p.name for p in (ROOT/'tests').iterdir()) and
                all('test_*audit.py' in text(path) for path in workflow_paths),
                'Read-only native map delta audit and workflow tests missing')
        print('PASS: 0.5.20 source preflight: external launcher, GLB draft editor, collision XML/undo tests, packaging and source structure.')
        return 0
    except (ValueError,OSError) as e:
        print(f'FAIL: {e}',file=sys.stderr)
        return 1

if __name__=='__main__':raise SystemExit(main())
