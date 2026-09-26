#include "SceneRenderer.h"
#include "CollisionPreview.h"
#include "LegacyTransform.h"
#include "GeometrySnap.h"
#include "LegacyMeshImport.h"
#include "DraftMeshImport.h"
#include "Logger.h"
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <d3dcompiler.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cctype>
#include <cmath>
#include <limits>
#include <utility>
#include <sstream>
#include <unordered_map>
#include <type_traits>
#include <windows.h>

using Microsoft::WRL::ComPtr;
using namespace DirectX;

namespace {

struct GridVertex {
    XMFLOAT3 position;
    XMFLOAT4 color;
};

struct SpriteVertex {
    XMFLOAT2 corner;
    XMFLOAT2 uv;
};

std::string BlobMessage(ID3DBlob* blob) {
    if (!blob || !blob->GetBufferPointer()) return {};
    return std::string(static_cast<const char*>(blob->GetBufferPointer()), blob->GetBufferSize());
}

bool Compile(const char* source, const char* entry, const char* profile, ComPtr<ID3DBlob>& blob, std::string& error) {
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(_DEBUG)
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif
    ComPtr<ID3DBlob> errors;
    const HRESULT hr = D3DCompile(source, std::strlen(source), nullptr, nullptr, nullptr, entry, profile, flags, 0, &blob, &errors);
    if (FAILED(hr)) {
        error = "Shader compilation failed: " + BlobMessage(errors.Get());
        return false;
    }
    return true;
}

std::string Lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

void AppendWarning(std::string& target, const std::string& warning) {
    if (warning.empty()) return;
    if (!target.empty()) target += '\n';
    target += warning;
}

XMFLOAT3 Min3(const XMFLOAT3& a, const XMFLOAT3& b) {
    return {std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z)};
}
XMFLOAT3 Max3(const XMFLOAT3& a, const XMFLOAT3& b) {
    return {std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z)};
}

} // namespace

bool SceneRenderer::Initialize(ID3D11Device* device, ID3D11DeviceContext* context, std::string& error) {
    device_ = device;
    context_ = context;
    if (!device_ || !context_) {
        error = "Scene renderer received a null D3D11 device/context.";
        return false;
    }

    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wic_));
    if (FAILED(hr)) {
        error = "Windows Imaging Component could not be initialized.";
        return false;
    }

    if (!CreateShaders(error) || !CreateStates(error) || !CreateFallbackTexture(error) || !CreateSpriteQuad(error))
        return false;

    D3D11_BUFFER_DESC cb{};
    cb.ByteWidth = sizeof(CameraConstants);
    cb.Usage = D3D11_USAGE_DEFAULT;
    cb.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    hr = device_->CreateBuffer(&cb, nullptr, &cameraBuffer_);
    if (FAILED(hr)) {
        error = "Could not create the scene camera constant buffer.";
        return false;
    }

    cb.ByteWidth = sizeof(NativeLightConstants);
    hr = device_->CreateBuffer(&cb, nullptr, &nativeLightBuffer_);
    if (FAILED(hr)) { error="Could not create editor light preview buffer."; return false; }

    // A world-space editor grid. Fine lines every 500 legacy units, stronger lines every 5000.
    std::vector<GridVertex> grid;
    constexpr int halfExtent = 100000;
    constexpr int step = 500;
    grid.reserve(((halfExtent * 2 / step) + 1) * 4);
    for (int v = -halfExtent; v <= halfExtent; v += step) {
        const bool major = (v % 5000) == 0;
        const float alpha = major ? 0.25f : 0.085f;
        const XMFLOAT4 color = (v == 0) ? XMFLOAT4{gridColor_.x * 0.55f, gridColor_.y * 0.85f, gridColor_.z, 0.42f} : XMFLOAT4{gridColor_.x, gridColor_.y, gridColor_.z, alpha};
        grid.push_back({{static_cast<float>(-halfExtent), 0.0f, static_cast<float>(v)}, color});
        grid.push_back({{static_cast<float>( halfExtent), 0.0f, static_cast<float>(v)}, color});
        grid.push_back({{static_cast<float>(v), 0.0f, static_cast<float>(-halfExtent)}, color});
        grid.push_back({{static_cast<float>(v), 0.0f, static_cast<float>( halfExtent)}, color});
    }
    gridVertexCount_ = static_cast<uint32_t>(grid.size());
    D3D11_BUFFER_DESC gb{};
    gb.ByteWidth = static_cast<UINT>(grid.size() * sizeof(GridVertex));
    gb.Usage = D3D11_USAGE_DEFAULT;
    gb.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA gd{grid.data()};
    hr = device_->CreateBuffer(&gb, &gd, &gridVertexBuffer_);
    if (FAILED(hr)) {
        error = "Could not create the editor grid buffer.";
        return false;
    }

    D3D11_BUFFER_DESC sb{};
    sb.ByteWidth = sizeof(GridVertex) * 24;
    sb.Usage = D3D11_USAGE_DYNAMIC;
    sb.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    sb.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    hr = device_->CreateBuffer(&sb, nullptr, &selectionVertexBuffer_);
    if (FAILED(hr)) {
        error = "Could not create the selection outline buffer.";
        return false;
    }

    error.clear();
    return true;
}

void SceneRenderer::SetGridColor(const XMFLOAT3& rgb) {
    const XMFLOAT3 newColor{std::clamp(rgb.x,0.f,1.f),std::clamp(rgb.y,0.f,1.f),std::clamp(rgb.z,0.f,1.f)};
    if (std::fabs(gridColor_.x-newColor.x)<0.00001f &&
        std::fabs(gridColor_.y-newColor.y)<0.00001f &&
        std::fabs(gridColor_.z-newColor.z)<0.00001f) return;
    gridColor_=newColor;
    if(!context_ || !gridVertexBuffer_)return;
    std::vector<GridVertex> grid;
    constexpr int halfExtent=100000, step=500;
    grid.reserve(((halfExtent*2/step)+1)*4);
    for(int v=-halfExtent;v<=halfExtent;v+=step) {
        const float alpha=(v%5000)==0?0.25f:0.085f;
        const XMFLOAT4 color=(v==0)?XMFLOAT4{gridColor_.x*0.55f,gridColor_.y*0.85f,gridColor_.z,0.42f}:
            XMFLOAT4{gridColor_.x,gridColor_.y,gridColor_.z,alpha};
        grid.push_back({{static_cast<float>(-halfExtent),0.f,static_cast<float>(v)},color});
        grid.push_back({{static_cast<float>( halfExtent),0.f,static_cast<float>(v)},color});
        grid.push_back({{static_cast<float>(v),0.f,static_cast<float>(-halfExtent)},color});
        grid.push_back({{static_cast<float>(v),0.f,static_cast<float>( halfExtent)},color});
    }
    context_->UpdateSubresource(gridVertexBuffer_.Get(),0,nullptr,grid.data(),0,0);
}

void SceneRenderer::Shutdown() {
    ClearScene();
    meshCache_.clear();
    textureCache_.clear();
    fallbackTexture_.reset();
    colorSrv_.Reset(); colorRtv_.Reset(); colorTexture_.Reset(); depthDsv_.Reset(); depthTexture_.Reset();
    meshVs_.Reset(); meshPs_.Reset(); ghostMeshPs_.Reset(); ghostSpritePs_.Reset(); meshLayout_.Reset();
    spriteVs_.Reset(); spritePs_.Reset(); spriteLayout_.Reset();
    gridVs_.Reset(); gridPs_.Reset(); gridLayout_.Reset();
    cameraBuffer_.Reset(); nativeLightBuffer_.Reset(); spriteVertexBuffer_.Reset(); spriteIndexBuffer_.Reset(); gridVertexBuffer_.Reset(); selectionVertexBuffer_.Reset(); debugVertexBuffer_.Reset(); collisionFaceBuffer_.Reset(); collisionEdgeBuffer_.Reset(); functionalPadBuffer_.Reset(); functionalGhostBuffer_.Reset(); functionalGhostModelBuffer_.Reset();
    sampler_.Reset(); spriteSampler_.Reset(); rasterizer_.Reset(); rasterizerPairedAtlas_.Reset(); depthState_.Reset(); spriteDepthState_.Reset();
    opaqueBlend_.Reset(); alphaBlend_.Reset();
    wic_.Reset();
    device_ = nullptr;
    context_ = nullptr;
    width_ = height_ = 0;
}

bool SceneRenderer::CreateShaders(std::string& error) {
    static constexpr const char* meshShader = R"HLSL(
cbuffer Camera : register(b0) {
    row_major float4x4 viewProjection;
    float4 cameraRight;
    float4 cameraUp;
    float4 lightDirection;
};
// Editor-only approximation of native omni records, not original ProTLVK shading.
cbuffer NativeLightPreview : register(b1) {
    float4 omniPositionIntensity[8];
    float4 omniColorBegin[8];
    float4 omniEndReserved[8];
    float4 omniCount;
};
Texture2D diffuseTexture : register(t0);
SamplerState diffuseSampler : register(s0);

struct VSIn {
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float2 uv       : TEXCOORD0;
    float4 row0     : INSTANCE0;
    float4 row1     : INSTANCE1;
    float4 row2     : INSTANCE2;
    float4 row3     : INSTANCE3;
};
struct VSOut {
    float4 position : SV_POSITION;
    float3 normal   : NORMAL;
    float2 uv       : TEXCOORD0;
    float3 worldPos : TEXCOORD1;
};
VSOut VSMain(VSIn input) {
    row_major float4x4 world = float4x4(input.row0, input.row1, input.row2, input.row3);
    float4 worldPosition = mul(float4(input.position, 1.0), world);
    VSOut output;
    output.position = mul(worldPosition, viewProjection);
    output.normal = normalize(mul(float4(input.normal, 0.0), world).xyz);
    output.uv = input.uv;
    output.worldPos = worldPosition.xyz;
    return output;
}
float4 PSMain(VSOut input) : SV_TARGET {
    float4 albedo = diffuseTexture.Sample(diffuseSampler, input.uv);
    // Meshes are rendered in an opaque depth-writing pass. Treat transparent
    // texels as a CUTOUT rather than painting near-zero alpha texels opaquely.
    // A separate sorted transparent pass is required for true glass materials.
    clip(albedo.a - 0.5);
    // Diagnostic bypass: exact source texture (no editor lighting or color FX).
    const float mode = lightDirection.w;
    if (mode > 5.5) return float4(albedo.rgb, 1.0);
    // WIC creates an R8G8B8A8_UNORM SRV; the source JPEG/PNG RGB bytes are
    // sRGB-encoded. Lighting those bytes directly causes very dark materials.
    float3 linearAlbedo = pow(max(albedo.rgb, 0.0), 2.2);
    float light = 0.38 + 0.62 * saturate(dot(normalize(input.normal), -lightDirection.xyz));
    float3 c = linearAlbedo * light;
    float3 nearLight = float3(0,0,0);
    [loop] for(int i=0;i<8;++i) {
        if (i >= (int)omniCount.x) break;
        float3 delta = omniPositionIntensity[i].xyz - input.worldPos;
        float distanceFromLamp = length(delta);
        float reach = max(omniEndReserved[i].x, 0.001);
        float begin = min(omniColorBegin[i].w,reach-0.0001);
        float attenuation = 1.0 - smoothstep(begin,reach,distanceFromLamp);
        // Preview lights are isotropic; materials/shadows are intentionally approximate.
        nearLight += omniColorBegin[i].rgb * omniPositionIntensity[i].w * attenuation * 0.85;
    }
    c += linearAlbedo * nearLight;
    c = pow(saturate(c), 1.0 / 2.2); // Encode to sRGB for the UNORM render target.
    if (mode > 0.5 && mode < 1.5) c = c * 1.45 + 0.035; // Bright
    if (mode > 1.5 && mode < 2.5) c = (c - 0.5) * 1.40 + 0.54; // Crisp
    if (mode > 2.5 && mode < 3.5) c *= float3(1.13, 1.035, 0.91); // Warm
    if (mode > 3.5 && mode < 4.5) c *= float3(0.92, 1.035, 1.15); // Cool
    if (mode > 4.5 && mode < 5.5) c = (c - 0.5) * 1.35 + 0.5; // High contrast
    return float4(saturate(c), albedo.a);
}
float4 PSGhost(VSOut input) : SV_TARGET {
    float4 albedo = diffuseTexture.Sample(diffuseSampler, input.uv);
    clip(albedo.a - 0.015);
    return float4(albedo.rgb * 0.70 + float3(0.04, 0.16, 0.23), albedo.a * 0.53);
}
// The full Alternativa color-map/lighting pipeline is not reproduced here.

)HLSL";

    static constexpr const char* spriteShader = R"HLSL(
cbuffer Camera : register(b0) {
    row_major float4x4 viewProjection;
    float4 cameraRight;
    float4 cameraUp;
    float4 lightDirection;
};
Texture2D spriteTexture : register(t0);
SamplerState spriteSampler : register(s0);
struct VSIn {
    float2 corner           : POSITION;
    float2 uv               : TEXCOORD0;
    float3 instancePosition : INSTANCEPOS;
    float  scale            : INSTANCESCALE;
    float  originY          : INSTANCEORIGIN;
};
struct VSOut { float4 position : SV_POSITION; float2 uv : TEXCOORD0; };
VSOut VSMain(VSIn input) {
    uint width, height;
    spriteTexture.GetDimensions(width, height);
    float localX = input.corner.x * float(width) * input.scale;
    float localY = (input.originY - input.uv.y) * float(height) * input.scale;
    float3 world = input.instancePosition + cameraRight.xyz * localX + cameraUp.xyz * localY;
    VSOut output;
    output.position = mul(float4(world, 1.0), viewProjection);
    output.uv = input.uv;
    return output;
}
float4 PSMain(VSOut input) : SV_TARGET {
    float4 c = spriteTexture.Sample(spriteSampler, input.uv);
    clip(c.a - 0.04);
    float3 rgb=c.rgb;
    const float mode=lightDirection.w;
    if (mode > 0.5 && mode < 1.5) rgb *= 1.22;
    if (mode > 1.5 && mode < 2.5) rgb = (rgb-0.5)*1.18+0.54;
    if (mode > 2.5 && mode < 3.5) rgb *= float3(1.13,1.035,0.91);
    if (mode > 3.5 && mode < 4.5) rgb *= float3(0.92,1.035,1.15);
    if (mode > 4.5) rgb=(rgb-0.5)*1.35+0.5;
    return float4(saturate(rgb),c.a);
}
float4 PSGhost(VSOut input) : SV_TARGET {
    float4 c = spriteTexture.Sample(spriteSampler, input.uv);
    clip(c.a - 0.04);
    return float4(c.rgb * 0.70 + float3(0.04, 0.16, 0.23), c.a * 0.53);
}
)HLSL";

    static constexpr const char* gridShader = R"HLSL(
cbuffer Camera : register(b0) {
    row_major float4x4 viewProjection;
    float4 cameraRight;
    float4 cameraUp;
    float4 lightDirection;
};
struct VSIn { float3 position : POSITION; float4 color : COLOR0; };
struct VSOut { float4 position : SV_POSITION; float4 color : COLOR0; };
VSOut VSMain(VSIn input) {
    VSOut output;
    output.position = mul(float4(input.position, 1.0), viewProjection);
    output.color = input.color;
    return output;
}
float4 PSMain(VSOut input) : SV_TARGET { return input.color; }
)HLSL";

    ComPtr<ID3DBlob> vs, ps;
    if (!Compile(meshShader, "VSMain", "vs_5_0", vs, error) || !Compile(meshShader, "PSMain", "ps_5_0", ps, error)) return false;
    if (FAILED(device_->CreateVertexShader(vs->GetBufferPointer(), vs->GetBufferSize(), nullptr, &meshVs_)) ||
        FAILED(device_->CreatePixelShader(ps->GetBufferPointer(), ps->GetBufferSize(), nullptr, &meshPs_))) {
        error = "Could not create mesh shaders."; return false;
    }
    if (!Compile(meshShader, "PSGhost", "ps_5_0", ps, error) ||
        FAILED(device_->CreatePixelShader(ps->GetBufferPointer(), ps->GetBufferSize(), nullptr, &ghostMeshPs_))) {
        error = "Could not create mesh ghost shader."; return false;
    }
    const D3D11_INPUT_ELEMENT_DESC meshElements[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, offsetof(Vertex, position), D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, offsetof(Vertex, normal),   D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, offsetof(Vertex, uv),       D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"INSTANCE", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, offsetof(InstanceData, row0), D3D11_INPUT_PER_INSTANCE_DATA, 1},
        {"INSTANCE", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, offsetof(InstanceData, row1), D3D11_INPUT_PER_INSTANCE_DATA, 1},
        {"INSTANCE", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, offsetof(InstanceData, row2), D3D11_INPUT_PER_INSTANCE_DATA, 1},
        {"INSTANCE", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, offsetof(InstanceData, row3), D3D11_INPUT_PER_INSTANCE_DATA, 1},
    };
    if (FAILED(device_->CreateInputLayout(meshElements, ARRAYSIZE(meshElements), vs->GetBufferPointer(), vs->GetBufferSize(), &meshLayout_))) {
        error = "Could not create mesh input layout."; return false;
    }

    if (!Compile(spriteShader, "VSMain", "vs_5_0", vs, error) || !Compile(spriteShader, "PSMain", "ps_5_0", ps, error)) return false;
    if (FAILED(device_->CreateVertexShader(vs->GetBufferPointer(), vs->GetBufferSize(), nullptr, &spriteVs_)) ||
        FAILED(device_->CreatePixelShader(ps->GetBufferPointer(), ps->GetBufferSize(), nullptr, &spritePs_))) {
        error = "Could not create sprite shaders."; return false;
    }
    if (!Compile(spriteShader, "PSGhost", "ps_5_0", ps, error) ||
        FAILED(device_->CreatePixelShader(ps->GetBufferPointer(), ps->GetBufferSize(), nullptr, &ghostSpritePs_))) {
        error = "Could not create sprite ghost shader."; return false;
    }
    const D3D11_INPUT_ELEMENT_DESC spriteElements[] = {
        {"POSITION",       0, DXGI_FORMAT_R32G32_FLOAT,    0, offsetof(SpriteVertex, corner), D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD",       0, DXGI_FORMAT_R32G32_FLOAT,    0, offsetof(SpriteVertex, uv),     D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"INSTANCEPOS",    0, DXGI_FORMAT_R32G32B32_FLOAT, 1, offsetof(SpriteInstanceData, position), D3D11_INPUT_PER_INSTANCE_DATA, 1},
        {"INSTANCESCALE",  0, DXGI_FORMAT_R32_FLOAT,       1, offsetof(SpriteInstanceData, scale),    D3D11_INPUT_PER_INSTANCE_DATA, 1},
        {"INSTANCEORIGIN", 0, DXGI_FORMAT_R32_FLOAT,       1, offsetof(SpriteInstanceData, originY),  D3D11_INPUT_PER_INSTANCE_DATA, 1},
    };
    if (FAILED(device_->CreateInputLayout(spriteElements, ARRAYSIZE(spriteElements), vs->GetBufferPointer(), vs->GetBufferSize(), &spriteLayout_))) {
        error = "Could not create sprite input layout."; return false;
    }

    if (!Compile(gridShader, "VSMain", "vs_5_0", vs, error) || !Compile(gridShader, "PSMain", "ps_5_0", ps, error)) return false;
    if (FAILED(device_->CreateVertexShader(vs->GetBufferPointer(), vs->GetBufferSize(), nullptr, &gridVs_)) ||
        FAILED(device_->CreatePixelShader(ps->GetBufferPointer(), ps->GetBufferSize(), nullptr, &gridPs_))) {
        error = "Could not create grid shaders."; return false;
    }
    const D3D11_INPUT_ELEMENT_DESC gridElements[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, offsetof(GridVertex, position), D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(GridVertex, color),    D3D11_INPUT_PER_VERTEX_DATA, 0},
    };
    if (FAILED(device_->CreateInputLayout(gridElements, ARRAYSIZE(gridElements), vs->GetBufferPointer(), vs->GetBufferSize(), &gridLayout_))) {
        error = "Could not create grid input layout."; return false;
    }

    error.clear();
    return true;
}

bool SceneRenderer::CreateStates(std::string& error) {
    D3D11_SAMPLER_DESC sd{};
    sd.Filter = D3D11_FILTER_ANISOTROPIC;
    sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sd.MaxAnisotropy = 8;
    sd.MaxLOD = D3D11_FLOAT32_MAX;
    if (FAILED(device_->CreateSamplerState(&sd, &sampler_))) { error = "Could not create mesh sampler."; return false; }
    sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    if (FAILED(device_->CreateSamplerState(&sd, &spriteSampler_))) { error = "Could not create sprite sampler."; return false; }

    D3D11_RASTERIZER_DESC rd{};
    rd.FillMode = D3D11_FILL_SOLID;
    rd.CullMode = D3D11_CULL_NONE; // Legacy .3ds assets are not consistently wound.
    rd.DepthClipEnable = TRUE;
    if (FAILED(device_->CreateRasterizerState(&rd, &rasterizer_))) { error = "Could not create rasterizer state."; return false; }
    // Only meshes with fully paired opposite-winding UV atlas faces need culling.
    // This avoids painting the underside over the upper face at identical depth.
    // A 0.5.18 Windows comparison showed this state selected the DARK atlas
    // side above the Bridge 1 ramp, and the BRIGHT atlas side underneath.
    // Cull the opposite source winding for this detected paired-face case.
    // Other legacy models keep their original double-sided rasterizer.
    rd.CullMode = D3D11_CULL_FRONT;
    rd.FrontCounterClockwise = TRUE;
    if (FAILED(device_->CreateRasterizerState(&rd, &rasterizerPairedAtlas_))) {
        error = "Could not create paired-atlas rasterizer state."; return false;
    }

    D3D11_DEPTH_STENCIL_DESC dd{};
    dd.DepthEnable = TRUE;
    dd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    dd.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    if (FAILED(device_->CreateDepthStencilState(&dd, &depthState_))) { error = "Could not create depth state."; return false; }
    dd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    if (FAILED(device_->CreateDepthStencilState(&dd, &spriteDepthState_))) { error = "Could not create sprite depth state."; return false; }

    D3D11_BLEND_DESC bd{};
    bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    if (FAILED(device_->CreateBlendState(&bd, &opaqueBlend_))) { error = "Could not create opaque blend state."; return false; }
    bd.RenderTarget[0].BlendEnable = TRUE;
    bd.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    bd.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    bd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    if (FAILED(device_->CreateBlendState(&bd, &alphaBlend_))) { error = "Could not create alpha blend state."; return false; }

    error.clear();
    return true;
}

bool SceneRenderer::CreateFallbackTexture(std::string& error) {
    const uint32_t pixel = 0xFF8B8580u; // neutral warm gray in RGBA8 memory order
    D3D11_TEXTURE2D_DESC td{};
    td.Width = td.Height = 1;
    td.MipLevels = td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_IMMUTABLE;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA data{&pixel, sizeof(pixel), 0};
    ComPtr<ID3D11Texture2D> texture;
    if (FAILED(device_->CreateTexture2D(&td, &data, &texture))) { error = "Could not create fallback texture."; return false; }
    fallbackTexture_ = std::make_shared<TextureGpu>();
    if (FAILED(device_->CreateShaderResourceView(texture.Get(), nullptr, &fallbackTexture_->srv))) { error = "Could not create fallback texture view."; return false; }
    error.clear();
    return true;
}

bool SceneRenderer::CreateSpriteQuad(std::string& error) {
    // corner.x is centered horizontally. UV y/origin-y determines vertical anchoring in the shader.
    const SpriteVertex vertices[] = {
        {{-0.5f, 0.0f}, {0,0}}, {{ 0.5f, 0.0f}, {1,0}},
        {{ 0.5f, 1.0f}, {1,1}}, {{-0.5f, 1.0f}, {0,1}},
    };
    const uint16_t indices[] = {0,1,2, 0,2,3};
    D3D11_BUFFER_DESC vb{}; vb.ByteWidth = sizeof(vertices); vb.Usage = D3D11_USAGE_IMMUTABLE; vb.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA vd{vertices};
    if (FAILED(device_->CreateBuffer(&vb, &vd, &spriteVertexBuffer_))) { error = "Could not create sprite vertex buffer."; return false; }
    D3D11_BUFFER_DESC ib{}; ib.ByteWidth = sizeof(indices); ib.Usage = D3D11_USAGE_IMMUTABLE; ib.BindFlags = D3D11_BIND_INDEX_BUFFER;
    D3D11_SUBRESOURCE_DATA id{indices};
    if (FAILED(device_->CreateBuffer(&ib, &id, &spriteIndexBuffer_))) { error = "Could not create sprite index buffer."; return false; }
    error.clear();
    return true;
}

void SceneRenderer::Resize(unsigned width, unsigned height) {
    width = std::max(1u, width);
    height = std::max(1u, height);
    if (width == width_ && height == height_ && colorSrv_) return;
    width_ = width; height_ = height;
    CreateTargets();
}

void SceneRenderer::CreateTargets() {
    colorSrv_.Reset(); colorRtv_.Reset(); colorTexture_.Reset(); depthDsv_.Reset(); depthTexture_.Reset();
    if (!device_ || width_ == 0 || height_ == 0) return;

    D3D11_TEXTURE2D_DESC td{};
    td.Width = width_; td.Height = height_; td.MipLevels = td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    if (FAILED(device_->CreateTexture2D(&td, nullptr, &colorTexture_))) return;
    if (FAILED(device_->CreateRenderTargetView(colorTexture_.Get(), nullptr, &colorRtv_))) return;
    if (FAILED(device_->CreateShaderResourceView(colorTexture_.Get(), nullptr, &colorSrv_))) return;

    td.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    td.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    if (FAILED(device_->CreateTexture2D(&td, nullptr, &depthTexture_))) return;
    device_->CreateDepthStencilView(depthTexture_.Get(), nullptr, &depthDsv_);
}

std::string SceneRenderer::PathKey(const std::filesystem::path& path) {
    std::error_code ec;
    auto normalized = std::filesystem::weakly_canonical(path, ec);
    if (ec) normalized = path.lexically_normal();
    return Lower(normalized.generic_string());
}

std::shared_ptr<SceneRenderer::MeshGpu> SceneRenderer::LoadMesh(const std::filesystem::path& file, std::string& warning) {
    const std::string key = PathKey(file);
    if (const auto it = meshCache_.find(key); it != meshCache_.end()) return it->second;
    if (!std::filesystem::exists(file)) {
        warning = "Missing mesh: " + Log::PathUtf8(file);
        Log::Warning(warning);
        return {};
    }

    LegacyMeshImport::Model imported;
    try { imported = Lower(file.extension().string()) == ".glb" ? DraftMeshImport::Load(file) : LegacyMeshImport::Load(file); }
    catch (const std::exception& e) {
        warning = "Mesh import failed: " + Log::PathUtf8(file) + ": " + e.what();
        Log::Error(warning); return {};
    }
    for (const auto& w : imported.warnings) Log::Warning(Log::PathUtf8(file) + ": " + w);
    std::vector<Vertex> vertices;
    vertices.reserve(imported.vertices.size());
    auto& indices = imported.indices;
    XMFLOAT3 bmin{ std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max() };
    XMFLOAT3 bmax{-std::numeric_limits<float>::max(),-std::numeric_limits<float>::max(),-std::numeric_limits<float>::max() };
    for (const auto& v : imported.vertices) {
        const XMFLOAT3 p{v.position.x,v.position.y,v.position.z};
        vertices.push_back({p,{v.normal.x,v.normal.y,v.normal.z},{v.uv.x,v.uv.y}});
        bmin=Min3(bmin,p); bmax=Max3(bmax,p);
    }
    auto gpu = std::make_shared<MeshGpu>();
    gpu->cpuVertices=vertices;
    gpu->cpuIndices=indices;
    for (const auto& part : imported.parts) gpu->parts.push_back({part.firstIndex,part.indexCount,part.diffuse,{part.color.r,part.color.g,part.color.b,part.color.a},part.oneSidedPairedAtlas});
    Log::Debug("Mesh visual anchor: " + imported.anchor + " parts=" + std::to_string(imported.parts.size()) + " ignoredHelperNodes=" + std::to_string(imported.ignoredMeshNodes));
    D3D11_BUFFER_DESC vb{}; vb.ByteWidth = static_cast<UINT>(vertices.size() * sizeof(Vertex)); vb.Usage = D3D11_USAGE_IMMUTABLE; vb.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA vd{vertices.data()};
    if (FAILED(device_->CreateBuffer(&vb, &vd, &gpu->vertexBuffer))) { warning = "GPU vertex upload failed: " + file.string(); return {}; }
    D3D11_BUFFER_DESC ib{}; ib.ByteWidth = static_cast<UINT>(indices.size() * sizeof(uint32_t)); ib.Usage = D3D11_USAGE_IMMUTABLE; ib.BindFlags = D3D11_BIND_INDEX_BUFFER;
    D3D11_SUBRESOURCE_DATA id{indices.data()};
    if (FAILED(device_->CreateBuffer(&ib, &id, &gpu->indexBuffer))) { warning = "GPU index upload failed: " + file.string(); return {}; }
    gpu->indexCount = static_cast<uint32_t>(indices.size());
    gpu->triangles = indices.size() / 3;
    gpu->boundsMin = bmin; gpu->boundsMax = bmax;
    meshCache_.insert_or_assign(key, gpu);
    {
        std::ostringstream msg;
        msg << "Mesh imported: " << Log::PathUtf8(file)
            << " vertices=" << vertices.size() << " triangles=" << gpu->triangles
            << " boundsMin=(" << bmin.x << "," << bmin.y << "," << bmin.z << ")"
            << " boundsMax=(" << bmax.x << "," << bmax.y << "," << bmax.z << ")";
        Log::Debug(msg.str());
    }
    return gpu;
}

std::shared_ptr<SceneRenderer::TextureGpu> SceneRenderer::LoadTexture(const std::filesystem::path& file, std::string& warning) {
    const std::string key = PathKey(file);
    if (const auto it = textureCache_.find(key); it != textureCache_.end()) return it->second;
    if (!std::filesystem::exists(file)) {
        warning = "Missing texture: " + Log::PathUtf8(file);
        Log::Warning(warning);
        textureCache_.insert_or_assign(key, fallbackTexture_);
        return fallbackTexture_;
    }

    ComPtr<IWICBitmapDecoder> decoder;
    HRESULT hr = wic_->CreateDecoderFromFilename(file.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder);
    if (FAILED(hr)) {
        warning = "WIC could not decode: " + Log::PathUtf8(file);
        Log::Warning(warning);
        textureCache_.insert_or_assign(key, fallbackTexture_);
        return fallbackTexture_;
    }
    ComPtr<IWICBitmapFrameDecode> frame;
    if (FAILED(decoder->GetFrame(0, &frame))) {
        warning = "WIC could not read frame: " + file.string();
        textureCache_.insert_or_assign(key, fallbackTexture_);
        return fallbackTexture_;
    }
    UINT width = 0, height = 0; frame->GetSize(&width, &height);
    if (!width || !height) {
        warning = "Texture has zero dimensions: " + file.string();
        textureCache_.insert_or_assign(key, fallbackTexture_);
        return fallbackTexture_;
    }
    ComPtr<IWICFormatConverter> converter;
    if (FAILED(wic_->CreateFormatConverter(&converter)) ||
        FAILED(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom))) {
        warning = "WIC pixel conversion failed: " + file.string();
        textureCache_.insert_or_assign(key, fallbackTexture_);
        return fallbackTexture_;
    }

    const UINT stride = width * 4;
    std::vector<uint8_t> pixels(static_cast<size_t>(stride) * height);
    if (FAILED(converter->CopyPixels(nullptr, stride, static_cast<UINT>(pixels.size()), pixels.data()))) {
        warning = "WIC pixel copy failed: " + file.string();
        textureCache_.insert_or_assign(key, fallbackTexture_);
        return fallbackTexture_;
    }

    D3D11_TEXTURE2D_DESC td{};
    td.Width = width; td.Height = height; td.MipLevels = td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_IMMUTABLE;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA data{pixels.data(), stride, 0};
    ComPtr<ID3D11Texture2D> texture;
    if (FAILED(device_->CreateTexture2D(&td, &data, &texture))) {
        warning = "GPU texture upload failed: " + file.string();
        textureCache_.insert_or_assign(key, fallbackTexture_);
        return fallbackTexture_;
    }
    auto gpu = std::make_shared<TextureGpu>(); gpu->width = width; gpu->height = height;
    if (FAILED(device_->CreateShaderResourceView(texture.Get(), nullptr, &gpu->srv))) {
        warning = "GPU texture view failed: " + file.string();
        textureCache_.insert_or_assign(key, fallbackTexture_);
        return fallbackTexture_;
    }
    textureCache_.insert_or_assign(key, gpu);
    Log::Debug("Texture loaded: " + Log::PathUtf8(file) + " size=" + std::to_string(width) + "x" + std::to_string(height));
    return gpu;
}

std::shared_ptr<SceneRenderer::TextureGpu> SceneRenderer::SolidTexture(const XMFLOAT4& color) {
    const auto byte = [](float v) -> uint32_t {
        return static_cast<uint32_t>(std::clamp(std::isfinite(v) ? v : 1.0f,0.0f,1.0f)*255.0f+0.5f);
    };
    const uint32_t pixel=byte(color.x)|(byte(color.y)<<8)|(byte(color.z)<<16)|(byte(color.w)<<24);
    const std::string key="material-color:"+std::to_string(pixel);
    if (const auto it=textureCache_.find(key);it!=textureCache_.end()) return it->second;
    D3D11_TEXTURE2D_DESC td{};
    td.Width=td.Height=td.MipLevels=td.ArraySize=1;
    td.Format=DXGI_FORMAT_R8G8B8A8_UNORM;td.SampleDesc.Count=1;
    td.Usage=D3D11_USAGE_IMMUTABLE;td.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA data{&pixel,4,0};
    ComPtr<ID3D11Texture2D> texture;
    auto gpu=std::make_shared<TextureGpu>();
    if (FAILED(device_->CreateTexture2D(&td,&data,&texture)) ||
        FAILED(device_->CreateShaderResourceView(texture.Get(),nullptr,&gpu->srv))) {
        Log::Warning("Cannot upload material color");return fallbackTexture_;
    }
    textureCache_.insert_or_assign(key,gpu);return gpu;
}

std::shared_ptr<SceneRenderer::TextureGpu> SceneRenderer::ResolveTexture(const AssetDefinition& asset, const std::string& variant, const std::filesystem::path& materialTexture, std::string& warning) {
    if (asset.textures.empty()) {
        if (!materialTexture.empty()) return LoadTexture(materialTexture, warning);
        return {}; // Untextured material: use its diffuse color.
    }
    const TextureVariant* chosen = nullptr;
    if (!variant.empty()) {
        const std::string wanted = Lower(variant);
        for (const auto& t : asset.textures) if (Lower(t.name) == wanted) { chosen = &t; break; }
    }
    if (!chosen) chosen = &asset.textures.front();
    return LoadTexture(chosen->diffuse, warning);
}

SceneRenderer::InstanceData SceneRenderer::MakeInstance(const PropInstance& prop) {
    const XMMATRIX world = LegacyTransform::World(prop.position, prop.rotation);
    XMFLOAT4X4 m{}; XMStoreFloat4x4(&m, world);
    return {{m._11,m._12,m._13,m._14}, {m._21,m._22,m._23,m._24}, {m._31,m._32,m._33,m._34}, {m._41,m._42,m._43,m._44}};
}

void SceneRenderer::ReleasePreviewResources() {
    ClearScene();
    meshCache_.clear();
    textureCache_.clear();
}

void SceneRenderer::ClearScene() {
    meshBatches_.clear(); spriteBatches_.clear(); functionalModelBatches_ = {}; ghostItems_.clear(); pickProxies_.clear(); propBindings_.clear(); selectedProps_.clear(); selectedProp_ = -1; stats_ = {}; lastWarning_.clear(); hasBounds_ = false;
    debugVertices_.clear(); functionalPadBuffer_.Reset(); functionalPadVertices_=functionalFlagVertices_=functionalPointVertices_=0; functionalSpawnStarts_.fill(0);functionalSpawnCounts_.fill(0); functionalGhostActive_=false; boundsVertexCount_ = 0; gameplayVertexCount_ = 0; zoneVertexCount_ = 0; debugVertexBuffer_.Reset();
    collisionFaceBuffer_.Reset(); collisionEdgeBuffer_.Reset(); collisionFaceVertices_=collisionEdgeVertices_=0; collisionPreviewCounts_.fill(0);
    nativeLights_.clear();
}

bool SceneRenderer::BuildScene(const MapDocument& map, const AssetRegistry& assets, std::string& error, bool frameScene) {
    const auto started = std::chrono::steady_clock::now();
    Log::Info("Scene build begin. props=" + std::to_string(map.Props().size()) + " libraryRoot=" + Log::PathUtf8(assets.Root()));
    ClearScene();
    nativeLights_=map.Lights();
    stats_.sourceProps = map.Props().size();
    propBindings_.resize(map.Props().size());

    std::unordered_map<std::string, size_t> meshLookup;
    std::unordered_map<std::string, size_t> spriteLookup;
    size_t warningsStored = 0;

    for (size_t propIndex = 0; propIndex < map.Props().size(); ++propIndex) {
        const auto& prop = map.Props()[propIndex];
        const AssetDefinition* asset = assets.Find(prop.library, prop.group, prop.name);
        if (!asset) {
            ++stats_.missingAssets;
            Log::Warning("Unresolved prop[" + std::to_string(propIndex) + "]: " + prop.library + " / " + prop.group + " / " + prop.name);
            continue;
        }
        ++stats_.resolvedProps;
        {
            const auto internal = LegacyTransform::Position(prop.position);
            std::ostringstream msg;
            msg << "Prop[" << propIndex << "] " << prop.library << "/" << prop.group << "/" << prop.name
                << " tex=\"" << prop.texture << "\" legacyPos=(" << prop.position.x << "," << prop.position.y << "," << prop.position.z << ")"
                << " internalPos=(" << internal.x << "," << internal.y << "," << internal.z << ")"
                << " legacyRot=(" << prop.rotation.x << "," << prop.rotation.y << "," << prop.rotation.z << ")";
            Log::Debug(msg.str());
        }
        std::string warning;

        if (!asset->mesh.empty()) {
            auto mesh = LoadMesh(asset->mesh, warning);
            if (!mesh) { ++stats_.missingAssets; continue; }
            std::vector<std::shared_ptr<TextureGpu>> textures;
            std::string key = PathKey(asset->mesh);
            for (const auto& part : mesh->parts) {
                auto texture = ResolveTexture(*asset, prop.texture, part.diffuse, warning);
                textures.push_back(texture ? texture : SolidTexture(part.color));
                key += "\x1f" + std::to_string(reinterpret_cast<uintptr_t>(textures.back().get()));
            }
            size_t index;
            if (const auto it = meshLookup.find(key); it != meshLookup.end()) index = it->second;
            else { index = meshBatches_.size(); meshLookup.emplace(key, index); meshBatches_.push_back({mesh, std::move(textures)}); }
            const size_t instanceIndex = meshBatches_[index].instances.size();
            meshBatches_[index].instances.push_back(MakeInstance(prop));
            const XMMATRIX world = LegacyTransform::World(prop.position, prop.rotation);
            UpdateSceneBounds(*mesh, world);
            XMFLOAT3 pickMin{}, pickMax{};
            WorldBounds(*mesh, world, pickMin, pickMax);
            const size_t proxyIndex = pickProxies_.size();
            pickProxies_.push_back({static_cast<int>(propIndex), pickMin, pickMax});
            propBindings_[propIndex] = {PropBinding::Kind::Mesh, index, instanceIndex, proxyIndex};
            ++stats_.meshInstances;
        } else if (!asset->sprite.empty()) {
            auto texture = LoadTexture(asset->sprite, warning);
            const std::string key = PathKey(asset->sprite);
            size_t index;
            if (const auto it = spriteLookup.find(key); it != spriteLookup.end()) index = it->second;
            else { index = spriteBatches_.size(); spriteLookup.emplace(key, index); spriteBatches_.push_back({texture ? texture : fallbackTexture_}); }
            const XMFLOAT3 pos = LegacyTransform::Position(prop.position);
            const size_t instanceIndex = spriteBatches_[index].instances.size();
            spriteBatches_[index].instances.push_back({pos, asset->spriteScale, asset->spriteOriginY, {0,0,0}});
            // Conservative sprite bounds using decoded image size.
            const float hw = 0.5f * float(texture ? texture->width : 1) * asset->spriteScale;
            const float h = float(texture ? texture->height : 1) * asset->spriteScale;
            const XMFLOAT3 lo{pos.x-hw, pos.y-h*0.05f, pos.z-hw};
            const XMFLOAT3 hi{pos.x+hw, pos.y+h,       pos.z+hw};
            if (!hasBounds_) { boundsMin_=lo; boundsMax_=hi; hasBounds_=true; } else { boundsMin_=Min3(boundsMin_,lo); boundsMax_=Max3(boundsMax_,hi); }
            const size_t proxyIndex = pickProxies_.size();
            pickProxies_.push_back({static_cast<int>(propIndex), lo, hi});
            propBindings_[propIndex] = {PropBinding::Kind::Sprite, index, instanceIndex, proxyIndex};
            ++stats_.spriteInstances;
        }

        if (!warning.empty()) { Log::Warning(warning); if (warningsStored < 12) { AppendWarning(lastWarning_, warning); ++warningsStored; } }
    }

    BuildFunctionalModels(map);

    auto uploadInstances = [&](auto& batches) {
        for (auto& batch : batches) {
            if (batch.instances.empty()) continue;
            using T = typename std::decay_t<decltype(batch.instances)>::value_type;
            D3D11_BUFFER_DESC desc{};
            desc.ByteWidth = static_cast<UINT>(batch.instances.size() * sizeof(T));
            desc.Usage = D3D11_USAGE_DYNAMIC;
            desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            D3D11_SUBRESOURCE_DATA data{batch.instances.data()};
            if (FAILED(device_->CreateBuffer(&desc, &data, &batch.instanceBuffer))) return false;
        }
        return true;
    };
    if (!uploadInstances(meshBatches_) || !uploadInstances(spriteBatches_) || !uploadInstances(functionalModelBatches_)) {
        error = "Could not upload scene instance buffers to the GPU.";
        Log::Error(error);
        ClearScene();
        return false;
    }

    BuildFunctionalPads(map);
    BuildDebugGeometry(map);
    if (!UploadDebugGeometry()) {
        Log::Warning("Gameplay debug geometry could not be uploaded; map geometry remains usable.");
    }
    BuildCollisionGeometry(map); // GPU-only diagnostic; no changes to the map or physics.

    stats_.meshBatches = meshBatches_.size();
    stats_.spriteBatches = spriteBatches_.size();
    stats_.gpuMeshes = meshCache_.size();
    stats_.gpuTextures = textureCache_.size();
    stats_.drawCalls = stats_.spriteBatches;
    for (const auto& b : meshBatches_) stats_.drawCalls += b.mesh->parts.size();
    for (const auto& b : meshBatches_) stats_.triangles += b.mesh->triangles * b.instances.size();
    for (const auto& b : functionalModelBatches_) if (b.mesh && !b.instances.empty()) {
        stats_.drawCalls += b.mesh->parts.size();
        stats_.triangles += b.mesh->triangles * b.instances.size();
    }
    stats_.buildMilliseconds = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();

    {
        std::ostringstream msg;
        msg << "Scene build complete. resolved=" << stats_.resolvedProps << " missing=" << stats_.missingAssets
            << " meshInstances=" << stats_.meshInstances << " spriteInstances=" << stats_.spriteInstances
            << " batches=" << stats_.drawCalls << " triangles=" << stats_.triangles
            << " buildMs=" << stats_.buildMilliseconds;
        if (hasBounds_) msg << " sceneBoundsMin=(" << boundsMin_.x << "," << boundsMin_.y << "," << boundsMin_.z << ")"
                           << " sceneBoundsMax=(" << boundsMax_.x << "," << boundsMax_.y << "," << boundsMax_.z << ")";
        Log::Info(msg.str());
    }
    if (frameScene) FrameScene();
    error.clear();
    return true;
}

void SceneRenderer::ApplyDraftPreviewTint(const std::array<float,3>& tint) {
    for(auto& batch:meshBatches_) {
        for(size_t i=0;i<batch.mesh->parts.size() && i<batch.textures.size();++i) {
            const auto& part=batch.mesh->parts[i];
            if (!part.diffuse.empty()) continue; // existing texture is not altered
            const XMFLOAT4 color{part.color.x*tint[0],part.color.y*tint[1],part.color.z*tint[2],part.color.w};
            batch.textures[i]=SolidTexture(color);
        }
    }
}

bool SceneRenderer::BuildAssetPreview(const AssetDefinition& asset, const std::string& textureVariant, std::string& error) {
    ClearScene();
    previewMeshDetached_=false;
    stats_.sourceProps = 1;
    stats_.resolvedProps = 1;
    std::string warning;
    PropInstance prop;
    prop.library = asset.library;
    prop.group = asset.group;
    prop.name = asset.name;
    prop.texture = textureVariant;
    prop.position = {0,0,0};
    prop.rotation = {0,0,0};
    propBindings_.resize(1);

    if (!asset.mesh.empty()) {
        auto mesh = LoadMesh(asset.mesh, warning);
        if (!mesh) { error = warning.empty() ? "Could not load preview mesh." : warning; return false; }
        std::vector<std::shared_ptr<TextureGpu>> textures;
        for (const auto& part : mesh->parts) {
            auto texture = ResolveTexture(asset, textureVariant, part.diffuse, warning);
            textures.push_back(texture ? texture : SolidTexture(part.color));
        }
        meshBatches_.push_back({mesh, std::move(textures)});
        meshBatches_[0].instances.push_back(MakeInstance(prop));
        const XMMATRIX world = LegacyTransform::World(prop.position, prop.rotation);
        UpdateSceneBounds(*mesh, world);
        XMFLOAT3 pickMin{}, pickMax{};
        WorldBounds(*mesh, world, pickMin, pickMax);
        pickProxies_.push_back({0, pickMin, pickMax});
        propBindings_[0] = {PropBinding::Kind::Mesh, 0, 0, 0};
        ++stats_.meshInstances;
        stats_.triangles = mesh->triangles;
    } else if (!asset.sprite.empty()) {
        auto texture = LoadTexture(asset.sprite, warning);
        spriteBatches_.push_back({texture ? texture : fallbackTexture_});
        const XMFLOAT3 pos{0,0,0};
        spriteBatches_[0].instances.push_back({pos, asset.spriteScale, asset.spriteOriginY, {0,0,0}});
        const float hw = 0.5f * float(texture ? texture->width : 1) * asset.spriteScale;
        const float h = float(texture ? texture->height : 1) * asset.spriteScale;
        boundsMin_ = {-hw, -h*0.05f, -hw};
        boundsMax_ = { hw, h, hw};
        hasBounds_ = true;
        pickProxies_.push_back({0, boundsMin_, boundsMax_});
        propBindings_[0] = {PropBinding::Kind::Sprite, 0, 0, 0};
        ++stats_.spriteInstances;
    } else {
        error = "Selected legacy asset has no mesh or sprite source.";
        return false;
    }

    auto upload = [&](auto& batches) {
        for (auto& batch : batches) {
            if (batch.instances.empty()) continue;
            using T = typename std::decay_t<decltype(batch.instances)>::value_type;
            D3D11_BUFFER_DESC desc{};
            desc.ByteWidth = static_cast<UINT>(batch.instances.size() * sizeof(T));
            desc.Usage = D3D11_USAGE_DYNAMIC;
            desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            D3D11_SUBRESOURCE_DATA data{batch.instances.data()};
            if (FAILED(device_->CreateBuffer(&desc, &data, &batch.instanceBuffer))) return false;
        }
        return true;
    };
    if (!upload(meshBatches_) || !upload(spriteBatches_)) { error = "Could not upload asset preview to the GPU."; ClearScene(); return false; }
    stats_.meshBatches = meshBatches_.size();
    stats_.spriteBatches = spriteBatches_.size();
    stats_.drawCalls = stats_.spriteBatches;
    for (const auto& b : meshBatches_) stats_.drawCalls += b.mesh->parts.size();
    stats_.gpuMeshes = meshCache_.size();
    stats_.gpuTextures = textureCache_.size();
    if (!warning.empty()) lastWarning_ = warning;
    FrameScene();
    // Preview is intentionally a little closer than the full-scene framing.
    cameraDistance_ *= 0.82f;
    error.clear();
    return true;
}

void SceneRenderer::BuildCollisionGeometry(const MapDocument& map) {
    collisionFaceBuffer_.Reset(); collisionEdgeBuffer_.Reset();
    collisionFaceVertices_=collisionEdgeVertices_=0;
    collisionPreviewCounts_.fill(0);
    const auto preview=CollisionPreview::Build(map);
    collisionPreviewCounts_={preview.planes,preview.boxes,preview.triangles,preview.omitted};
    // CollisionPreview::Vertex and DebugVertex are the same GPU layout, but upload
    // the original vertex type so no reinterpret cast or ABI assumption is needed.
    auto upload=[&](const std::vector<CollisionPreview::Vertex>& vertices,
                    Microsoft::WRL::ComPtr<ID3D11Buffer>& gpu,uint32_t& count) {
        if(vertices.empty())return;
        if(vertices.size()>UINT32_MAX/sizeof(CollisionPreview::Vertex)) {
            Log::Warning("Collision preview exceeds the D3D11 buffer size limit.");return;
        }
        D3D11_BUFFER_DESC desc{};
        desc.ByteWidth=static_cast<UINT>(vertices.size()*sizeof(CollisionPreview::Vertex));
        desc.Usage=D3D11_USAGE_IMMUTABLE;desc.BindFlags=D3D11_BIND_VERTEX_BUFFER;
        D3D11_SUBRESOURCE_DATA data{vertices.data()};
        if(SUCCEEDED(device_->CreateBuffer(&desc,&data,&gpu)))
            count=static_cast<uint32_t>(vertices.size());
        else Log::Warning("Collision preview GPU upload failed; original map remains unaffected.");
    };
    upload(preview.faces,collisionFaceBuffer_,collisionFaceVertices_);
    upload(preview.edges,collisionEdgeBuffer_,collisionEdgeVertices_);
    Log::Info("Native collision preview generated. planes="+std::to_string(preview.planes)+
              " boxes="+std::to_string(preview.boxes)+" triangles="+std::to_string(preview.triangles)+
              " omitted="+std::to_string(preview.omitted));
}

void SceneRenderer::BuildDebugGeometry(const MapDocument& map) {
    debugVertices_.clear(); gameplayRanges_.clear(); boundsVertexCount_ = 0; gameplayVertexCount_ = 0; zoneVertexCount_ = 0;
    auto addLine = [&](const XMFLOAT3& a, const XMFLOAT3& b, const XMFLOAT4& color) {
        debugVertices_.push_back({a,color});
        debugVertices_.push_back({b,color});
    };
    auto addBox = [&](XMFLOAT3 lo, XMFLOAT3 hi, const XMFLOAT4& color) {
        const XMFLOAT3 c[8] = {
            {lo.x,lo.y,lo.z},{hi.x,lo.y,lo.z},{hi.x,hi.y,lo.z},{lo.x,hi.y,lo.z},
            {lo.x,lo.y,hi.z},{hi.x,lo.y,hi.z},{hi.x,hi.y,hi.z},{lo.x,hi.y,hi.z}
        };
        const int e[12][2]={{0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}};
        for (const auto& edge : e) addLine(c[edge[0]], c[edge[1]], color);
    };
    auto beginRange = [&] { return static_cast<uint32_t>(debugVertices_.size()); };
    auto finishRange = [&](uint32_t begin, uint32_t layer, uint32_t modes) {
        const uint32_t count = static_cast<uint32_t>(debugVertices_.size()) - begin;
        if (count) gameplayRanges_.push_back({begin,count,layer,modes});
    };
    auto modeOf = [](const std::string& value) -> uint32_t {
        const auto lower = Lower(value);
        if (lower == "dm") return 1u;
        if (lower == "tdm") return 2u;
        if (lower == "ctf") return 4u;
        if (lower == "dom" || lower == "cp" || lower == "ctp") return 8u;
        if (lower == "red" || lower == "blue") return 2u | 4u;
        return 15u;
    };
    for (const auto& proxy : pickProxies_) addBox(proxy.boundsMin, proxy.boundsMax, XMFLOAT4{0.30f,0.57f,0.78f,0.28f});
    boundsVertexCount_ = static_cast<uint32_t>(debugVertices_.size());

    // Compact, editor-only tank/spawn outline. No additional geometry is serialized to XML.
    for (const auto& spawn : map.Spawns()) {
        const uint32_t start = beginRange();
        const XMFLOAT3 p = LegacyTransform::Position(spawn.position);
        const bool red = Lower(spawn.team) == "red" || Lower(spawn.type) == "red";
        const bool blue = Lower(spawn.team) == "blue" || Lower(spawn.type) == "blue";
        const XMFLOAT4 c = red ? XMFLOAT4{1.0f,0.38f,0.36f,0.74f} : blue ? XMFLOAT4{0.35f,0.67f,1.0f,0.74f} : XMFLOAT4{0.42f,0.9f,0.65f,0.68f};
        addBox({p.x-90,p.y+15,p.z-120},{p.x+90,p.y+95,p.z+120},c);
        const float angle = spawn.rotationZ;
        addLine({p.x,p.y+100,p.z},{p.x+std::cos(angle)*180,p.y+100,p.z-std::sin(angle)*180},c);
        finishRange(start,1u,modeOf(spawn.type));
    }
    for (const auto& flag : map.CtfFlags()) {
        // Keep wireframe fallback only if a real pedestal asset was not packaged.
        const size_t modelIndex=Lower(flag.team)=="red"?0u:1u;
        if (functionalModelBatches_[modelIndex].mesh) continue;
        const uint32_t start = beginRange();
        const XMFLOAT3 p = LegacyTransform::Position(flag.position);
        const bool red = Lower(flag.team) == "red";
        const XMFLOAT4 color = red ? XMFLOAT4{0.95f,0.25f,0.22f,0.88f} : XMFLOAT4{0.22f,0.55f,1.0f,0.88f};
        addBox({p.x-72,p.y,p.z-72},{p.x+72,p.y+35,p.z+72},color);
        const XMFLOAT3 a{p.x,p.y+35,p.z}, top{p.x,p.y+420,p.z};
        addLine(a,top,color);
        // Two-panel cloth outline, with a simple inset fold.
        const XMFLOAT3 b{p.x+250,p.y+365,p.z}, c{p.x+210,p.y+230,p.z}, d{p.x,p.y+245,p.z};
        addLine(top,b,color); addLine(b,c,color); addLine(c,d,color); addLine(d,top,color);
        addLine(top,c,color);
        finishRange(start,2u,4u);
    }
    for (const auto& point : map.ControlPoints()) {
        if (functionalModelBatches_[2].mesh) continue;
        const uint32_t start = beginRange();
        const XMFLOAT3 p = LegacyTransform::Position(point.position);
        const XMFLOAT4 c{0.9f,0.75f,0.31f,0.84f};
        const float radius = std::clamp(point.distance * 0.15f, 90.0f, 350.0f);
        for (int i=0;i<16;++i) {
            const float a=static_cast<float>(i)*6.283185307f/16.0f, b=static_cast<float>(i+1)*6.283185307f/16.0f;
            addLine({p.x+radius*std::cos(a),p.y+12,p.z+radius*std::sin(a)},
                    {p.x+radius*std::cos(b),p.y+12,p.z+radius*std::sin(b)},c);
        }
        addLine(p,{p.x,p.y+270,p.z},c);
        finishRange(start,4u,8u);
    }
    for (const auto& bonus : map.Bonuses()) {
        const uint32_t start = beginRange();
        const auto a=LegacyTransform::Position(bonus.min), b=LegacyTransform::Position(bonus.max);
        addBox(Min3(a,b), Max3(a,b), XMFLOAT4{0.5f,0.8f,0.56f,0.42f});
        uint32_t modes = 0;
        for (const auto& m : bonus.modes) modes |= modeOf(m);
        finishRange(start,8u,modes); // An unspecified mode list is not confirmed to mean every mode.
    }
    gameplayVertexCount_ = static_cast<uint32_t>(debugVertices_.size()) - boundsVertexCount_;
    for (const auto& box : map.SpecialBoxes()) {
        const uint32_t start = beginRange();
        const auto a=LegacyTransform::Position(box.min), b=LegacyTransform::Position(box.max);
        const std::string action = Lower(box.action);
        const XMFLOAT4 color = action == "kick" ? XMFLOAT4{1.0f,0.63f,0.18f,0.50f} : XMFLOAT4{1.0f,0.22f,0.20f,0.46f};
        addBox(Min3(a,b),Max3(a,b),color);
        finishRange(start,16u,15u);
    }
    zoneVertexCount_ = static_cast<uint32_t>(debugVertices_.size()) - boundsVertexCount_ - gameplayVertexCount_;
    // Read-only light visualization. Radius is symbolic: native attenuationEnd
    // is not a verified world-space radius. No fake glow or extra XML geometry.
    for(const auto& light:map.Lights()) {
        const uint32_t start=beginRange();
        const auto p=LegacyTransform::Position(light.position);
        const XMFLOAT4 c{((light.color>>16)&255)/255.f,((light.color>>8)&255)/255.f,
                         (light.color&255)/255.f,0.95f};
        constexpr float marker=85.f;
        addLine({p.x-marker,p.y,p.z},{p.x+marker,p.y,p.z},c);
        addLine({p.x,p.y-marker,p.z},{p.x,p.y+marker,p.z},c);
        addLine({p.x,p.y,p.z-marker},{p.x,p.y,p.z+marker},c);
        for(int i=0;i<16;++i) {
            const float a=i*6.283185307f/16.f,b=(i+1)*6.283185307f/16.f;
            addLine({p.x+marker*std::cos(a),p.y,p.z+marker*std::sin(a)},
                    {p.x+marker*std::cos(b),p.y,p.z+marker*std::sin(b)},c);
        }
        finishRange(start,32u,15u);
    }
}

bool SceneRenderer::UploadDebugGeometry() {
    debugVertexBuffer_.Reset();
    if (debugVertices_.empty()) return true;
    D3D11_BUFFER_DESC desc{};
    desc.ByteWidth = static_cast<UINT>(debugVertices_.size() * sizeof(DebugVertex));
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA data{debugVertices_.data()};
    return SUCCEEDED(device_->CreateBuffer(&desc, &data, &debugVertexBuffer_));
}

void SceneRenderer::WorldBounds(const MeshGpu& mesh, const XMMATRIX& world, XMFLOAT3& outMin, XMFLOAT3& outMax) {
    const XMFLOAT3 lo = mesh.boundsMin, hi = mesh.boundsMax;
    const XMFLOAT3 corners[8] = {
        {lo.x,lo.y,lo.z},{hi.x,lo.y,lo.z},{lo.x,hi.y,lo.z},{hi.x,hi.y,lo.z},
        {lo.x,lo.y,hi.z},{hi.x,lo.y,hi.z},{lo.x,hi.y,hi.z},{hi.x,hi.y,hi.z}
    };
    bool first = true;
    for (const auto& c : corners) {
        XMVECTOR p = XMVector3TransformCoord(XMLoadFloat3(&c), world);
        XMFLOAT3 out{}; XMStoreFloat3(&out, p);
        if (first) { outMin = outMax = out; first = false; }
        else { outMin = Min3(outMin, out); outMax = Max3(outMax, out); }
    }
}

void SceneRenderer::UpdateSceneBounds(const MeshGpu& mesh, const XMMATRIX& world) {
    XMFLOAT3 lo{}, hi{};
    WorldBounds(mesh, world, lo, hi);
    if (!hasBounds_) { boundsMin_ = lo; boundsMax_ = hi; hasBounds_ = true; }
    else { boundsMin_ = Min3(boundsMin_, lo); boundsMax_ = Max3(boundsMax_, hi); }
}

XMVECTOR SceneRenderer::CameraEye() const {
    const float cp = std::cos(cameraPitch_);
    XMFLOAT3 offset{
        cp * std::sin(cameraYaw_) * cameraDistance_,
        std::sin(cameraPitch_) * cameraDistance_,
        cp * std::cos(cameraYaw_) * cameraDistance_};
    return XMVectorSet(cameraTarget_.x + offset.x, cameraTarget_.y + offset.y, cameraTarget_.z + offset.z, 1.0f);
}

void SceneRenderer::FrameScene() {
    cameraFocusAnimating_=false;
    if (!hasBounds_) { cameraTarget_ = {0,0,0}; cameraDistance_ = 15000.0f; return; }
    cameraTarget_ = {(boundsMin_.x+boundsMax_.x)*0.5f, (boundsMin_.y+boundsMax_.y)*0.5f, (boundsMin_.z+boundsMax_.z)*0.5f};
    const float dx=boundsMax_.x-boundsMin_.x, dy=boundsMax_.y-boundsMin_.y, dz=boundsMax_.z-boundsMin_.z;
    const float radius = std::max(500.0f, 0.5f * std::sqrt(dx*dx+dy*dy+dz*dz));
    cameraDistance_ = radius * 1.75f;
}

void SceneRenderer::ReverseViewDirection() {
    cameraFocusAnimating_=false;
    cameraYaw_=std::remainder(cameraYaw_+DirectX::XM_PI,DirectX::XM_2PI);
}

void SceneRenderer::ResetReferenceViewDirection() {
    cameraFocusAnimating_=false;
    cameraYaw_=2.39159265f;
    cameraPitch_=0.55f;
}

void SceneRenderer::Orbit(float dxPixels, float dyPixels) {
    cameraFocusAnimating_=false;
    cameraYaw_ -= dxPixels * 0.006f;
    cameraPitch_ += dyPixels * 0.005f;
    cameraPitch_ = std::clamp(cameraPitch_, -1.45f, 1.45f);
}

void SceneRenderer::Pan(float dxPixels, float dyPixels) {
    cameraFocusAnimating_=false;
    const XMVECTOR eye = CameraEye();
    const XMVECTOR target = XMLoadFloat3(&cameraTarget_);
    const XMVECTOR forward = XMVector3Normalize(target - eye);
    const XMVECTOR worldUp = XMVectorSet(0,1,0,0);
    const XMVECTOR right = XMVector3Normalize(XMVector3Cross(worldUp, forward));
    const XMVECTOR up = XMVector3Normalize(XMVector3Cross(forward, right));
    const float factor = cameraDistance_ * 0.00115f;
    XMVECTOR moved = target + right * (-dxPixels * factor) + up * (dyPixels * factor);
    XMStoreFloat3(&cameraTarget_, moved);
}

void SceneRenderer::Zoom(float wheelDelta, float speed) {
    if (wheelDelta == 0.0f) return;
    speed = std::clamp(speed, 0.15f, 4.0f);
    cameraDistance_ *= std::pow(0.86f, wheelDelta * speed);
    cameraDistance_ = std::clamp(cameraDistance_, 25.0f, 500000.0f);
}

void SceneRenderer::FrameSelection() {
    cameraFocusAnimating_=false;
    if (selectedProp_ < 0) { FrameScene(); return; }
    const auto it = std::find_if(pickProxies_.begin(), pickProxies_.end(), [&](const PickProxy& p){ return p.propIndex == selectedProp_; });
    if (it == pickProxies_.end()) { FrameScene(); return; }
    cameraTarget_ = {(it->boundsMin.x+it->boundsMax.x)*0.5f, (it->boundsMin.y+it->boundsMax.y)*0.5f, (it->boundsMin.z+it->boundsMax.z)*0.5f};
    const float dx=it->boundsMax.x-it->boundsMin.x, dy=it->boundsMax.y-it->boundsMin.y, dz=it->boundsMax.z-it->boundsMin.z;
    const float radius = std::max(75.0f, 0.5f * std::sqrt(dx*dx+dy*dy+dz*dz));
    cameraDistance_ = std::clamp(radius * 2.25f, 150.0f, 500000.0f);
}

void SceneRenderer::FocusLegacyPoint(const XMFLOAT3& point, float distance) {
    cameraFocusAnimating_=false;
    cameraTarget_ = LegacyTransform::Position(point);
    cameraDistance_ = std::clamp(distance, 150.0f, 500000.0f);
}

void SceneRenderer::FocusSelectionKeepDistance(bool smooth) {
    if (selectedProp_ < 0) return;
    const auto it = std::find_if(pickProxies_.begin(), pickProxies_.end(), [&](const PickProxy& p){ return p.propIndex == selectedProp_; });
    if (it == pickProxies_.end()) return;
    const XMFLOAT3 goal{(it->boundsMin.x+it->boundsMax.x)*0.5f, (it->boundsMin.y+it->boundsMax.y)*0.5f, (it->boundsMin.z+it->boundsMax.z)*0.5f};
    cameraFocusGoal_=goal;
    cameraFocusAnimating_=smooth;
    if(!smooth)cameraTarget_=goal;
}

void SceneRenderer::AdvanceCameraFocus(float dt) {
    if(!cameraFocusAnimating_)return;
    const float t=1.0f-std::exp(-std::clamp(dt,0.f,0.1f)*11.f);
    cameraTarget_.x+=(cameraFocusGoal_.x-cameraTarget_.x)*t;
    cameraTarget_.y+=(cameraFocusGoal_.y-cameraTarget_.y)*t;
    cameraTarget_.z+=(cameraFocusGoal_.z-cameraTarget_.z)*t;
    if(std::fabs(cameraTarget_.x-cameraFocusGoal_.x)+std::fabs(cameraTarget_.y-cameraFocusGoal_.y)+
       std::fabs(cameraTarget_.z-cameraFocusGoal_.z)<0.05f) {
        cameraTarget_=cameraFocusGoal_;cameraFocusAnimating_=false;
    }
}

XMFLOAT3 SceneRenderer::CameraTargetLegacy() const {
    return LegacyTransform::ToLegacyPosition(cameraTarget_);
}

int SceneRenderer::Pick(float xPixels, float yPixels) const {
    if (!width_ || !height_ || pickProxies_.empty()) return -1;
    XMFLOAT3 o{}, d{};
    if (!ScreenRay(xPixels, yPixels, o, d)) return -1;

    auto rayBox = [&](const PickProxy& b, float& hit)->bool {
        float tmin = 0.0f, tmax = 1.0e30f;
        const float oo[3] = {o.x,o.y,o.z}, dd[3] = {d.x,d.y,d.z};
        const float mn[3] = {b.boundsMin.x,b.boundsMin.y,b.boundsMin.z}, mx[3] = {b.boundsMax.x,b.boundsMax.y,b.boundsMax.z};
        for (int axis=0; axis<3; ++axis) {
            if (std::fabs(dd[axis]) < 1.0e-7f) { if (oo[axis] < mn[axis] || oo[axis] > mx[axis]) return false; continue; }
            float a = (mn[axis]-oo[axis]) / dd[axis], c = (mx[axis]-oo[axis]) / dd[axis];
            if (a > c) std::swap(a,c);
            tmin = std::max(tmin,a); tmax = std::min(tmax,c);
            if (tmax < tmin) return false;
        }
        hit = tmin; return true;
    };

    int best = -1; float bestT = 1.0e30f;
    for (const auto& proxy : pickProxies_) {
        float t{}; if (rayBox(proxy,t) && t < bestT) { bestT=t; best=proxy.propIndex; }
    }
    return best;
}


template<class T>
bool SceneRenderer::UploadDynamicInstances(const std::vector<T>& instances, ID3D11Buffer* buffer) {
    if (!buffer || instances.empty()) return false;
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(context_->Map(buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) return false;
    std::memcpy(mapped.pData, instances.data(), instances.size() * sizeof(T));
    context_->Unmap(buffer, 0);
    return true;
}

void SceneRenderer::RecomputeSceneBoundsFromProxies() {
    hasBounds_ = false;
    for (const auto& p : pickProxies_) {
        if (!hasBounds_) { boundsMin_ = p.boundsMin; boundsMax_ = p.boundsMax; hasBounds_ = true; }
        else { boundsMin_ = Min3(boundsMin_, p.boundsMin); boundsMax_ = Max3(boundsMax_, p.boundsMax); }
    }
}

bool SceneRenderer::UpdatePropTransform(int propIndex, const PropInstance& prop) {
    if (propIndex < 0 || static_cast<size_t>(propIndex) >= propBindings_.size()) return false;
    const auto& binding = propBindings_[static_cast<size_t>(propIndex)];
    if (binding.kind == PropBinding::Kind::None || binding.proxy >= pickProxies_.size()) return false;

    if (binding.kind == PropBinding::Kind::Mesh) {
        if (binding.batch >= meshBatches_.size()) return false;
        auto& batch = meshBatches_[binding.batch];
        if (binding.instance >= batch.instances.size() || !batch.mesh) return false;
        batch.instances[binding.instance] = MakeInstance(prop);
        if (!UploadDynamicInstances(batch.instances, batch.instanceBuffer.Get())) return false;
        const XMMATRIX world = LegacyTransform::World(prop.position, prop.rotation);
        WorldBounds(*batch.mesh, world, pickProxies_[binding.proxy].boundsMin, pickProxies_[binding.proxy].boundsMax);
    } else {
        if (binding.batch >= spriteBatches_.size()) return false;
        auto& batch = spriteBatches_[binding.batch];
        if (binding.instance >= batch.instances.size()) return false;
        auto& instance = batch.instances[binding.instance];
        instance.position = LegacyTransform::Position(prop.position);
        if (!UploadDynamicInstances(batch.instances, batch.instanceBuffer.Get())) return false;
        const float hw = 0.5f * float(batch.texture ? batch.texture->width : 1) * instance.scale;
        const float h = float(batch.texture ? batch.texture->height : 1) * instance.scale;
        const XMFLOAT3 pos = instance.position;
        pickProxies_[binding.proxy].boundsMin = {pos.x-hw, pos.y-h*0.05f, pos.z-hw};
        pickProxies_[binding.proxy].boundsMax = {pos.x+hw, pos.y+h, pos.z+hw};
    }
    RecomputeSceneBoundsFromProxies();
    return true;
}

bool SceneRenderer::ScreenRay(float xPixels, float yPixels, XMFLOAT3& origin, XMFLOAT3& direction) const {
    if (!width_ || !height_) return false;
    const XMVECTOR eye = CameraEye();
    const XMVECTOR target = XMLoadFloat3(&cameraTarget_);
    const XMVECTOR up = XMVectorSet(0,1,0,0);
    const XMMATRIX view = XMMatrixLookAtLH(eye, target, up);
    const XMMATRIX projection = XMMatrixPerspectiveFovLH(XMConvertToRadians(58.0f), static_cast<float>(width_)/static_cast<float>(height_), 5.0f, 500000.0f);
    const XMMATRIX identity = XMMatrixIdentity();
    const XMVECTOR nearP = XMVector3Unproject(XMVectorSet(xPixels, yPixels, 0.0f, 1.0f), 0,0,static_cast<float>(width_),static_cast<float>(height_),0,1, projection, view, identity);
    const XMVECTOR farP  = XMVector3Unproject(XMVectorSet(xPixels, yPixels, 1.0f, 1.0f), 0,0,static_cast<float>(width_),static_cast<float>(height_),0,1, projection, view, identity);
    XMStoreFloat3(&origin, nearP);
    XMStoreFloat3(&direction, XMVector3Normalize(farP-nearP));
    return true;
}

bool SceneRenderer::ScreenToLegacyPlane(float xPixels, float yPixels, float legacyZ, XMFLOAT3& outLegacy) const {
    XMFLOAT3 o{}, d{};
    if (!ScreenRay(xPixels, yPixels, o, d)) return false;
    // Legacy Z is internal Y in the Assimp-compatible basis.
    if (std::fabs(d.y) < 1.0e-6f) return false;
    const float t = (legacyZ - o.y) / d.y;
    if (t < 0.0f) return false;
    const XMFLOAT3 internal{o.x+d.x*t, o.y+d.y*t, o.z+d.z*t};
    outLegacy = LegacyTransform::ToLegacyPosition(internal);
    return true;
}

void SceneRenderer::Render(bool showGrid, bool showBounds, bool showGameplay, bool showZones,
                           unsigned overlayMask, unsigned modeMask, bool collisionView, bool showLights, bool previewNativeLighting) {
    if (!context_ || !colorRtv_ || !depthDsv_ || width_ == 0 || height_ == 0) return;

    ID3D11RenderTargetView* renderTarget = colorRtv_.Get();
    context_->OMSetRenderTargets(1, &renderTarget, depthDsv_.Get());
    const float clear[] = {backgroundColor_.x,backgroundColor_.y,backgroundColor_.z,1.0f};
    context_->ClearRenderTargetView(colorRtv_.Get(), clear);
    context_->ClearDepthStencilView(depthDsv_.Get(), D3D11_CLEAR_DEPTH|D3D11_CLEAR_STENCIL, 1.0f, 0);
    D3D11_VIEWPORT vp{0,0,static_cast<float>(width_),static_cast<float>(height_),0,1};
    context_->RSSetViewports(1, &vp);
    context_->RSSetState(rasterizer_.Get());

    const XMVECTOR eye = CameraEye();
    const XMVECTOR target = XMLoadFloat3(&cameraTarget_);
    const XMVECTOR worldUp = XMVectorSet(0,1,0,0);
    const XMMATRIX view = XMMatrixLookAtLH(eye, target, worldUp);
    const float aspect = static_cast<float>(width_) / static_cast<float>(height_);
    const XMMATRIX projection = XMMatrixPerspectiveFovLH(XMConvertToRadians(58.0f), aspect, 5.0f, 500000.0f);
    const XMMATRIX viewProjection = view * projection;

    const XMVECTOR forward = XMVector3Normalize(target - eye);
    const XMVECTOR right = XMVector3Normalize(XMVector3Cross(worldUp, forward));
    const XMVECTOR up = XMVector3Normalize(XMVector3Cross(forward, right));
    CameraConstants camera{};
    XMStoreFloat4x4(&camera.viewProjection, viewProjection);
    XMStoreFloat4(&camera.cameraRight, right);
    XMStoreFloat4(&camera.cameraUp, up);
    camera.lightDirection = {-0.38f,-0.82f,0.42f,static_cast<float>(effectMode_)};
    context_->UpdateSubresource(cameraBuffer_.Get(), 0, nullptr, &camera, 0, 0);
    // Mesh and sprite PIXEL shaders read lightDirection.w for Effects. Historically
    // b0 was bound only to VS, so every effect mode was invisible in PS.
    ID3D11Buffer* pixelCamera = cameraBuffer_.Get();
    context_->PSSetConstantBuffers(0, 1, &pixelCamera);
    NativeLightConstants nativePreview{};
    if (previewNativeLighting && !collisionView && nativeLightBuffer_) {
        // Sort by camera focus, not by authoring order: an open map may contain
        // hundreds of native lights; only eight local ones reach the shader.
        std::array<std::pair<float,size_t>,8> nearest{};
        for(auto& item:nearest)item.first=std::numeric_limits<float>::max();
        for(size_t i=0;i<nativeLights_.size();++i) {
            const auto& l=nativeLights_[i];
            if(l.type!="omni" || l.intensity<=0.f || l.attenuationEnd<=l.attenuationBegin)continue;
            const auto v=LegacyTransform::Position(l.position);
            const float dx=v.x-cameraTarget_.x,dy=v.y-cameraTarget_.y,dz=v.z-cameraTarget_.z;
            const float dist2=dx*dx+dy*dy+dz*dz;
            if(dist2>=nearest.back().first)continue;
            size_t k=nearest.size()-1;
            while(k>0 && dist2<nearest[k-1].first){nearest[k]=nearest[k-1];--k;}
            nearest[k]={dist2,i};
        }
        for(const auto& item:nearest) {
            if(item.first==std::numeric_limits<float>::max())break;
            const auto& l=nativeLights_[item.second];
            const auto pos=LegacyTransform::Position(l.position);
            const int k=static_cast<int>(nativePreview.count.x++);
            nativePreview.positionsAndIntensity[k]={pos.x,pos.y,pos.z,std::min(l.intensity,5.f)};
            nativePreview.colorsAndBegin[k]={((l.color>>16)&255)/255.f,((l.color>>8)&255)/255.f,(l.color&255)/255.f,l.attenuationBegin*previewLightScale_};
            nativePreview.endsAndReserved[k]={l.attenuationEnd*previewLightScale_,0,0,0};
        }
    }
    if(nativeLightBuffer_) {
        context_->UpdateSubresource(nativeLightBuffer_.Get(),0,nullptr,&nativePreview,0,0);
        ID3D11Buffer* lightCb=nativeLightBuffer_.Get();
        context_->PSSetConstantBuffers(1,1,&lightCb);
    }

    if (showGrid) RenderGrid(viewProjection);
    if (collisionView) {
        // Replacement visual mode: only the authored physics in <collision-geometry>.
        // Green=near-horizontal surface; red=steep surface or box. This does NOT
        // classify gameplay flags, materials, or whether a surface is passable.
        RenderCollisionGeometry();
    } else {
        RenderMeshes(camera);
        RenderFunctionalModels(overlayMask,modeMask);
        RenderSprites(camera);
        RenderGhost();
        RenderFunctionalPads(overlayMask,modeMask);
        RenderFunctionalGhost();
        RenderDebugOverlay(showBounds, showGameplay, showZones, overlayMask, modeMask, showLights);
        RenderSelection();
    }

    ID3D11ShaderResourceView* nullSrv = nullptr;
    context_->PSSetShaderResources(0, 1, &nullSrv);
    context_->VSSetShaderResources(0, 1, &nullSrv);
}

void SceneRenderer::RenderCollisionGeometry() {
    ID3D11Buffer* cb=cameraBuffer_.Get();
    context_->VSSetConstantBuffers(0,1,&cb);
    context_->IASetInputLayout(gridLayout_.Get());
    context_->VSSetShader(gridVs_.Get(),nullptr,0);
    context_->PSSetShader(gridPs_.Get(),nullptr,0);
    const float blendFactor[4]{};
    UINT stride=sizeof(CollisionPreview::Vertex),offset=0;
    if(collisionFaceBuffer_ && collisionFaceVertices_) {
        ID3D11Buffer* vb=collisionFaceBuffer_.Get();
        context_->IASetVertexBuffers(0,1,&vb,&stride,&offset);
        context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context_->OMSetDepthStencilState(depthState_.Get(),0);
        context_->OMSetBlendState(opaqueBlend_.Get(),blendFactor,0xFFFFFFFFu);
        context_->Draw(collisionFaceVertices_,0);
    }
    if(collisionEdgeBuffer_ && collisionEdgeVertices_) {
        ID3D11Buffer* vb=collisionEdgeBuffer_.Get();
        context_->IASetVertexBuffers(0,1,&vb,&stride,&offset);
        context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
        // LESS_EQUAL makes borders on the face itself visible.
        context_->OMSetDepthStencilState(spriteDepthState_.Get(),0);
        context_->OMSetBlendState(alphaBlend_.Get(),blendFactor,0xFFFFFFFFu);
        context_->Draw(collisionEdgeVertices_,0);
    }
}

void SceneRenderer::RenderGrid(const XMMATRIX&) {
    if (!gridVertexBuffer_ || !gridVertexCount_) return;
    UINT stride = sizeof(GridVertex), offset = 0;
    ID3D11Buffer* vb = gridVertexBuffer_.Get();
    context_->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
    context_->IASetInputLayout(gridLayout_.Get());
    context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    context_->VSSetShader(gridVs_.Get(), nullptr, 0); context_->PSSetShader(gridPs_.Get(), nullptr, 0);
    ID3D11Buffer* cb = cameraBuffer_.Get(); context_->VSSetConstantBuffers(0, 1, &cb);
    context_->OMSetDepthStencilState(spriteDepthState_.Get(), 0);
    const float blendFactor[4]{}; context_->OMSetBlendState(alphaBlend_.Get(), blendFactor, 0xFFFFFFFFu);
    context_->Draw(gridVertexCount_, 0);
}

void SceneRenderer::RenderMeshes(const CameraConstants&) {
    context_->IASetInputLayout(meshLayout_.Get());
    context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context_->VSSetShader(meshVs_.Get(), nullptr, 0); context_->PSSetShader(meshPs_.Get(), nullptr, 0);
    ID3D11Buffer* cb = cameraBuffer_.Get(); context_->VSSetConstantBuffers(0, 1, &cb);
    ID3D11SamplerState* samp = sampler_.Get(); context_->PSSetSamplers(0, 1, &samp);
    context_->OMSetDepthStencilState(depthState_.Get(), 0);
    const float blendFactor[4]{}; context_->OMSetBlendState(opaqueBlend_.Get(), blendFactor, 0xFFFFFFFFu);

    for (const auto& batch : meshBatches_) {
        if (!batch.mesh || !batch.instanceBuffer || batch.instances.empty()) continue;
        ID3D11Buffer* buffers[2] = {batch.mesh->vertexBuffer.Get(), batch.instanceBuffer.Get()};
        const UINT strides[2] = {sizeof(Vertex), sizeof(InstanceData)};
        const UINT offsets[2] = {0,0};
        context_->IASetVertexBuffers(0, 2, buffers, strides, offsets);
        context_->IASetIndexBuffer(batch.mesh->indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
        for (size_t i=0; i<batch.mesh->parts.size(); ++i) {
            const auto& part = batch.mesh->parts[i];
            context_->RSSetState(part.oppositeFaceAtlas?rasterizerPairedAtlas_.Get():rasterizer_.Get());
            ID3D11ShaderResourceView* srv = batch.textures[i]->srv.Get();
            context_->PSSetShaderResources(0, 1, &srv);
            context_->DrawIndexedInstanced(part.indexCount, static_cast<UINT>(batch.instances.size()), part.firstIndex, 0, 0);
        }
    }
    context_->RSSetState(rasterizer_.Get());
}

void SceneRenderer::RenderSprites(const CameraConstants&) {
    if (spriteBatches_.empty()) return;
    context_->IASetInputLayout(spriteLayout_.Get());
    context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context_->IASetIndexBuffer(spriteIndexBuffer_.Get(), DXGI_FORMAT_R16_UINT, 0);
    context_->VSSetShader(spriteVs_.Get(), nullptr, 0); context_->PSSetShader(spritePs_.Get(), nullptr, 0);
    ID3D11Buffer* cb = cameraBuffer_.Get(); context_->VSSetConstantBuffers(0, 1, &cb);
    ID3D11SamplerState* samp = spriteSampler_.Get(); context_->PSSetSamplers(0, 1, &samp);
    context_->OMSetDepthStencilState(spriteDepthState_.Get(), 0);
    const float blendFactor[4]{}; context_->OMSetBlendState(alphaBlend_.Get(), blendFactor, 0xFFFFFFFFu);

    for (const auto& batch : spriteBatches_) {
        if (!batch.instanceBuffer || batch.instances.empty()) continue;
        ID3D11Buffer* buffers[2] = {spriteVertexBuffer_.Get(), batch.instanceBuffer.Get()};
        const UINT strides[2] = {sizeof(SpriteVertex), sizeof(SpriteInstanceData)};
        const UINT offsets[2] = {0,0};
        context_->IASetVertexBuffers(0, 2, buffers, strides, offsets);
        ID3D11ShaderResourceView* srv = batch.texture ? batch.texture->srv.Get() : fallbackTexture_->srv.Get();
        // Sprite VS calls Texture2D.GetDimensions; binding only the PS resource
        // yields zero/undefined quad dimensions even though instances are indexed.
        context_->VSSetShaderResources(0, 1, &srv);
        context_->PSSetShaderResources(0, 1, &srv);
        context_->DrawIndexedInstanced(6, static_cast<UINT>(batch.instances.size()), 0, 0, 0);
    }
}


void SceneRenderer::RenderDebugOverlay(bool showBounds, bool showGameplay, bool showZones, unsigned overlayMask, unsigned modeMask, bool showLights) {
    if (!debugVertexBuffer_ || (!showBounds && !showGameplay && !showZones && !showLights)) return;
    UINT stride = sizeof(DebugVertex), offset = 0;
    ID3D11Buffer* vb = debugVertexBuffer_.Get();
    context_->IASetVertexBuffers(0,1,&vb,&stride,&offset);
    context_->IASetInputLayout(gridLayout_.Get());
    context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    context_->VSSetShader(gridVs_.Get(),nullptr,0); context_->PSSetShader(gridPs_.Get(),nullptr,0);
    ID3D11Buffer* cb = cameraBuffer_.Get(); context_->VSSetConstantBuffers(0,1,&cb);
    context_->OMSetDepthStencilState(depthState_.Get(),0);
    const float blendFactor[4]{}; context_->OMSetBlendState(alphaBlend_.Get(),blendFactor,0xFFFFFFFFu);
    if (showBounds && boundsVertexCount_) context_->Draw(boundsVertexCount_, 0);
    for (const auto& range : gameplayRanges_) {
        const bool enabled = range.layer == 32u ? showLights : range.layer == 16u ? showZones : showGameplay;
        if (enabled && ((overlayMask & range.layer) || range.layer==32u) && ((modeMask & range.modes) || range.layer==32u || (range.layer==8u && range.modes==0u && modeMask==15u)))
            context_->Draw(range.count, range.start);
    }
}

void SceneRenderer::RenderSelection() {
    if (selectedProps_.empty() || !selectionVertexBuffer_) return;
    for (int selected : selectedProps_) {
    const auto it = std::find_if(pickProxies_.begin(), pickProxies_.end(), [&](const PickProxy& p){ return p.propIndex == selected; });
    if (it == pickProxies_.end()) continue;
    const auto& lo=it->boundsMin; const auto& hi=it->boundsMax;
    const XMFLOAT3 c[8] = {
        {lo.x,lo.y,lo.z},{hi.x,lo.y,lo.z},{hi.x,hi.y,lo.z},{lo.x,hi.y,lo.z},
        {lo.x,lo.y,hi.z},{hi.x,lo.y,hi.z},{hi.x,hi.y,hi.z},{lo.x,hi.y,hi.z}
    };
    const int e[12][2] = {{0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}};
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(context_->Map(selectionVertexBuffer_.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&mapped))) return;
    auto* v = static_cast<GridVertex*>(mapped.pData);
    const XMFLOAT4 color{0.32f,0.66f,0.95f,0.95f};
    for (int i=0;i<12;++i) { v[i*2]={c[e[i][0]],color}; v[i*2+1]={c[e[i][1]],color}; }
    context_->Unmap(selectionVertexBuffer_.Get(),0);
    UINT stride=sizeof(GridVertex), offset=0; ID3D11Buffer* vb=selectionVertexBuffer_.Get();
    context_->IASetVertexBuffers(0,1,&vb,&stride,&offset); context_->IASetInputLayout(gridLayout_.Get());
    context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    context_->VSSetShader(gridVs_.Get(),nullptr,0); context_->PSSetShader(gridPs_.Get(),nullptr,0);
    ID3D11Buffer* cb=cameraBuffer_.Get(); context_->VSSetConstantBuffers(0,1,&cb);
    context_->OMSetDepthStencilState(depthState_.Get(),0); const float blendFactor[4]{}; context_->OMSetBlendState(alphaBlend_.Get(),blendFactor,0xFFFFFFFFu);
    context_->Draw(24,0);
    }
}


void SceneRenderer::MoveCameraLegacy(const XMFLOAT3& displacement) {
    cameraFocusAnimating_=false;
    const auto render=LegacyTransform::Position(displacement);
    cameraTarget_.x+=render.x; cameraTarget_.y+=render.y; cameraTarget_.z+=render.z;
}

void SceneRenderer::CameraMoveBasisLegacy(XMFLOAT3& rightOut, XMFLOAT3& forwardOut) const {
    const XMVECTOR eye = CameraEye();
    const XMVECTOR target = XMLoadFloat3(&cameraTarget_);
    XMVECTOR forward = target - eye;
    // Horizontal movement is independent of camera pitch.
    forward = XMVectorSet(XMVectorGetX(forward), 0.0f, XMVectorGetZ(forward), 0.0f);
    if (XMVectorGetX(XMVector3LengthSq(forward)) < 0.0001f) forward = XMVectorSet(0,0,1,0);
    forward = XMVector3Normalize(forward);
    const XMVECTOR right = XMVector3Normalize(XMVector3Cross(XMVectorSet(0,1,0,0), forward));
    XMFLOAT3 f{}, r{}; XMStoreFloat3(&f, forward); XMStoreFloat3(&r, right);
    forwardOut = LegacyTransform::ToLegacyPosition(f);
    rightOut = LegacyTransform::ToLegacyPosition(r);
}

std::vector<int> SceneRenderer::SelectInScreenRect(float x0, float y0, float x1, float y1) const {
    std::vector<int> result;
    if (!width_ || !height_) return result;
    const float left=std::min(x0,x1), right=std::max(x0,x1), top=std::min(y0,y1), bottom=std::max(y0,y1);
    const XMMATRIX view=XMMatrixLookAtLH(CameraEye(), XMLoadFloat3(&cameraTarget_), XMVectorSet(0,1,0,0));
    const XMMATRIX projection=XMMatrixPerspectiveFovLH(XMConvertToRadians(58.0f),float(width_)/float(height_),5.0f,500000.0f);
    for (const auto& proxy: pickProxies_) {
        if (proxy.propIndex<0) continue;
        const auto& a=proxy.boundsMin; const auto& b=proxy.boundsMax;
        const XMFLOAT3 points[8]={{a.x,a.y,a.z},{a.x,a.y,b.z},{a.x,b.y,a.z},{a.x,b.y,b.z},
                                   {b.x,a.y,a.z},{b.x,a.y,b.z},{b.x,b.y,a.z},{b.x,b.y,b.z}};
        float sx=std::numeric_limits<float>::max(), sy=sx, ex=-sx, ey=-sx;
        int visible=0;
        for (const auto& p:points) {
            const XMVECTOR projected=XMVector3Project(XMLoadFloat3(&p),0,0,float(width_),float(height_),0,1,projection,view,XMMatrixIdentity());
            const float z=XMVectorGetZ(projected);
            if (z<0 || z>1) continue;
            const float x=XMVectorGetX(projected), y=XMVectorGetY(projected);
            sx=std::min(sx,x); sy=std::min(sy,y); ex=std::max(ex,x); ey=std::max(ey,y); ++visible;
        }
        if (visible && sx<=right && ex>=left && sy<=bottom && ey>=top) result.push_back(proxy.propIndex);
    }
    return result;
}

bool SceneRenderer::SuggestEdgeSnap(const std::vector<PropInstance>& props,float tolerance,
                                    float clearance,float& legacyDx,float& legacyDy) const {
    legacyDx=0.f;legacyDy=0.f;
    if(props.size()!=1 || ghostItems_.size()!=1 || !ghostItems_[0].mesh ||
       pickProxies_.empty())return false;
    const auto& prop=props[0];
    // Horizontal world AABB is exact for grid-aligned rectangles (including
    // translated pivots). Non-orthogonal objects are not eligible; an AABB is
    // not a true rotated polygon edge.
    const float quarterTurn=DirectX::XM_PIDIV2;
    if(std::fabs(std::remainder(prop.rotation.z,quarterTurn))>0.0001f)return false;
    DirectX::XMFLOAT3 lo{},hi{};
    WorldBounds(*ghostItems_[0].mesh,LegacyTransform::World(prop.position,prop.rotation),lo,hi);
    const GeometrySnap::Rect moving{lo.x,hi.x,lo.z,hi.z,lo.y};
    std::vector<GeometrySnap::Rect> others;others.reserve(pickProxies_.size());
    for(const auto& fixed:pickProxies_) {
        if(fixed.propIndex<0)continue;
        const auto& a=fixed.boundsMin;const auto& b=fixed.boundsMax;
        others.push_back({a.x,b.x,a.z,b.z,a.y});
    }
    const auto snap=GeometrySnap::Find(moving,others,tolerance,clearance);
    legacyDx=snap.x; legacyDy=-snap.y; // internal Z = -legacy Y
    return snap.xMatched||snap.yMatched;
}

void SceneRenderer::SetGhost(const std::vector<PropInstance>& props, const AssetRegistry& assets) {
    if (props.empty()) { ghostItems_.clear(); return; }
    std::vector<std::string> keys;
    keys.reserve(props.size());
    for (const auto& p:props) keys.push_back(p.library+"\x1f"+p.group+"\x1f"+p.name+"\x1f"+p.texture);
    bool rebuild=ghostItems_.size()!=props.size();
    if (!rebuild) for (size_t i=0;i<keys.size();++i) if (ghostItems_[i].identity!=keys[i]) { rebuild=true; break; }
    if (rebuild) {
        ghostItems_.clear(); ghostItems_.reserve(props.size());
        for (size_t i=0;i<props.size();++i) {
            GhostItem item{}; item.identity=keys[i];
            const AssetDefinition* asset=assets.Find(props[i].library,props[i].group,props[i].name);
            std::string warning;
            if (asset) {
                if (!asset->mesh.empty()) {
                    item.mesh=LoadMesh(asset->mesh,warning);
                    if (item.mesh) for (const auto& part:item.mesh->parts) {
                        auto t=ResolveTexture(*asset,props[i].texture,part.diffuse,warning);
                        item.textures.push_back(t?t:SolidTexture(part.color));
                    }
                } else if (!asset->sprite.empty()) {
                    item.sprite=LoadTexture(asset->sprite,warning);
                    item.spriteScale=asset->spriteScale; item.spriteOriginY=asset->spriteOriginY;
                }
            }
            if (item.mesh || item.sprite) {
                D3D11_BUFFER_DESC desc{};
                desc.ByteWidth = item.mesh ? sizeof(InstanceData) : sizeof(SpriteInstanceData);
                desc.Usage=D3D11_USAGE_DYNAMIC; desc.BindFlags=D3D11_BIND_VERTEX_BUFFER;
                desc.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
                if (FAILED(device_->CreateBuffer(&desc,nullptr,&item.instanceBuffer))) item.instanceBuffer.Reset();
            }
            ghostItems_.push_back(std::move(item));
        }
    }
    for (size_t i=0;i<props.size();++i) {
        auto& item=ghostItems_[i]; if (!item.instanceBuffer) continue;
        D3D11_MAPPED_SUBRESOURCE mapped{};
        if (FAILED(context_->Map(item.instanceBuffer.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&mapped))) continue;
        if (item.mesh) {
            const InstanceData instance=MakeInstance(props[i]);
            std::memcpy(mapped.pData,&instance,sizeof(instance));
        } else {
            const SpriteInstanceData instance{LegacyTransform::Position(props[i].position),item.spriteScale,item.spriteOriginY,{0,0,0}};
            std::memcpy(mapped.pData,&instance,sizeof(instance));
        }
        context_->Unmap(item.instanceBuffer.Get(),0);
    }
}

void SceneRenderer::RenderGhost() {
    if (ghostItems_.empty()) return;
    const float blendFactor[4]{};
    context_->OMSetBlendState(alphaBlend_.Get(),blendFactor,0xFFFFFFFFu);
    context_->OMSetDepthStencilState(spriteDepthState_.Get(),0);
    ID3D11Buffer* cb=cameraBuffer_.Get(); context_->VSSetConstantBuffers(0,1,&cb);
    for (const auto& item:ghostItems_) {
        if (!item.instanceBuffer) continue;
        if (item.mesh) {
            context_->IASetInputLayout(meshLayout_.Get());
            context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            context_->VSSetShader(meshVs_.Get(),nullptr,0); context_->PSSetShader(ghostMeshPs_.Get(),nullptr,0);
            ID3D11SamplerState* sampler=sampler_.Get(); context_->PSSetSamplers(0,1,&sampler);
            ID3D11Buffer* buffers[2]={item.mesh->vertexBuffer.Get(),item.instanceBuffer.Get()};
            const UINT strides[2]={sizeof(Vertex),sizeof(InstanceData)}, offsets[2]={0,0};
            context_->IASetVertexBuffers(0,2,buffers,strides,offsets);
            context_->IASetIndexBuffer(item.mesh->indexBuffer.Get(),DXGI_FORMAT_R32_UINT,0);
            for (size_t i=0;i<item.mesh->parts.size();++i) {
                const auto& part=item.mesh->parts[i];
                context_->RSSetState(part.oppositeFaceAtlas?rasterizerPairedAtlas_.Get():rasterizer_.Get());
                ID3D11ShaderResourceView* srv=item.textures[i]->srv.Get();
                context_->PSSetShaderResources(0,1,&srv);
                context_->DrawIndexedInstanced(part.indexCount,1,part.firstIndex,0,0);
            }
            context_->RSSetState(rasterizer_.Get());
        } else if (item.sprite) {
            context_->IASetInputLayout(spriteLayout_.Get());
            context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            context_->VSSetShader(spriteVs_.Get(),nullptr,0); context_->PSSetShader(ghostSpritePs_.Get(),nullptr,0);
            ID3D11SamplerState* sampler=spriteSampler_.Get(); context_->PSSetSamplers(0,1,&sampler);
            ID3D11Buffer* buffers[2]={spriteVertexBuffer_.Get(),item.instanceBuffer.Get()};
            const UINT strides[2]={sizeof(SpriteVertex),sizeof(SpriteInstanceData)}, offsets[2]={0,0};
            context_->IASetVertexBuffers(0,2,buffers,strides,offsets);
            context_->IASetIndexBuffer(spriteIndexBuffer_.Get(),DXGI_FORMAT_R16_UINT,0);
            ID3D11ShaderResourceView* srv=item.sprite->srv.Get();
            context_->VSSetShaderResources(0,1,&srv); context_->PSSetShaderResources(0,1,&srv);
            context_->DrawIndexedInstanced(6,1,0,0,0);
        }
    }
}

bool SceneRenderer::ProjectLegacy(const XMFLOAT3& point, float& x, float& y) const {
    return ProjectWorld(LegacyTransform::Position(point),x,y);
}

bool SceneRenderer::ProjectWorld(const XMFLOAT3& point, float& x, float& y) const {
    if (!width_ || !height_) return false;
    const XMMATRIX view=XMMatrixLookAtLH(CameraEye(), XMLoadFloat3(&cameraTarget_),XMVectorSet(0,1,0,0));
    const XMMATRIX projection=XMMatrixPerspectiveFovLH(XMConvertToRadians(58.0f),float(width_)/float(height_),5.0f,500000.0f);
    const XMVECTOR projected=XMVector3Project(XMLoadFloat3(&point),0,0,float(width_),float(height_),0,1,projection,view,XMMatrixIdentity());
    const float depth=XMVectorGetZ(projected);
    if(depth<0 || depth>1) return false;
    x=XMVectorGetX(projected); y=XMVectorGetY(projected); return true;
}

bool SceneRenderer::ScreenToWorldViewPlane(float x,float y,const XMFLOAT3& anchor,XMFLOAT3& out) const {
    if(!width_ || !height_)return false;
    const XMVECTOR eye=CameraEye(),target=XMLoadFloat3(&cameraTarget_);
    const XMMATRIX view=XMMatrixLookAtLH(eye,target,XMVectorSet(0,1,0,0));
    const XMMATRIX projection=XMMatrixPerspectiveFovLH(XMConvertToRadians(58.f),float(width_)/float(height_),5.f,500000.f);
    const XMVECTOR nearPoint=XMVector3Unproject(XMVectorSet(x,y,0.f,1.f),0,0,float(width_),float(height_),0,1,projection,view,XMMatrixIdentity());
    const XMVECTOR farPoint=XMVector3Unproject(XMVectorSet(x,y,1.f,1.f),0,0,float(width_),float(height_),0,1,projection,view,XMMatrixIdentity());
    const XMVECTOR ray=XMVector3Normalize(XMVectorSubtract(farPoint,nearPoint));
    const XMVECTOR normal=XMVector3Normalize(XMVectorSubtract(target,eye));
    const float denominator=XMVectorGetX(XMVector3Dot(ray,normal));
    if(std::abs(denominator)<1e-6f)return false;
    const float t=XMVectorGetX(XMVector3Dot(XMVectorSubtract(XMLoadFloat3(&anchor),nearPoint),normal))/denominator;
    if(!std::isfinite(t) || t<0.f)return false;
    XMStoreFloat3(&out,XMVectorMultiplyAdd(ray,XMVectorReplicate(t),nearPoint));
    return std::isfinite(out.x)&&std::isfinite(out.y)&&std::isfinite(out.z);
}

bool SceneRenderer::UpdatePreviewMeshGeometry(const std::vector<XMFLOAT3>& points,
                                            const std::vector<uint32_t>& triangles) {
    if(!device_ || !context_ || meshBatches_.size()!=1 || !meshBatches_[0].mesh ||
       points.empty() || points.size()>200000 || triangles.empty() || triangles.size()>600000 || triangles.size()%3!=0)return false;
    for(const auto& v:points)
        if(!std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.z)||
           std::abs(v.x)>1000000.f||std::abs(v.y)>1000000.f||std::abs(v.z)>1000000.f)return false;
    for(const auto i:triangles)if(i>=points.size())return false;
    auto& mesh=meshBatches_[0].mesh;
    // Detach from the path-keyed cache: authoring never changes original assets.
    if(!previewMeshDetached_) {
        mesh=std::make_shared<MeshGpu>(*mesh);
        mesh->vertexBuffer.Reset();mesh->indexBuffer.Reset();
        previewMeshDetached_=true;
    }
    const auto previousVertexCount=mesh->cpuVertices.size();
    if(previousVertexCount!=points.size()) {
        const Vertex example=mesh->cpuVertices.back();
        mesh->cpuVertices.resize(points.size(),example);
        mesh->vertexBuffer.Reset();
    }
    for(size_t i=0;i<points.size();++i)mesh->cpuVertices[i].position=points[i];
    for(auto& part:mesh->parts)part.oppositeFaceAtlas=false; // edited topology is not the original paired atlas
    if(!mesh->vertexBuffer) {
        D3D11_BUFFER_DESC desc{};desc.ByteWidth=static_cast<UINT>(mesh->cpuVertices.size()*sizeof(Vertex));
        desc.BindFlags=D3D11_BIND_VERTEX_BUFFER;desc.Usage=D3D11_USAGE_DYNAMIC;desc.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
        D3D11_SUBRESOURCE_DATA data{mesh->cpuVertices.data()};
        Microsoft::WRL::ComPtr<ID3D11Buffer> updated;
        if(FAILED(device_->CreateBuffer(&desc,&data,&updated)))return false;
        mesh->vertexBuffer=std::move(updated);
    } else {
        D3D11_MAPPED_SUBRESOURCE mapped{};
        if(FAILED(context_->Map(mesh->vertexBuffer.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&mapped)))return false;
        std::memcpy(mapped.pData,mesh->cpuVertices.data(),mesh->cpuVertices.size()*sizeof(Vertex));
        context_->Unmap(mesh->vertexBuffer.Get(),0);
    }
    const auto oldIndices=mesh->cpuIndices.size();
    if(triangles.size()!=oldIndices)mesh->indexBuffer.Reset();
    mesh->cpuIndices=triangles;
    if(!mesh->indexBuffer) {
        D3D11_BUFFER_DESC desc{};desc.ByteWidth=static_cast<UINT>(triangles.size()*sizeof(uint32_t));
        desc.BindFlags=D3D11_BIND_INDEX_BUFFER;desc.Usage=D3D11_USAGE_DYNAMIC;desc.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
        D3D11_SUBRESOURCE_DATA data{triangles.data()};
        Microsoft::WRL::ComPtr<ID3D11Buffer> updated;
        if(FAILED(device_->CreateBuffer(&desc,&data,&updated)))return false;
        mesh->indexBuffer=std::move(updated);
    } else {
        D3D11_MAPPED_SUBRESOURCE mapped{};
        if(FAILED(context_->Map(mesh->indexBuffer.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&mapped)))return false;
        std::memcpy(mapped.pData,triangles.data(),triangles.size()*sizeof(uint32_t));
        context_->Unmap(mesh->indexBuffer.Get(),0);
    }
    if(!mesh->parts.empty()) {
        // Original material ranges are retained when faces are appended. If the user
        // removes too many faces to reach the final material's first triangle,
        // collapse the preview into one valid draw range instead of UINT underflow.
        if(triangles.size()<mesh->parts.back().firstIndex) {
            mesh->parts.resize(1);
            meshBatches_[0].textures.resize(1);
            mesh->parts[0].firstIndex=0;
            mesh->parts[0].indexCount=static_cast<uint32_t>(triangles.size());
        } else {
            mesh->parts.back().indexCount=static_cast<uint32_t>(triangles.size()-mesh->parts.back().firstIndex);
        }
    }
    mesh->indexCount=static_cast<uint32_t>(triangles.size());
    mesh->triangles=triangles.size()/3;stats_.triangles=mesh->triangles;
    auto lo=points.front(),hi=points.front();
    for(const auto& v:points){lo=Min3(lo,v);hi=Max3(hi,v);}
    mesh->boundsMin=lo;mesh->boundsMax=hi;
    boundsMin_=lo;boundsMax_=hi;hasBounds_=true;
    return true;
}

void SceneRenderer::SetFunctionalGhost(const XMFLOAT3& position, unsigned kind, bool active) {
    functionalGhostPosition_=position; functionalGhostKind_=kind; functionalGhostActive_=active;
}


void SceneRenderer::BuildFunctionalModels(const MapDocument& map) {
    // Models and texture atlases supplied with the user's ProTLVK archive.
    // Package them next to the executable. No fake static prop is written to XML.
    wchar_t module[32768]{};
    const DWORD len=GetModuleFileNameW(nullptr,module,ARRAYSIZE(module));
    std::filesystem::path root;
    if (len && len<ARRAYSIZE(module)) root=std::filesystem::path(module).parent_path()/L"assets"/L"functional";
    if (!std::filesystem::is_directory(root)) root=std::filesystem::current_path()/L"assets"/L"functional";
    struct Resource { const wchar_t* folder; const wchar_t* texture; };
    const Resource sources[3]={{L"ctf_red",L"fs_red.jpg"},{L"ctf_blue",L"fs_blue.jpg"},{L"cp",L"pedestal.jpg"}};
    for (size_t i=0;i<3;++i) {
        const auto folder=root/sources[i].folder;
        std::string warning;
        auto mesh=LoadMesh(folder/L"object.3ds",warning);
        if (!mesh) {if (!warning.empty()) Log::Warning("Functional model: "+warning);continue;}
        auto texture=LoadTexture(folder/sources[i].texture,warning);
        auto& batch=functionalModelBatches_[i];batch.mesh=mesh;
        for (const auto& part:mesh->parts) batch.textures.push_back(texture?texture:SolidTexture(part.color));
        if (i<2) {
            const std::string team=i==0?"red":"blue";
            for (const auto& flag:map.CtfFlags()) if (Lower(flag.team)==team) {
                PropInstance visual;visual.position=flag.position;
                batch.instances.push_back(MakeInstance(visual));
            }
        } else {
            for (const auto& point:map.ControlPoints()) {
                PropInstance visual;visual.position=point.position;
                batch.instances.push_back(MakeInstance(visual));
            }
        }
        if (!warning.empty()) Log::Warning("Functional texture: "+warning);
    }
}

void SceneRenderer::RenderFunctionalModels(unsigned overlayMask,unsigned modeMask) {
    if (!(overlayMask & (2u|4u))) return;
    context_->IASetInputLayout(meshLayout_.Get());
    context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context_->VSSetShader(meshVs_.Get(),nullptr,0);context_->PSSetShader(meshPs_.Get(),nullptr,0);
    ID3D11Buffer* cb=cameraBuffer_.Get();context_->VSSetConstantBuffers(0,1,&cb);
    ID3D11SamplerState* sample=sampler_.Get();context_->PSSetSamplers(0,1,&sample);
    context_->OMSetDepthStencilState(depthState_.Get(),0);
    const float blend[4]{};context_->OMSetBlendState(opaqueBlend_.Get(),blend,0xffffffffu);
    for (size_t i=0;i<3;++i) {
        if (i<2 && (!(overlayMask&2u)||!(modeMask&4u)))continue;
        if (i==2 && (!(overlayMask&4u)||!(modeMask&8u)))continue;
        const auto& b=functionalModelBatches_[i];
        if (!b.mesh || !b.instanceBuffer || b.instances.empty())continue;
        ID3D11Buffer* buffers[]={b.mesh->vertexBuffer.Get(),b.instanceBuffer.Get()};
        const UINT strides[]={sizeof(Vertex),sizeof(InstanceData)},offsets[]={0,0};
        context_->IASetVertexBuffers(0,2,buffers,strides,offsets);
        context_->IASetIndexBuffer(b.mesh->indexBuffer.Get(),DXGI_FORMAT_R32_UINT,0);
        for (size_t part=0;part<b.mesh->parts.size();++part) {
            const auto& segment=b.mesh->parts[part];
            context_->RSSetState(segment.oppositeFaceAtlas?rasterizerPairedAtlas_.Get():rasterizer_.Get());
            ID3D11ShaderResourceView* tex=b.textures[part]->srv.Get();
            context_->PSSetShaderResources(0,1,&tex);
            context_->DrawIndexedInstanced(segment.indexCount,static_cast<UINT>(b.instances.size()),segment.firstIndex,0,0);
        }
        context_->RSSetState(rasterizer_.Get());
    }
}

void SceneRenderer::BuildFunctionalPads(const MapDocument& map) {
    functionalPadBuffer_.Reset(); functionalPadVertices_=functionalFlagVertices_=functionalPointVertices_=0;
    functionalSpawnStarts_.fill(0);functionalSpawnCounts_.fill(0);
    std::vector<GridVertex> v;
    auto addPad=[&](const XMFLOAT3& legacy, float radius, XMFLOAT4 color) {
        const XMFLOAT3 p=LegacyTransform::Position(legacy);
        constexpr int sides=16;
        constexpr float pi=3.14159265358979323846f;
        const float bottom=p.y+5.0f, top=p.y+43.0f;
        const XMFLOAT4 edge{0.42f,0.45f,0.50f,0.85f};
        for (int i=0;i<sides;++i) {
            const float a=float(i)*2*pi/sides, b=float(i+1)*2*pi/sides;
            const XMFLOAT3 t0{p.x+std::cos(a)*radius,top,p.z+std::sin(a)*radius};
            const XMFLOAT3 t1{p.x+std::cos(b)*radius,top,p.z+std::sin(b)*radius};
            const XMFLOAT3 b0{t0.x,bottom,t0.z}, b1{t1.x,bottom,t1.z};
            v.push_back({{p.x,top,p.z},color});v.push_back({t0,color});v.push_back({t1,color});
            v.push_back({b0,edge});v.push_back({t0,edge});v.push_back({t1,edge});
            v.push_back({b0,edge});v.push_back({t1,edge});v.push_back({b1,edge});
        }
    };
    for(const auto& flag:map.CtfFlags()) {
        const auto col=Lower(flag.team)=="red"?XMFLOAT4{1.0f,0.08f,0.07f,1.0f}:XMFLOAT4{0.03f,0.20f,1.0f,1.0f};
        addPad(flag.position,165.0f,col);
    }
    functionalFlagVertices_=static_cast<uint32_t>(v.size());
    for(const auto& point:map.ControlPoints()) addPad(point.position,200.0f,{1.0f,0.75f,0.07f,1.0f});
    functionalPointVertices_=static_cast<uint32_t>(v.size())-functionalFlagVertices_;
    // Spawn points are solid, oriented triangular editor objects (DM / TDM / CTF / DOM).
    // They are never saved as static geometry: underlying native spawn XML remains intact.
    std::array<std::vector<GridVertex>,4> groups;
    for(const auto& spawn:map.Spawns()) {
        const std::string kind=Lower(spawn.type);
        const unsigned mode=kind=="dom"?3u:kind=="dm"?0u:1u;
        const XMFLOAT4 color=kind=="dm"?XMFLOAT4{0.16f,0.96f,0.26f,1.0f}:
          (Lower(spawn.team)=="red"||kind=="red")?XMFLOAT4{1.0f,0.15f,0.10f,1.0f}:
          (Lower(spawn.team)=="blue"||kind=="blue")?XMFLOAT4{0.13f,0.52f,1.0f,1.0f}:
          XMFLOAT4{0.98f,0.81f,0.13f,1.0f};
        // Keep the whole arrow coloured, not just one face / its wire cage.
        const XMFLOAT4 edge{color.x*0.70f,color.y*0.70f,color.z*0.70f,1.0f};
        const XMFLOAT3 p=LegacyTransform::Position(spawn.position);
        const float c=std::cos(spawn.rotationZ), s=std::sin(spawn.rotationZ);
        auto at=[&](float x,float z,float y) {return XMFLOAT3{p.x+c*x+s*z,p.y+y,p.z-s*x+c*z};};
        const XMFLOAT3 a=at(-96,-75,22),b=at(96,-75,22),tip=at(0,145,86),rear=at(0,-75,22);
        auto& out=groups[mode];
        out.push_back({a,color});out.push_back({b,color});out.push_back({tip,color});
        out.push_back({rear,edge});out.push_back({tip,edge});out.push_back({a,edge});
        out.push_back({rear,edge});out.push_back({b,edge});out.push_back({tip,edge});
    }
    for(size_t i=0;i<groups.size();++i) {
        functionalSpawnStarts_[i]=static_cast<uint32_t>(v.size());
        functionalSpawnCounts_[i]=static_cast<uint32_t>(groups[i].size());
        v.insert(v.end(),groups[i].begin(),groups[i].end());
    }
    if(v.empty()) return;
    D3D11_BUFFER_DESC d{}; d.ByteWidth=static_cast<UINT>(v.size()*sizeof(GridVertex));
    d.Usage=D3D11_USAGE_IMMUTABLE; d.BindFlags=D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA data{v.data()};
    if(SUCCEEDED(device_->CreateBuffer(&d,&data,&functionalPadBuffer_))) functionalPadVertices_=static_cast<uint32_t>(v.size());
}

void SceneRenderer::RenderFunctionalPads(unsigned overlayMask, unsigned modeMask) {
    if(!functionalPadBuffer_) return;
    UINT stride=sizeof(GridVertex),offset=0; ID3D11Buffer* buffer=functionalPadBuffer_.Get();
    context_->IASetVertexBuffers(0,1,&buffer,&stride,&offset);
    context_->IASetInputLayout(gridLayout_.Get());context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context_->VSSetShader(gridVs_.Get(),nullptr,0);context_->PSSetShader(gridPs_.Get(),nullptr,0);
    ID3D11Buffer* cb=cameraBuffer_.Get();context_->VSSetConstantBuffers(0,1,&cb);
    context_->OMSetDepthStencilState(spriteDepthState_.Get(),0);
    const float blend[4]{};context_->OMSetBlendState(alphaBlend_.Get(),blend,0xffffffffu);
    // When real ProTLVK pedestal meshes are packaged, do not draw the old flat placeholder pads.
    if((overlayMask&2u)&&(modeMask&4u)&&functionalFlagVertices_ &&
       (!functionalModelBatches_[0].mesh || !functionalModelBatches_[1].mesh))
        context_->Draw(functionalFlagVertices_,0);
    if((overlayMask&4u)&&(modeMask&8u)&&functionalPointVertices_ && !functionalModelBatches_[2].mesh)
        context_->Draw(functionalPointVertices_,functionalFlagVertices_);
    if (overlayMask&1u) {
        for(size_t i=0;i<4;++i) {
            const unsigned bit=i==0?1u:i==1?2u|4u:i==2?4u:8u;
            if ((modeMask&bit) && functionalSpawnCounts_[i])
                context_->Draw(functionalSpawnCounts_[i],functionalSpawnStarts_[i]);
        }
    }
}

void SceneRenderer::RenderFunctionalGhost() {
    if(!functionalGhostActive_) return;
    const int modelIndex=functionalGhostKind_==1u?0:functionalGhostKind_==2u?1:functionalGhostKind_==3u?2:-1;
    if(modelIndex>=0) {
        const auto& b=functionalModelBatches_[static_cast<size_t>(modelIndex)];
        if (b.mesh && !b.textures.empty()) {
            if (!functionalGhostModelBuffer_) {
                D3D11_BUFFER_DESC desc{};desc.ByteWidth=sizeof(InstanceData);
                desc.Usage=D3D11_USAGE_DYNAMIC;desc.BindFlags=D3D11_BIND_VERTEX_BUFFER;
                desc.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
                device_->CreateBuffer(&desc,nullptr,&functionalGhostModelBuffer_);
            }
            if(functionalGhostModelBuffer_) {
                PropInstance ghost;ghost.position=functionalGhostPosition_;
                const InstanceData instance=MakeInstance(ghost);
                D3D11_MAPPED_SUBRESOURCE mapped{};
                if(SUCCEEDED(context_->Map(functionalGhostModelBuffer_.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&mapped))) {
                    std::memcpy(mapped.pData,&instance,sizeof(instance));
                    context_->Unmap(functionalGhostModelBuffer_.Get(),0);
                    context_->IASetInputLayout(meshLayout_.Get());
                    context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
                    context_->VSSetShader(meshVs_.Get(),nullptr,0);context_->PSSetShader(ghostMeshPs_.Get(),nullptr,0);
                    ID3D11Buffer* cb=cameraBuffer_.Get();context_->VSSetConstantBuffers(0,1,&cb);
                    ID3D11SamplerState* sampler=sampler_.Get();context_->PSSetSamplers(0,1,&sampler);
                    context_->OMSetDepthStencilState(depthState_.Get(),0);
                    const float blend[4]{};context_->OMSetBlendState(alphaBlend_.Get(),blend,0xffffffffu);
                    ID3D11Buffer* buffers[]={b.mesh->vertexBuffer.Get(),functionalGhostModelBuffer_.Get()};
                    const UINT strides[]={sizeof(Vertex),sizeof(InstanceData)},offsets[]={0,0};
                    context_->IASetVertexBuffers(0,2,buffers,strides,offsets);
                    context_->IASetIndexBuffer(b.mesh->indexBuffer.Get(),DXGI_FORMAT_R32_UINT,0);
                    for(size_t part=0;part<b.mesh->parts.size();++part) {
                        const auto& segment=b.mesh->parts[part];
                        context_->RSSetState(segment.oppositeFaceAtlas?rasterizerPairedAtlas_.Get():rasterizer_.Get());
                        ID3D11ShaderResourceView* tex=b.textures[part]->srv.Get();
                        context_->PSSetShaderResources(0,1,&tex);
                        context_->DrawIndexedInstanced(segment.indexCount,1,segment.firstIndex,0,0);
                    }
                    context_->RSSetState(rasterizer_.Get());
                    return;
                }
            }
        }
    }
    // A readable translucent 3D ground pad, independent of the static-prop GPU batches.
    std::vector<GridVertex> v;
    constexpr int sides=16; constexpr float pi=3.14159265358979323846f;
    const XMFLOAT3 p=LegacyTransform::Position(functionalGhostPosition_);
    XMFLOAT4 c= functionalGhostKind_==1?XMFLOAT4{0.92f,0.15f,0.12f,0.47f}:
        functionalGhostKind_==2?XMFLOAT4{0.13f,0.37f,0.99f,0.47f}:XMFLOAT4{0.97f,0.70f,0.14f,0.47f};
    for(int i=0;i<sides;++i) {
        float a=float(i)*2*pi/sides,b=float(i+1)*2*pi/sides;
        v.push_back({{p.x,p.y+48,p.z},c});
        v.push_back({{p.x+std::cos(a)*165,p.y+48,p.z+std::sin(a)*165},c});
        v.push_back({{p.x+std::cos(b)*165,p.y+48,p.z+std::sin(b)*165},c});
    }
    D3D11_BUFFER_DESC d{};d.ByteWidth=static_cast<UINT>(v.size()*sizeof(GridVertex));
    d.Usage=D3D11_USAGE_DYNAMIC;d.BindFlags=D3D11_BIND_VERTEX_BUFFER;d.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
    if(!functionalGhostBuffer_ && FAILED(device_->CreateBuffer(&d,nullptr,&functionalGhostBuffer_)))return;
    D3D11_MAPPED_SUBRESOURCE m{};if(FAILED(context_->Map(functionalGhostBuffer_.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&m)))return;
    std::memcpy(m.pData,v.data(),v.size()*sizeof(GridVertex));context_->Unmap(functionalGhostBuffer_.Get(),0);
    UINT stride=sizeof(GridVertex),offset=0;ID3D11Buffer* buffer=functionalGhostBuffer_.Get();
    context_->IASetVertexBuffers(0,1,&buffer,&stride,&offset);context_->IASetInputLayout(gridLayout_.Get());
    context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context_->VSSetShader(gridVs_.Get(),nullptr,0);context_->PSSetShader(gridPs_.Get(),nullptr,0);
    ID3D11Buffer* cb=cameraBuffer_.Get();context_->VSSetConstantBuffers(0,1,&cb);
    context_->OMSetDepthStencilState(spriteDepthState_.Get(),0);const float blend[4]{};
    context_->OMSetBlendState(alphaBlend_.Get(),blend,0xffffffffu);context_->Draw(static_cast<UINT>(v.size()),0);
}
