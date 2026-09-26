#pragma once
#include <d3d11.h>
#include <wrl/client.h>

class Renderer {
public:
    bool Initialize(HWND hwnd, int width, int height);
    void Resize(int width, int height);
    void BeginFrame();
    void EndFrame(bool vsync);
    void Shutdown();

    ID3D11Device* Device() const { return device_.Get(); }
    ID3D11DeviceContext* Context() const { return context_.Get(); }

private:
    void CreateRenderTarget();
    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
    Microsoft::WRL::ComPtr<IDXGISwapChain> swap_;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv_;
};
