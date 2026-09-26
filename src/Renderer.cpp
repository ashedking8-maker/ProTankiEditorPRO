#include "Renderer.h"
#include "Logger.h"
#include <dxgi.h>
#include <sstream>

bool Renderer::Initialize(HWND hwnd, int width, int height) {
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2;
    sd.BufferDesc.Width = width;
    sd.BufferDesc.Height = height;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    UINT flags = 0;
#if defined(_DEBUG)
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    D3D_FEATURE_LEVEL level{};
    const D3D_FEATURE_LEVEL requested[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1 };
    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
        requested, ARRAYSIZE(requested), D3D11_SDK_VERSION, &sd, &swap_, &device_, &level, &context_);
    if (FAILED(hr)) {
        std::ostringstream msg; msg << "D3D11CreateDeviceAndSwapChain failed hr=0x" << std::hex << static_cast<unsigned long>(hr);
        Log::Error(msg.str());
        return false;
    }

    std::ostringstream info;
    info << "D3D11 hardware device created. featureLevel=0x" << std::hex << static_cast<unsigned int>(level);
    Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
    Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
    if (SUCCEEDED(device_.As(&dxgiDevice)) && SUCCEEDED(dxgiDevice->GetAdapter(&adapter))) {
        DXGI_ADAPTER_DESC desc{};
        if (SUCCEEDED(adapter->GetDesc(&desc))) {
            info << " gpu=\"" << Log::WideUtf8(desc.Description) << "\""
                 << " vendor=0x" << desc.VendorId << " device=0x" << desc.DeviceId
                 << std::dec << " dedicatedVideoMB=" << (desc.DedicatedVideoMemory / (1024ull*1024ull));
        }
    }
    Log::Info(info.str());
    CreateRenderTarget();
    return true;
}

void Renderer::CreateRenderTarget() {
    Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
    if (SUCCEEDED(swap_->GetBuffer(0, IID_PPV_ARGS(&backBuffer))))
        device_->CreateRenderTargetView(backBuffer.Get(), nullptr, &rtv_);
}

void Renderer::Resize(int width, int height) {
    if (!swap_ || width <= 0 || height <= 0) return;
    rtv_.Reset();
    swap_->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
    CreateRenderTarget();
}

void Renderer::BeginFrame() {
    ID3D11RenderTargetView* view = rtv_.Get();
    context_->OMSetRenderTargets(1, &view, nullptr);
    const float clear[] = {0.025f, 0.028f, 0.032f, 1.0f};
    context_->ClearRenderTargetView(rtv_.Get(), clear);
}

void Renderer::EndFrame(bool vsync) { swap_->Present(vsync ? 1 : 0, 0); }
void Renderer::Shutdown() { rtv_.Reset(); swap_.Reset(); context_.Reset(); device_.Reset(); }
