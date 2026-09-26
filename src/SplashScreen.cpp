#include "SplashScreen.h"
// With WIN32_LEAN_AND_MEAN, GDI+ needs the COM stream/property types explicitly.
// These headers MUST precede gdiplus.h, which uses IStream and PROPID.
#include <objidl.h>
#include <propidl.h>
#include <gdiplus.h>
#include <algorithm>
#include <memory>
#include <string>
#include <cstring>

namespace {
using namespace Gdiplus;
HWND window = nullptr;
ULONG_PTR gdiplusToken = 0;
IStream* imageStream = nullptr;
std::unique_ptr<Bitmap> image;
std::wstring status = L"Starting application...";
constexpr int width = 870;
constexpr int height = 427;

LRESULT CALLBACK SplashProc(HWND h, UINT message, WPARAM w, LPARAM l) {
    if (message == WM_ERASEBKGND) return 1;
    if (message == WM_PAINT) {
        PAINTSTRUCT ps{};
        HDC dc = BeginPaint(h, &ps);
        Graphics g(dc);
        g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
        SolidBrush background(Color(255, 12, 15, 19));
        g.FillRectangle(&background, 0, 0, width, height);
        if (image && image->GetLastStatus() == Ok) {
            g.DrawImage(image.get(), Rect(0, 0, width, 373));
            SolidBrush copyrightShadow(Color(195,0,0,0));
            SolidBrush copyrightText(Color(235,218,224,232));
            // Windows/RPC headers may define `small` as a macro (char).
            // Avoid it as a C++ variable name in this translation unit.
            Gdiplus::Font copyrightFont(L"Segoe UI", 11.0f, FontStyleRegular, UnitPixel);
            g.DrawString(L"Created by Niss  © 2026. All rights reserved.", -1, &copyrightFont, PointF(634, 353), &copyrightShadow);
            g.DrawString(L"Created by Niss  © 2026. All rights reserved.", -1, &copyrightFont, PointF(633, 352), &copyrightText);
        }
        Pen divider(Color(255, 63, 86, 109), 1);
        g.DrawLine(&divider, 0, 373, width, 373);
        SolidBrush ink(Color(255, 222, 230, 236));
        Font font(L"Segoe UI", 14.0f, FontStyleRegular, UnitPixel);
        g.DrawString(status.c_str(), -1, &font, PointF(23, 390), &ink);
        EndPaint(h, &ps);
        return 0;
    }
    return DefWindowProcW(h, message, w, l);
}
}

void SplashScreen::Open(HINSTANCE instance) {
    if (window) return;
    GdiplusStartupInput startup;
    if (GdiplusStartup(&gdiplusToken, &startup, nullptr) != Ok) return;
    HRSRC resource = FindResourceW(instance, MAKEINTRESOURCEW(201), RT_RCDATA);
    if (resource) {
        HGLOBAL raw = LoadResource(instance, resource);
        const DWORD bytes = SizeofResource(instance, resource);
        const void* source = raw ? LockResource(raw) : nullptr;
        if (source && bytes) {
            HGLOBAL copy = GlobalAlloc(GMEM_MOVEABLE, bytes);
            if (copy) {
                void* destination = GlobalLock(copy);
                if (destination) { std::memcpy(destination, source, bytes); GlobalUnlock(copy); }
                if (destination && SUCCEEDED(CreateStreamOnHGlobal(copy, TRUE, &imageStream)))
                    image.reset(Bitmap::FromStream(imageStream));
                else GlobalFree(copy);
            }
        }
    }
    WNDCLASSW wc{};
    wc.lpfnWndProc = SplashProc; wc.hInstance = instance;
    wc.lpszClassName = L"GTanksNativeSplash"; wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DROPSHADOW;
    RegisterClassW(&wc);
    const int screenW = GetSystemMetrics(SM_CXSCREEN), screenH = GetSystemMetrics(SM_CYSCREEN);
    window = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, wc.lpszClassName,
        L"ProTanki Editor PRO", WS_POPUP, (screenW-width)/2, (screenH-height)/2,
        width, height, nullptr, nullptr, instance, nullptr);
    if (window) { ShowWindow(window, SW_SHOWNOACTIVATE); UpdateWindow(window); }
}

void SplashScreen::Status(const wchar_t* oneLine) {
    status = oneLine ? oneLine : L"Loading...";
    if (window) { InvalidateRect(window, nullptr, FALSE); UpdateWindow(window); }
}

void SplashScreen::Close() {
    if (window) { DestroyWindow(window); window = nullptr; }
    image.reset();
    if (imageStream) { imageStream->Release(); imageStream = nullptr; }
    if (gdiplusToken) { GdiplusShutdown(gdiplusToken); gdiplusToken = 0; }
}

bool SplashScreen::IsOpen() { return window != nullptr; }
