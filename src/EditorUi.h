#pragma once
#include "MapDocument.h"
#include "AssetRegistry.h"
#include "SceneRenderer.h"
#include "EditHistory.h"
#include "GuidanceState.h"
#include "ObjectDraft.h"
#include <filesystem>
#include <array>
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <wrl/client.h>

class EditorUi {
public:
    EditorUi();
    void Draw(MapDocument& map, AssetRegistry& assets, SceneRenderer& scene, SceneRenderer& previewScene);
    const std::filesystem::path& PendingMap() const { return pendingMap_; }
    const std::filesystem::path& PendingLibrary() const { return pendingLibrary_; }
    void ClearPendingMap() { pendingMap_.clear(); }
    void ClearPendingLibrary() { pendingLibrary_.clear(); }
    bool ConsumeNewMapRequest() { const bool request=newMapRequested_; newMapRequested_=false; return request; }
    // Used by application-level close / map-switch confirmation. False means save failed or was cancelled.
    bool SaveForTransition(MapDocument& map) { return Save(map, map.Path().empty()); }
    void OnNewMap() { OnMapLoaded(); }
    void SetMessage(std::string text, bool error = false) { message_ = std::move(text); messageError_ = error; }
    void OnMapLoaded(const std::filesystem::path& successfulMapPath = {});
    void OnLibraryLoaded(SceneRenderer& previewScene, const std::filesystem::path& successfulLibraryRoot = {});
    // Native dialogs run between ImGui frames, never while a popup is drawing.
    void ProcessPendingNativeDialogs();
    void ApplyPreferredTheme() const;
    bool FirstRun() const { return welcomePending_; }
    void ShutdownObjectPreview() { objectScene_.Shutdown(); objectSceneInitialized_=false; objectSceneHasModel_=false; }

    bool ConsumeFullscreenToggle() { const bool v = fullscreenToggleRequested_; fullscreenToggleRequested_ = false; return v; }
    bool ConsumeSceneRebuildRequest(bool& preserveCamera) {
        if (!sceneRebuildRequested_) return false;
        preserveCamera = preserveCameraOnRebuild_;
        sceneRebuildRequested_ = false;
        preserveCameraOnRebuild_ = false;
        return true;
    }
    void SetFullscreenState(bool value) { fullscreen_ = value; }
    bool CaptureNativeWheel(short delta); // called BEFORE ImGui backend handles WM_MOUSEWHEEL
    bool ConsumeTestRequest() { const bool v = testRequested_; testRequested_ = false; return v; }
    void SetTestFeedback(const std::string& text, bool error = false) { SetMessage(text, error); }
    bool LaunchExternalTester();

private:
    enum class ToolMode { Select, Move, Rotate };
    enum class FunctionalType { None, Flag, Spawn, Point, Bonus, Zone, Light };
    enum class FunctionalPlacement { None, RedFlag, BlueFlag, SpawnDm, SpawnRed, SpawnBlue, SpawnDomRed, SpawnDomBlue, ControlPoint, BonusRegion, KillZone, KickZone, Light };
    enum class NavigationMode { Legacy, Adobe, Simple, Custom };
    int uiTheme_{};
    std::array<float,3> customSurface_{0.13f,0.21f,0.29f};
    std::array<float,3> customText_{0.94f,0.96f,0.98f};
    float viewportBackground_[3]{0.055f,0.073f,0.10f};
    float viewportGridColor_[3]{0.52f,0.56f,0.60f};
    bool showCustomThemePopup_{};
    bool allowVisualOnlyPlacement_{}; // explicit opt-in; no silent pass-through walls
    bool allowOpaqueMetadataCopy_{}; // next-placement approval; auto-resets, never a gameplay property
    bool collisionBindingsPending_{true};
    bool showBackgroundPopup_{};
    bool smoothCameraFocus_{true};
    bool previewNativeLighting_{true};
    float previewLightReach_{100.0f};
    enum class Action {
        Open, Save, SaveAs, Undo, Redo, Select, Move, Rotate, Delete, Snap,
        Grid, Bounds, Gameplay, Zones, Frame, Place, Cancel,
        MoveXP, MoveXN, MoveYP, MoveYN, MoveZP, MoveZN, RotateP, RotateN,
        Grid100, Grid200, Grid300, Grid400, Grid500, Fullscreen, Count
    };
    struct KeyBinding { int key{}; bool ctrl{}, shift{}, alt{}; };
    enum class MouseGesture { Right, Middle, AltLeft, SpaceLeft, ShiftRight };
    struct NavigationSettings {
        float orbitSensitivity{1.0f};
        float panSensitivity{1.0f};
        float zoomSpeed{1.0f};
        bool invertOrbitY{};
        bool focusSelectionOnClick{};
    };
    struct DragState {
        bool active{};
        ToolMode tool{ToolMode::Select};
        int propIndex{-1};
        PropTransformState before{};
        DirectX::XMFLOAT3 planeStart{};
        float mouseStartX{};
        double pressedAt{};
        bool moved{};
    };

    void DrawMenu(MapDocument&, SceneRenderer&);
    void DrawToolbar(MapDocument&, SceneRenderer&);
    void DrawScene(MapDocument&, SceneRenderer&);
    void DrawLibrary(MapDocument&, const AssetRegistry&, SceneRenderer&, SceneRenderer& previewScene);
    void DrawBrowseLibrary(MapDocument&, const AssetRegistry&, SceneRenderer&, SceneRenderer&);
    void CaptureBrowseThumbnail(size_t assetIndex, const AssetRegistry&, SceneRenderer&);
    void DrawProperties(MapDocument&, const AssetRegistry&, SceneRenderer&);
    void DrawFunctionalProperties(MapDocument&, SceneRenderer&);
    void DrawViewport(MapDocument&, const AssetRegistry&, SceneRenderer&);
    void DrawStatus(const MapDocument&, const AssetRegistry&, const SceneRenderer&);
    void DrawToast();
    static const char* EffectName(int mode);
    void DrawControlHelp();
    void DrawSupportPopup();
    void DrawFirstRunGuidance();
    void RequestGuidedOpen(int kind);
    void CompleteGuidedOpen(int kind);
    void DrawGameplay(MapDocument&, SceneRenderer&);
    void DrawLighting(MapDocument&, SceneRenderer&);
    void DrawObjectEditor(const AssetRegistry&, SceneRenderer&);
    bool SaveObjectDraft(const AssetRegistry&);
    void PushObjectUndo();
    void UndoObject();
    void RedoObject();
    void ResetObjectHistory();
    void DrawAxLibrary(const MapDocument&, const AssetRegistry&, SceneRenderer&, SceneRenderer&);
    void SelectOnly(int index, SceneRenderer& scene);
    void UpdatePlacementGhost(SceneRenderer&, const AssetRegistry&, float x, float y);
    void CommitPlacement(MapDocument&, const AssetRegistry&, SceneRenderer&);
    bool AuthorCollisionForPlacement(MapDocument&,const AssetRegistry&,size_t,std::string&);
    void StartClipboardPlacement();
    void CaptureRecentThumbnail(SceneRenderer& previewScene);
    void RememberCopiedAssets(const AssetRegistry& assets);
    void ActivateRecent(size_t recentPosition, const AssetRegistry&, SceneRenderer& previewScene);
    void BeginFunctionalPlacement(FunctionalPlacement type, SceneRenderer& scene);
    void CommitFunctionalPlacement(MapDocument&, SceneRenderer&);
    void DeleteFunctional(MapDocument&, SceneRenderer&);
    bool FunctionalPosition(const MapDocument&, DirectX::XMFLOAT3& out) const;
    void MoveFunctional(MapDocument&, const DirectX::XMFLOAT3& desired);

    bool Save(MapDocument&, bool saveAs);
    void Undo(MapDocument&, SceneRenderer&);
    void Redo(MapDocument&, SceneRenderer&);
    void SelectAllStaticProps(const MapDocument&, SceneRenderer&);
    void DeleteSelected(MapDocument&, SceneRenderer&);
    void ApplyLiveTransform(MapDocument&, SceneRenderer&, int propIndex, const PropTransformState& state);
    void PushDiscreteTransform(MapDocument&, SceneRenderer&, int propIndex, const PropTransformState& after, const char* label);
    void HandleEditorShortcuts(MapDocument&, SceneRenderer&, const AssetRegistry&);
    bool Pressed(Action action, bool allowFineShift = false) const;
    KeyBinding Binding(Action action) const;
    std::string Shortcut(Action action) const;
    std::string ToolbarLabel(const char* name, Action action) const;
    static const char* ActionName(Action action);
    static KeyBinding DefaultBinding(Action action);
    void ResetCustomBindings();
    void LoadControls();
    void SaveControls() const;
    bool GestureActive(MouseGesture gesture, bool dragging) const;
    void SelectAsset(const AssetRegistry&, size_t index, SceneRenderer& previewScene);
    void RebuildAssetPreview(const AssetRegistry&, SceneRenderer& previewScene);
    void BeginPlacement(const AssetRegistry&);
    void RequestSceneRebuild(bool preserveCamera = true) { sceneRebuildRequested_ = true; preserveCameraOnRebuild_ = preserveCamera; }
    void SetNavigationMode(NavigationMode mode);
    NavigationSettings& CurrentNavigationSettings();
    const char* NavigationModeName() const;
    static PropTransformState StateOf(const PropInstance& p);
    float SnapPosition(float v) const;
    float SnapDelta(float v) const;
    float SnapRotation(float radians) const;

    // 0=none, 1=map, 2=library, 3=external tester. No native picker opens before confirmation.
    int guideRequest_{};
    bool guideOpenRequested_{};
    int nativePickerRequest_{};
    bool guideSkipChecked_{};
    bool welcomePending_{true};
    bool welcomeSkipChecked_{true};
    GuidanceState guidance_;
    std::filesystem::path lastMapDirectory_;
    std::filesystem::path lastLibraryDirectory_;
    std::filesystem::path pendingMap_;
    bool newMapRequested_{};
    std::filesystem::path pendingLibrary_;
    std::string message_;
    bool messageError_{};
    bool showGrid_ = true;
    bool showCollision_ = false;
    bool showAllColliders_ = false;
    bool showBounds_ = false;
    bool showGameplay_ = false;
    bool showLights_ = false;
    bool lightPlacementActive_ = false; // explicit, independent of gameplay drop placement
    int selectedLight_{-1};
    LightMarker pendingLight_{};
    bool gameplayPaletteOpen_ = false;
    int newBonusKind_{};
    std::string newBonusTypeOverride_; // Native names already present in the map, or advanced manual token.
    bool newBonusFree_{true};
    bool newBonusParachute_{true};
    bool bonusModeExplanation_{};
    int newBonusModes_{1}; // Bitmask: DM, TDM, CTF, DOM, native AS; zero cannot be placed.
    float newSpawnYaw_{};
    bool showSpawns_ = false, showFlags_ = false, showPoints_ = false, showBonuses_ = false;
    int gameplayMode_ = -1; // -1 none, 0 all, 1 DM, 2 TDM, 3 CTF, 4 DOM/CP/CTP
    bool showShortcuts_ = false;
    bool showToastOverlay_ = false;
    bool showAuthorPopup_{}; // Optional, never covers the viewport by default.
    bool testRequested_ = false;
    bool firstGameplayFocus_=true;
    bool objectEditorOpen_=false;
    bool objectEditorOpening_{};
    unsigned long long objectEditorOpenAt_{};
    bool objectHelpRequested_{};
    bool objectSaveRequested_{};
    bool objectCloseRequested_{};
    bool objectDirty_{};
    bool objectVertexDragHistoryCaptured_{};
    struct ObjectSnapshot {
        ObjectDraft::Document draft;
        std::vector<DirectX::XMFLOAT3> vertices;
        std::vector<uint32_t> indices;
        int selectedVertex{-1};
    };
    std::vector<ObjectSnapshot> objectUndo_,objectRedo_;
    ObjectSnapshot CaptureObjectSnapshot() const;
    void RestoreObjectSnapshot(ObjectSnapshot&& state);
    ObjectDraft::Document objectDraft_;
    std::filesystem::path objectDraftOutputRoot_;
    SceneRenderer objectScene_; // independent 3D viewport; never reuses the map renderer
    std::vector<DirectX::XMFLOAT3> objectVisualVertices_;
    std::vector<DirectX::XMFLOAT3> objectOriginalVertices_;
    std::vector<uint32_t> objectVisualIndices_;
    std::vector<uint32_t> objectOriginalIndices_;
    std::array<int,3> objectFacePoints_{{-1,-1,-1}};
    int objectFacePointCount_{};
    int objectSelectedVertex_{-1};
    bool objectVertexDragActive_{};
    bool objectShowVertices_{true};
    bool objectShowBoxes_{}; // no unsolicited red collider over an imported mesh
    bool objectMeshEditable_{};
    bool objectShowMeshEdges_=true; // actual imported visual triangles, NOT inferred native colliders
    bool objectSceneInitialized_{};
    bool objectSceneNeedsBuild_{};
    bool objectSceneHasModel_{};
    int objectPickerRequest_{}; // 1=GLB, 2=output folder, 3=existing 3DS, 4=load draft
    int objectSelectedBox_{};
    char objectLegacySearch_[96]{};

    std::filesystem::path testerExecutable_;
    int effectMode_ = 0;
    float placementWheel_ = 0.0f;
    float pendingNativeWheel_{};
    bool axTabWasHeld_{};
    bool browseLibraryOpen_{};
    float browseWorkspaceY_{};
    char browseSearch_[128]{};
    int browseCategory_{}; // metadata-only heuristic category filter
    int browseSort_{};
    float browseThumbnailScale_{1.f};
    struct BrowseThumbnail {
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
        DirectX::XMFLOAT3 dimensions{}; // width/depth/height, computed only after this item's 3DS is opened
        unsigned long long touched{}; bool failed{}, hasDimensions{};
    };
    std::unordered_map<size_t,BrowseThumbnail> browseThumbnails_;
    unsigned long long browseFrame_{};
    bool browseRenderedThisFrame_{};
    bool browsePreviewNeedsRestore_{};
    bool showZones_ = false;
    bool snap_ = true;
    float gridSize_ = 500.0f;
    float rotationSnapDeg_ = 90.0f;
    float ghostRotation_{};
    bool gridFocusRequested_{};
    ToolMode tool_ = ToolMode::Select;
    int selected_ = -1;
    std::vector<int> selectedItems_;
    std::vector<PropInstance> clipboard_;
    DirectX::XMFLOAT3 clipboardAnchor_{};
    bool clipboardPlacement_{};
    // Native gameplay clipboard: retains all authored fields rather than palette defaults.
    FunctionalType functionalClipboardKind_{FunctionalType::None};
    DirectX::XMFLOAT3 functionalClipboardAnchor_{};
    CtfFlagMarker functionalClipboardFlag_;
    SpawnMarker functionalClipboardSpawn_;
    ControlPointMarker functionalClipboardPoint_;
    BonusRegionMarker functionalClipboardBonus_;
    SpecialBox functionalClipboardZone_;
    bool functionalPasteActive_{};
    std::vector<PropInstance> placementItems_; // relative to clipboard center or Library origin
    std::vector<PropInstance> ghostProps_;     // world positions; never serialized before Space
    DirectX::XMFLOAT3 ghostPivot_{};
    bool ghostValid_{};
    bool placementCommitRequested_{};
    float viewportX_{}, viewportY_{}, viewportW_{}, viewportH_{};
    bool selectionBoxActive_{};
    float selectionStartX_{}, selectionStartY_{}, selectionEndX_{}, selectionEndY_{};
    std::vector<PropTransformState> dragBefore_;
    std::vector<int> dragIndices_;
    struct RecentAsset { size_t index{}; Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> thumbnail; };
    std::vector<RecentAsset> recentAssets_;
    bool axPinned_{}, axOnlyUsed_{}, axConfirmRemove_{};
    int axRemovalIndex_{-1};
    int axCurrent_{0};
    bool axTabHeld_{};
    float axReveal_{};
    double axPinnedOpenedAt_{}; // short animated slide, 0 hidden .. 1 visible

    NavigationMode navigationMode_ = NavigationMode::Simple;
    NavigationSettings navLegacy_{1.0f,1.0f,1.0f,false,false};
    NavigationSettings navAdobe_{0.85f,1.0f,1.15f,false,true};
    NavigationSettings navSimple_{0.9f,1.0f,1.0f,false,true};
    NavigationSettings navCustom_{0.9f,1.0f,1.0f,false,true};
    MouseGesture customOrbit_{MouseGesture::Right};
    MouseGesture customPan_{MouseGesture::Middle};
    std::array<KeyBinding, static_cast<size_t>(Action::Count)> customKeys_{};
    bool showControlHelp_ = false;

    int selectedAsset_ = -1;
    int selectedTextureVariant_ = 0;
    bool assetPreviewReady_{};
    bool placementActive_{};
    PropInstance placementTemplate_{};
    float placementZ_{};

    FunctionalType functionalSelected_{FunctionalType::None};
    size_t functionalIndex_{};
    FunctionalPlacement functionalPlacement_{FunctionalPlacement::None};
    DirectX::XMFLOAT3 functionalGhostPosition_{};
    bool functionalGhostValid_{}, functionalCommitRequested_{};
    bool functionalDragActive_{};
    bool functionalResizeActive_{};
    bool functionalResizeMaxX_{}, functionalResizeMaxY_{};
    std::optional<SpecialBox> functionalResizeZoneBase_;
    std::optional<BonusRegionMarker> functionalResizeBonusBase_;
    std::optional<MapDocument> functionalDragBefore_;
    std::optional<MapDocument> functionalPropertyBefore_;
    std::optional<SpecialBox> zoneDragBefore_;
    std::optional<SpecialBox> zonePropertyBefore_;
    size_t zonePropertyIndex_{};
    double functionalDragPressedAt_{};
    DirectX::XMFLOAT3 functionalDragOriginal_{}, functionalDragPlaneStart_{};
    bool fullscreen_{};
    bool fullscreenToggleRequested_{};
    bool sceneRebuildRequested_{};
    bool preserveCameraOnRebuild_{};

    EditHistory history_;
    DragState drag_{};
    bool propertyEditActive_{};
    int propertyEditIndex_{-1};
    PropTransformState propertyEditBefore_{};
};
