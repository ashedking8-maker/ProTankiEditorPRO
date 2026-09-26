#pragma once
#include <windows.h>

// Native, synchronously repainted startup window. It exists before D3D/ImGui and
// remains visible until the editor has presented its first completed frame.
namespace SplashScreen {
    void Open(HINSTANCE instance);
    void Status(const wchar_t* oneLine);
    void Close();
    bool IsOpen();
}
