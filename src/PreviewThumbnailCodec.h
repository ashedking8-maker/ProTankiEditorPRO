#pragma once
// In-memory WIC PNG codec for Browse Library thumbnails. No filesystem I/O.
// GPU captures and uploads happen on the UI/D3D immediate-context thread only.
#include "BrowseCachePolicy.h"
#include <d3d11.h>
#include <dxgi.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <shlwapi.h>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>

namespace PreviewThumbnailCodec {
using Microsoft::WRL::ComPtr;
inline constexpr size_t MiB=BrowseCachePolicy::MiB;
inline size_t GpuBudget(ID3D11Device* device) {
    // Integrated/unknown GPU: conservative fallback. Dedicated GPU: at most
    // one sixteenth of dedicated VRAM, with an absolute 256 MiB upper bound.
    size_t dedicated=0;
    if(device) {
        ComPtr<IDXGIDevice> dxgi;
        ComPtr<IDXGIAdapter> adapter;
        DXGI_ADAPTER_DESC desc{};
        if(SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&dxgi))) &&
           SUCCEEDED(dxgi->GetAdapter(&adapter)) && SUCCEEDED(adapter->GetDesc(&desc)))
            dedicated=desc.DedicatedVideoMemory;
    }
    return BrowseCachePolicy::GpuBudget(dedicated);
}
inline constexpr size_t CpuBudget=BrowseCachePolicy::MaxCpuBytes;

inline bool CapturePng(ID3D11ShaderResourceView* srv,ID3D11Device* device,
                       ID3D11DeviceContext* context,std::vector<std::uint8_t>& png) {
    png.clear();
    if(!srv||!device||!context)return false;
    ComPtr<ID3D11Resource> resource;
    srv->GetResource(resource.GetAddressOf());
    ComPtr<ID3D11Texture2D> texture;
    if(!resource||FAILED(resource.As(&texture)))return false;
    D3D11_TEXTURE2D_DESC td{};texture->GetDesc(&td);
    if(!td.Width||!td.Height||td.Width>1024||td.Height>1024||
       td.Format!=DXGI_FORMAT_R8G8B8A8_UNORM||td.SampleDesc.Count!=1)return false;
    td.Usage=D3D11_USAGE_STAGING;td.BindFlags=0;td.CPUAccessFlags=D3D11_CPU_ACCESS_READ;td.MiscFlags=0;
    ComPtr<ID3D11Texture2D> staging;
    if(FAILED(device->CreateTexture2D(&td,nullptr,&staging)))return false;
    context->CopyResource(staging.Get(),texture.Get());
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if(FAILED(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped)))return false;
    const UINT stride=td.Width*4;
    std::vector<std::uint8_t> pixels(static_cast<size_t>(stride)*td.Height);
    for(UINT y=0;y<td.Height;++y)
        std::memcpy(pixels.data()+static_cast<size_t>(y)*stride,
                    static_cast<const std::uint8_t*>(mapped.pData)+static_cast<size_t>(y)*mapped.RowPitch,stride);
    context->Unmap(staging.Get(),0);
    ComPtr<IWICImagingFactory> factory;
    if(FAILED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory))))return false;
    ComPtr<IStream> stream;
    if(FAILED(CreateStreamOnHGlobal(nullptr,TRUE,&stream)))return false;
    ComPtr<IWICBitmapEncoder> encoder;
    if(FAILED(factory->CreateEncoder(GUID_ContainerFormatPng,nullptr,&encoder))||
       FAILED(encoder->Initialize(stream.Get(),WICBitmapEncoderNoCache)))return false;
    ComPtr<IWICBitmapFrameEncode> frame;
    ComPtr<IPropertyBag2> bag;
    if(FAILED(encoder->CreateNewFrame(&frame,&bag))||FAILED(frame->Initialize(bag.Get()))||
       FAILED(frame->SetSize(td.Width,td.Height)))return false;
    // The built-in PNG encoder commonly chooses 32bppBGRA; do not discard
    // every RAM thumbnail because the preview render target is RGBA.
    WICPixelFormatGUID format=GUID_WICPixelFormat32bppBGRA;
    if(FAILED(frame->SetPixelFormat(&format)))return false;
    if(IsEqualGUID(format,GUID_WICPixelFormat32bppBGRA)) {
        for(size_t i=0;i<pixels.size();i+=4)std::swap(pixels[i],pixels[i+2]);
    } else if(!IsEqualGUID(format,GUID_WICPixelFormat32bppRGBA))return false;
    if(FAILED(frame->WritePixels(td.Height,stride,static_cast<UINT>(pixels.size()),pixels.data()))||
       FAILED(frame->Commit())||FAILED(encoder->Commit()))return false;
    HGLOBAL global{};
    if(FAILED(GetHGlobalFromStream(stream.Get(),&global))||!global)return false;
    STATSTG stats{};
    if(FAILED(stream->Stat(&stats,STATFLAG_NONAME)))return false;
    const auto bytes=static_cast<size_t>(stats.cbSize.QuadPart);
    if(!bytes||bytes>4u*MiB||bytes>GlobalSize(global))return false;
    const void* data=GlobalLock(global);
    if(!data)return false;
    png.resize(bytes);std::memcpy(png.data(),data,bytes);GlobalUnlock(global);
    return true;
}

inline bool RestorePng(const std::vector<std::uint8_t>& png,ID3D11Device* device,
                       ComPtr<ID3D11ShaderResourceView>& srv) {
    srv.Reset();
    if(!device||png.empty()||png.size()>4u*MiB)return false;
    ComPtr<IStream> stream;
    stream.Attach(SHCreateMemStream(png.data(),static_cast<UINT>(png.size())));
    if(!stream)return false;
    ComPtr<IWICImagingFactory> factory;
    if(FAILED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory))))return false;
    ComPtr<IWICBitmapDecoder> decoder;
    if(FAILED(factory->CreateDecoderFromStream(stream.Get(),nullptr,WICDecodeMetadataCacheOnLoad,&decoder)))return false;
    ComPtr<IWICBitmapFrameDecode> frame;
    if(FAILED(decoder->GetFrame(0,&frame)))return false;
    UINT width{},height{};
    if(FAILED(frame->GetSize(&width,&height))||!width||!height||width>1024||height>1024)return false;
    ComPtr<IWICFormatConverter> converter;
    if(FAILED(factory->CreateFormatConverter(&converter))||
       FAILED(converter->Initialize(frame.Get(),GUID_WICPixelFormat32bppRGBA,
              WICBitmapDitherTypeNone,nullptr,0.f,WICBitmapPaletteTypeCustom)))return false;
    const UINT stride=width*4;
    std::vector<std::uint8_t> pixels(static_cast<size_t>(stride)*height);
    if(FAILED(converter->CopyPixels(nullptr,stride,static_cast<UINT>(pixels.size()),pixels.data())))return false;
    D3D11_TEXTURE2D_DESC td{};td.Width=width;td.Height=height;td.MipLevels=td.ArraySize=1;
    td.Format=DXGI_FORMAT_R8G8B8A8_UNORM;td.SampleDesc.Count=1;td.Usage=D3D11_USAGE_IMMUTABLE;
    td.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA init{};init.pSysMem=pixels.data();init.SysMemPitch=stride;
    ComPtr<ID3D11Texture2D> texture;
    return SUCCEEDED(device->CreateTexture2D(&td,&init,&texture)) &&
           SUCCEEDED(device->CreateShaderResourceView(texture.Get(),nullptr,&srv));
}
} // namespace PreviewThumbnailCodec
