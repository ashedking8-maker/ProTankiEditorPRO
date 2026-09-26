#pragma once
#include "Renderer.h"
#include "SceneRenderer.h"
#include "EditorUi.h"
#include "MapDocument.h"
#include "AssetRegistry.h"
#include <windows.h>

class App {
public:
    bool Initialize(HINSTANCE instance, int show);
    int Run();
    void Shutdown();
    LRESULT Message(HWND, UINT, WPARAM, LPARAM);
    bool CaptureWheel(WPARAM w);
private:
    void ProcessPendingOperations();
    void RebuildScene(bool frameScene = true);
    bool TryAutoLibraryForMap();
    enum class Transition { None, NewMap, OpenMap, Close };
    void CompleteTransition();
    void DrawUnsavedChangesDialog();
    void RefreshTitle();
    bool lastDirty_{};
    Transition transition_{Transition::None};
    bool transitionApproved_{};
    bool unsavedPopupOpened_{};
    bool closeRequested_{};
    std::filesystem::path transitionMapPath_;
    void ToggleFullscreen();

    HWND hwnd_{};
    Renderer renderer_;
    SceneRenderer scene_;
    SceneRenderer previewScene_;
    EditorUi ui_;
    MapDocument map_;
    AssetRegistry assets_;
    bool firstFrame_ = true;
    ULONGLONG splashCompleteAt_{};
    bool vsync_ = true;
    bool comInitialized_ = false;
    bool fullscreen_ = false;
    DWORD windowedStyle_ = WS_OVERLAPPEDWINDOW;
    WINDOWPLACEMENT windowedPlacement_{sizeof(WINDOWPLACEMENT)};
};
