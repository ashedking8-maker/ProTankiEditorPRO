#pragma once
#include "MapDocument.h"
#include "AssetRegistry.h"
#include <DirectXMath.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <wincodec.h>
#include <filesystem>
#include <array>
#include <algorithm>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

struct SceneRenderStats {
    size_t sourceProps{};
    size_t resolvedProps{};
    size_t missingAssets{};
    size_t meshInstances{};
    size_t spriteInstances{};
    size_t meshBatches{};
    size_t spriteBatches{};
    size_t gpuMeshes{};
    size_t gpuTextures{};
    size_t triangles{};
    size_t drawCalls{};
    double buildMilliseconds{};
};

class SceneRenderer {
public:
    bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context, std::string& error);
    void Shutdown();

    bool BuildScene(const MapDocument& map, const AssetRegistry& assets, std::string& error, bool frameScene = true);
    bool BuildAssetPreview(const AssetDefinition& asset, const std::string& textureVariant, std::string& error);
    // Preview-only tint for untextured GLB materials; never changes native maps.
    void ApplyDraftPreviewTint(const std::array<float,3>& tint);
    void ClearScene();
    // Browser keeps only small thumbnail SRVs; transient 3DS/texture GPU caches are not retained.
    void ReleasePreviewResources();

    void Resize(unsigned width, unsigned height);
    void SetEffectMode(int mode) { effectMode_ = mode < 0 ? 0 : mode > 6 ? 6 : mode; }
    void SetBackgroundColor(const DirectX::XMFLOAT3& rgb) { backgroundColor_=rgb; }
    void SetGridColor(const DirectX::XMFLOAT3& rgb);
    void SetNativeLights(const std::vector<LightMarker>& lights) { nativeLights_=lights; }
    void SetLightPreviewScale(float scale) { previewLightScale_=std::clamp(scale,1.0f,500.0f); } // editor-only; native XML unchanged
    // Overlay layer bits: spawns=1, flags=2, control points=4, bonuses=8, zones=16.
    // Mode bits: DM=1, TDM=2, CTF=4, DOM/CP=8; 15 displays all game modes.
    void Render(bool showGrid, bool showBounds = false, bool showGameplay = false, bool showZones = false,
                unsigned overlayMask = 31u, unsigned modeMask = 15u, bool collisionView = false, bool showLights = false, bool previewNativeLighting = false);

    ID3D11ShaderResourceView* Output() const { return colorSrv_.Get(); }
    ID3D11Device* Device() const { return device_; }
    ID3D11DeviceContext* Context() const { return context_; }
    unsigned Width() const { return width_; }
    unsigned Height() const { return height_; }
    const SceneRenderStats& Stats() const { return stats_; }
    // Parsed native collision primitives; zero for asset thumbnails / blank maps.
    std::array<size_t,4> CollisionPreviewCounts() const { return collisionPreviewCounts_; }
    DirectX::XMFLOAT3 PreviewDimensionsLegacy() const {
        if(!hasBounds_) return {};
        return {boundsMax_.x-boundsMin_.x,boundsMax_.z-boundsMin_.z,boundsMax_.y-boundsMin_.y};
    }
    const std::string& LastWarning() const { return lastWarning_; }

    void Orbit(float dxPixels, float dyPixels);
    void Pan(float dxPixels, float dyPixels);
    void Zoom(float wheelDelta, float speed = 1.0f);
    void FrameScene();
    // Camera-only 180-degree change; does not touch map positions or transforms.
    void ReverseViewDirection();
    void ResetReferenceViewDirection();
    void FrameSelection();
    void FocusSelectionKeepDistance(bool smooth = false);
    void AdvanceCameraFocus(float dt);
    void FocusLegacyPoint(const DirectX::XMFLOAT3& point, float distance = 1600.0f);
    DirectX::XMFLOAT3 CameraTargetLegacy() const;
    int Pick(float xPixels, float yPixels) const;
    bool ScreenToLegacyPlane(float xPixels, float yPixels, float legacyZ, DirectX::XMFLOAT3& outLegacy) const;
    bool ProjectLegacy(const DirectX::XMFLOAT3& point, float& x, float& y) const;
    // Imported GLB and 3DS visual vertices are ALREADY in internal Y-up space.
    bool ProjectWorld(const DirectX::XMFLOAT3& point, float& x, float& y) const;
    bool ScreenToWorldViewPlane(float x, float y,const DirectX::XMFLOAT3& anchor,DirectX::XMFLOAT3& out) const;
    bool UpdatePreviewMeshGeometry(const std::vector<DirectX::XMFLOAT3>& points,
                                   const std::vector<uint32_t>& triangles);
    void SetFunctionalGhost(const DirectX::XMFLOAT3& position, unsigned kind, bool active);
    bool UpdatePropTransform(int propIndex, const PropInstance& prop);
    void SetSelected(int propIndex) { selectedProp_ = propIndex; selectedProps_.clear(); if (propIndex >= 0) selectedProps_.push_back(propIndex); }
    void SetSelection(const std::vector<int>& indices) { selectedProps_ = indices; selectedProp_ = indices.empty() ? -1 : indices.back(); }
    std::vector<int> SelectInScreenRect(float x0, float y0, float x1, float y1) const;
    void CameraMoveBasisLegacy(DirectX::XMFLOAT3& right, DirectX::XMFLOAT3& forward) const;
    void MoveCameraLegacy(const DirectX::XMFLOAT3& displacement);
    // Transient 3D preview is not added to the map or to picking/serialization.
    void SetGhost(const std::vector<PropInstance>& items, const AssetRegistry& assets);
    void ClearGhost() { ghostItems_.clear(); }
    // Suggest geometry-edge translation for an already built ghost; never
    // changes XML or ghost until caller explicitly applies the returned delta.
    bool SuggestEdgeSnap(const std::vector<PropInstance>& props,float tolerance,
                         float clearance,float& legacyDx,float& legacyDy) const;
    int Selected() const { return selectedProp_; }

private:
    struct Vertex {
        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT3 normal;
        DirectX::XMFLOAT2 uv;
    };
    struct InstanceData {
        DirectX::XMFLOAT4 row0, row1, row2, row3;
    };
    struct SpriteInstanceData {
        DirectX::XMFLOAT3 position;
        float scale{};
        float originY{};
        DirectX::XMFLOAT3 padding{};
    };
    struct MeshPart {
        uint32_t firstIndex{}, indexCount{};
        std::filesystem::path diffuse;
        DirectX::XMFLOAT4 color{1,1,1,1};
        bool oppositeFaceAtlas{};
    };
    struct MeshGpu {
        std::vector<MeshPart> parts;
        std::vector<Vertex> cpuVertices; // retained for isolated live object authoring
        std::vector<uint32_t> cpuIndices;
        Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer;
        Microsoft::WRL::ComPtr<ID3D11Buffer> indexBuffer;
        uint32_t indexCount{};
        size_t triangles{};
        DirectX::XMFLOAT3 boundsMin{};
        DirectX::XMFLOAT3 boundsMax{};
    };
    struct TextureGpu {
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
        unsigned width{1};
        unsigned height{1};
    };
    struct MeshBatch {
        std::shared_ptr<MeshGpu> mesh;
        std::vector<std::shared_ptr<TextureGpu>> textures;
        std::vector<InstanceData> instances;
        Microsoft::WRL::ComPtr<ID3D11Buffer> instanceBuffer;
    };
    struct SpriteBatch {
        std::shared_ptr<TextureGpu> texture;
        std::vector<SpriteInstanceData> instances;
        Microsoft::WRL::ComPtr<ID3D11Buffer> instanceBuffer;
    };
    struct GhostItem {
        std::string identity;
        std::shared_ptr<MeshGpu> mesh;
        std::vector<std::shared_ptr<TextureGpu>> textures;
        std::shared_ptr<TextureGpu> sprite;
        float spriteScale{1.0f}, spriteOriginY{};
        Microsoft::WRL::ComPtr<ID3D11Buffer> instanceBuffer;
    };
    struct PickProxy {
        int propIndex{-1};
        DirectX::XMFLOAT3 boundsMin{};
        DirectX::XMFLOAT3 boundsMax{};
    };
    struct PropBinding {
        enum class Kind { None, Mesh, Sprite } kind{Kind::None};
        size_t batch{};
        size_t instance{};
        size_t proxy{};
    };
    struct DebugVertex { DirectX::XMFLOAT3 position; DirectX::XMFLOAT4 color; };
    struct DebugRange { uint32_t start{}, count{}, layer{}, modes{}; };
    // HLSL b1: max eight nearest omni lights, capped to avoid huge per-pixel work.
    struct NativeLightConstants {
        DirectX::XMFLOAT4 positionsAndIntensity[8]{};
        DirectX::XMFLOAT4 colorsAndBegin[8]{};
        DirectX::XMFLOAT4 endsAndReserved[8]{};
        DirectX::XMFLOAT4 count{};
    };
    struct CameraConstants {
        DirectX::XMFLOAT4X4 viewProjection;
        DirectX::XMFLOAT4 cameraRight;
        DirectX::XMFLOAT4 cameraUp;
        DirectX::XMFLOAT4 lightDirection;
    };

    bool CreateShaders(std::string& error);
    bool CreateStates(std::string& error);
    bool CreateFallbackTexture(std::string& error);
    bool CreateSpriteQuad(std::string& error);
    void CreateTargets();

    std::shared_ptr<MeshGpu> LoadMesh(const std::filesystem::path& file, std::string& warning);
    std::shared_ptr<TextureGpu> SolidTexture(const DirectX::XMFLOAT4& color);
    std::shared_ptr<TextureGpu> LoadTexture(const std::filesystem::path& file, std::string& warning);
    std::shared_ptr<TextureGpu> ResolveTexture(const AssetDefinition& asset, const std::string& variant, const std::filesystem::path& materialTexture, std::string& warning);
    static std::string PathKey(const std::filesystem::path& path);
    static InstanceData MakeInstance(const PropInstance& prop);

    void RenderGrid(const DirectX::XMMATRIX& viewProjection);
    void RenderCollisionGeometry();
    void BuildCollisionGeometry(const MapDocument& map);
    void RenderMeshes(const CameraConstants& camera);
    void RenderSprites(const CameraConstants& camera);
    void RenderDebugOverlay(bool showBounds, bool showGameplay, bool showZones, unsigned overlayMask, unsigned modeMask, bool showLights);
    void RenderSelection();
    void RenderGhost();
    void RenderFunctionalPads(unsigned overlayMask, unsigned modeMask);
    void BuildFunctionalModels(const MapDocument& map);
    void RenderFunctionalModels(unsigned overlayMask, unsigned modeMask);
    void RenderFunctionalGhost();
    void BuildDebugGeometry(const MapDocument& map);
    void BuildFunctionalPads(const MapDocument& map);
    bool UploadDebugGeometry();
    void UpdateSceneBounds(const MeshGpu& mesh, const DirectX::XMMATRIX& world);
    static void WorldBounds(const MeshGpu& mesh, const DirectX::XMMATRIX& world, DirectX::XMFLOAT3& outMin, DirectX::XMFLOAT3& outMax);
    DirectX::XMVECTOR CameraEye() const;
    bool ScreenRay(float xPixels, float yPixels, DirectX::XMFLOAT3& origin, DirectX::XMFLOAT3& direction) const;
    void RecomputeSceneBoundsFromProxies();
    template<class T> bool UploadDynamicInstances(const std::vector<T>& instances, ID3D11Buffer* buffer);

    ID3D11Device* device_{};
    ID3D11DeviceContext* context_{};
    Microsoft::WRL::ComPtr<IWICImagingFactory> wic_;
    unsigned width_{};
    unsigned height_{};

    Microsoft::WRL::ComPtr<ID3D11Texture2D> colorTexture_;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> colorRtv_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> colorSrv_;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> depthTexture_;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depthDsv_;

    Microsoft::WRL::ComPtr<ID3D11VertexShader> meshVs_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> meshPs_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> ghostMeshPs_, ghostSpritePs_;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> meshLayout_;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> spriteVs_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> spritePs_;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> spriteLayout_;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> gridVs_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> gridPs_;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> gridLayout_;

    Microsoft::WRL::ComPtr<ID3D11Buffer> cameraBuffer_, nativeLightBuffer_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> spriteVertexBuffer_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> spriteIndexBuffer_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> gridVertexBuffer_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> selectionVertexBuffer_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> debugVertexBuffer_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> collisionFaceBuffer_, collisionEdgeBuffer_;
    uint32_t collisionFaceVertices_{}, collisionEdgeVertices_{};
    std::array<size_t,4> collisionPreviewCounts_{}; // plane, box, triangle, omitted
    Microsoft::WRL::ComPtr<ID3D11Buffer> functionalPadBuffer_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> functionalGhostBuffer_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> functionalGhostModelBuffer_;
    std::array<MeshBatch,3> functionalModelBatches_{}; // red flag, blue flag, neutral CP; native XML is unchanged.
    uint32_t functionalPadVertices_{};
    uint32_t functionalFlagVertices_{};
    uint32_t functionalPointVertices_{};
    std::array<uint32_t,4> functionalSpawnStarts_{};
    std::array<uint32_t,4> functionalSpawnCounts_{};
    unsigned functionalGhostKind_{};
    bool functionalGhostActive_{};
    DirectX::XMFLOAT3 functionalGhostPosition_{};
    uint32_t gridVertexCount_{};
    uint32_t boundsVertexCount_{};
    uint32_t gameplayVertexCount_{};
    uint32_t zoneVertexCount_{};
    std::vector<DebugVertex> debugVertices_;
    std::vector<DebugRange> gameplayRanges_;

    Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler_;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> spriteSampler_;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizer_;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizerPairedAtlas_;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depthState_;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> spriteDepthState_;
    Microsoft::WRL::ComPtr<ID3D11BlendState> opaqueBlend_;
    Microsoft::WRL::ComPtr<ID3D11BlendState> alphaBlend_;

    std::shared_ptr<TextureGpu> fallbackTexture_;
    std::unordered_map<std::string, std::shared_ptr<MeshGpu>> meshCache_;
    std::unordered_map<std::string, std::shared_ptr<TextureGpu>> textureCache_;
    std::vector<MeshBatch> meshBatches_;
    std::vector<SpriteBatch> spriteBatches_;
    std::vector<PickProxy> pickProxies_;
    std::vector<GhostItem> ghostItems_;
    std::vector<int> selectedProps_;
    std::vector<PropBinding> propBindings_;
    int selectedProp_ = -1;
    bool previewMeshDetached_{};

    SceneRenderStats stats_{};
    std::string lastWarning_;
    int effectMode_{}; // Shader-only editor preview, never exported into legacy map XML.
    bool cameraFocusAnimating_{};
    DirectX::XMFLOAT3 cameraFocusGoal_{};
    DirectX::XMFLOAT3 gridColor_{0.52f,0.56f,0.60f};
    DirectX::XMFLOAT3 backgroundColor_{0.055f,0.073f,0.10f};
    std::vector<LightMarker> nativeLights_;
    float previewLightScale_{100.0f};
    bool hasBounds_{};
    DirectX::XMFLOAT3 boundsMin_{};
    DirectX::XMFLOAT3 boundsMax_{};

    DirectX::XMFLOAT3 cameraTarget_{0, 0, 0};
    float cameraYaw_ = 2.39159265f; // opposite side to 0.5.21; reference-map opening view
    float cameraPitch_ = 0.55f;
    float cameraDistance_ = 15000.0f;
};
