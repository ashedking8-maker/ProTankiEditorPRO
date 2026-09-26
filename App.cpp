#include "App.h"
#include "Theme.h"
#include "Logger.h"
#include "SplashScreen.h"
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>
#include <filesystem>

static App* g_app = nullptr;
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);
static LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m==WM_MOUSEWHEEL && g_app && g_app->CaptureWheel(w)) return 0;
    // Never allow ImGui to swallow WM_CLOSE; app owns the unsaved-map transition.
    if (m != WM_CLOSE && ImGui_ImplWin32_WndProcHandler(h,m,w,l)) return 1;
    return g_app ? g_app->Message(h,m,w,l) : DefWindowProcW(h,m,w,l);
}

bool App::CaptureWheel(WPARAM w) {
    return ui_.CaptureNativeWheel(GET_WHEEL_DELTA_WPARAM(w));
}

bool App::Initialize(HINSTANCE instance, int show) {
    g_app = this;
    Log::Info("App::Initialize begin. cwd=" + Log::PathUtf8(std::filesystem::current_path()));
    SplashScreen::Status(L"Initializing Windows and renderer...");
    const HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    comInitialized_ = SUCCEEDED(com);

    WNDCLASSEXW wc{sizeof(wc), CS_CLASSDC, WndProc, 0,0,instance,nullptr,LoadCursor(nullptr,IDC_ARROW),nullptr,nullptr,L"GTanksNextEditor",nullptr};
    // Use the same embedded transparent icon in the title bar, taskbar, and Alt+Tab.
    wc.hIcon = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(101), IMAGE_ICON,
        GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_SHARED));
    wc.hIconSm = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(101), IMAGE_ICON,
        GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_SHARED));
    RegisterClassExW(&wc);
    hwnd_ = CreateWindowExW(0,wc.lpszClassName,L"ProTanki Editor PRO 0.5.19",WS_OVERLAPPEDWINDOW,100,80,1500,900,nullptr,nullptr,instance,nullptr);
    if (!hwnd_) { Log::Error("CreateWindowExW failed."); return false; }
    RECT r{}; GetClientRect(hwnd_, &r);
    if (!renderer_.Initialize(hwnd_, r.right-r.left, r.bottom-r.top)) { Log::Error("D3D renderer initialization failed."); return false; }

    std::string sceneError;
    if (!scene_.Initialize(renderer_.Device(), renderer_.Context(), sceneError)) {
        Log::Error("Scene renderer initialization failed: " + sceneError);
        MessageBoxA(hwnd_, sceneError.c_str(), "ProTanki Editor PRO - renderer initialization", MB_ICONERROR | MB_OK);
        return false;
    }
    if (!previewScene_.Initialize(renderer_.Device(), renderer_.Context(), sceneError)) {
        Log::Error("Preview renderer initialization failed: " + sceneError);
        MessageBoxA(hwnd_, sceneError.c_str(), "ProTanki Editor PRO - preview initialization", MB_ICONERROR | MB_OK);
        return false;
    }

    SplashScreen::Status(L"Preparing user interface...");
    IMGUI_CHECKVERSION(); ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable; // Tab belongs to AX Library; no ImGui keyboard focus traversal.
    io.IniFilename = "gtanks_editor_layout_0513.ini"; // one-time docking migration, then user-customizable
    ui_.ApplyPreferredTheme();
    ImGui_ImplWin32_Init(hwnd_); ImGui_ImplDX11_Init(renderer_.Device(), renderer_.Context());

    // Always have an actual, initially clean empty XML workspace before a map is
    // opened. Ghost placement without a document used to create invisible props.
    map_.CreateBlank("1.0.Light", false);
    SplashScreen::Status(L"Checking local asset library...");
    const auto localLib = std::filesystem::current_path() / "library";
    if (std::filesystem::is_directory(localLib)) {
        std::string err;
        if (assets_.Scan(localLib, err)) { ui_.OnLibraryLoaded(previewScene_,assets_.Root()); Log::Info("Auto-indexed local library root: " + Log::PathUtf8(localLib)); }
        else Log::Warning("Local library auto-scan failed: " + err);
    }
    SplashScreen::Status(L"Opening editor workspace...");
    ShowWindow(hwnd_, ui_.FirstRun()?SW_SHOWMAXIMIZED:show); UpdateWindow(hwnd_);
    Log::Info("App::Initialize complete. sessionLog=" + Log::PathUtf8(Log::SessionFile()));
    return true;
}

bool App::TryAutoLibraryForMap() {
    if (map_.Path().empty()) return false;
    const auto mapDir = map_.Path().parent_path();
    const auto candidate = mapDir.parent_path() / "library";
    if (!std::filesystem::is_directory(candidate)) { Log::Debug("No adjacent library candidate for map: " + Log::PathUtf8(candidate)); return false; }
    std::error_code sameError;
    if (!assets_.Root().empty() && std::filesystem::equivalent(assets_.Root(), candidate, sameError) && !sameError) return true;
    std::string err;
    if (!assets_.Scan(candidate, err)) { Log::Error("Adjacent library scan failed: " + err); ui_.SetMessage(err, true); return false; }
    ui_.OnLibraryLoaded(previewScene_,assets_.Root());
    Log::Info("Auto-selected adjacent library: " + Log::PathUtf8(candidate));
    return true;
}

void App::RebuildScene(bool frameScene) {
    if (map_.Version().empty() || assets_.AssetCount() == 0) return;
    Log::Info("Rebuilding GPU scene. map=" + Log::PathUtf8(map_.Path()) + " assets=" + std::to_string(assets_.AssetCount()));
    std::string err;
    if (!scene_.BuildScene(map_, assets_, err, frameScene)) { Log::Error("GPU scene build failed: " + err); ui_.SetMessage(err, true); return; }
    const auto& s = scene_.Stats();
    std::string msg = "GPU scene: " + std::to_string(s.meshInstances) + " meshes + " + std::to_string(s.spriteInstances) +
        " sprites in " + std::to_string(s.drawCalls) + " draw calls.";
    if (s.missingAssets) msg += " Missing: " + std::to_string(s.missingAssets) + ".";
    Log::Info(msg);
    if(s.missingAssets) ui_.SetMessage(std::move(msg),true); // Routine library/scene indexing stays in the log.
}

int App::Run() {
    MSG msg{};
    while (msg.message != WM_QUIT) {
        if (PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); continue; }
        ProcessPendingOperations(); // Previous frame has finished rendering; GPU/UI references are no longer live.
        ImGui_ImplDX11_NewFrame(); ImGui_ImplWin32_NewFrame(); ImGui::NewFrame();
        ui_.Draw(map_, assets_, scene_, previewScene_);
        DrawUnsavedChangesDialog();

        RefreshTitle();
        ImGui::Render();
        renderer_.BeginFrame();
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        renderer_.EndFrame(vsync_);
        // Keep the native splash visible until the editor has actually presented a full frame.
        if (firstFrame_) {
            SplashScreen::Status(L"Loading complete");
            splashCompleteAt_=GetTickCount64(); firstFrame_=false;
        }
        if (splashCompleteAt_ && GetTickCount64()-splashCompleteAt_>=1000) {
            SplashScreen::Close(); splashCompleteAt_=0;
        }
    }
    return static_cast<int>(msg.wParam);
}

void App::CompleteTransition() {
    const auto operation=transition_;
    const auto requestedPath=transitionMapPath_;
    transition_=Transition::None;
    transitionApproved_=false;
    unsavedPopupOpened_=false;
    transitionMapPath_.clear();
    if(operation==Transition::Close) { PostQuitMessage(0); return; }
    if(operation==Transition::NewMap) {
        map_.CreateBlank();
        ui_.OnNewMap();
        if(assets_.AssetCount()) RebuildScene(); else scene_.ClearScene();
        ui_.SetMessage("New empty legacy map. File > Save As to choose an XML path.");
        return;
    }
    if(operation==Transition::OpenMap) {
        std::string err;
        SplashScreen::Open(GetModuleHandleW(nullptr));
        SplashScreen::Status(L"Loading map and GPU assets...");
        Log::Info("User opened map: " + Log::PathUtf8(requestedPath));
        // Parse the replacement before touching the live document. A load failure
        // never drops unsaved in-memory data or corrupts the existing GPU scene.
        MapDocument candidate;
        if(candidate.Load(requestedPath,err)) {
                map_=std::move(candidate);
            ui_.OnMapLoaded(map_.Path());
            TryAutoLibraryForMap();
            if(assets_.AssetCount()) RebuildScene();
            else {scene_.ClearScene();ui_.SetMessage("Map loaded. Select the original 'library' folder to render assets.");}
        } else {Log::Error("Map load failed: "+err);ui_.SetMessage(err,true);}
        SplashScreen::Close();
    }
}

void App::ProcessPendingOperations() {
    ui_.ProcessPendingNativeDialogs();
    if(transitionApproved_) {CompleteTransition();return;}
    if(transition_!=Transition::None) return; // Modal owns the app until a decision.
    if(ui_.ConsumeFullscreenToggle()) ToggleFullscreen();
    if(ui_.ConsumeTestRequest()) ui_.LaunchExternalTester();
    bool preserveCamera=false;
    if(ui_.ConsumeSceneRebuildRequest(preserveCamera)) RebuildScene(!preserveCamera);

    // Map-changing operations must not execute from inside an ImGui frame or a
    // native window-message callback. Stage them and resolve the dialog first.
    if(closeRequested_) {closeRequested_=false;transition_=Transition::Close;}
    else if(ui_.ConsumeNewMapRequest()) transition_=Transition::NewMap;
    else if(!ui_.PendingMap().empty()) {
        transitionMapPath_=ui_.PendingMap();ui_.ClearPendingMap();transition_=Transition::OpenMap;
    }
    if(transition_!=Transition::None) {
        unsavedPopupOpened_=false;
        if(!map_.Dirty()) CompleteTransition();
        return;
    }
    if(!ui_.PendingLibrary().empty()) {
        std::string err;
        SplashScreen::Open(GetModuleHandleW(nullptr));
        SplashScreen::Status(L"Indexing asset library...");
        Log::Info("User selected library root: "+Log::PathUtf8(ui_.PendingLibrary()));
        if(assets_.Scan(ui_.PendingLibrary(),err)) {
            ui_.OnLibraryLoaded(previewScene_,assets_.Root());
            Log::Info("Library indexed: "+std::to_string(assets_.AssetCount())+" assets.");
            RebuildScene();
        } else {Log::Error("Library scan failed: "+err);ui_.SetMessage(err,true);}
        ui_.ClearPendingLibrary();
        SplashScreen::Close();
    }
}

void App::DrawUnsavedChangesDialog() {
    if(transition_==Transition::None || transitionApproved_ || !map_.Dirty()) return;
    if(!unsavedPopupOpened_) {ImGui::OpenPopup("Unsaved map changes##editor");unsavedPopupOpened_=true;}
    ImGui::SetNextWindowSize(ImVec2(470,0),ImGuiCond_Appearing);
    if(ImGui::BeginPopupModal("Unsaved map changes##editor",nullptr,
            ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoMove)) {
        ImGui::Spacing();
        ImGui::TextWrapped("Save changes to the current map before continuing?");
        ImGui::TextDisabled("Cancel returns to the editor without changing the map.");
        ImGui::Spacing();ImGui::Separator();
        if(ImGui::Button("Save changes",ImVec2(140,0))) {
            if(ui_.SaveForTransition(map_)) {transitionApproved_=true;ImGui::CloseCurrentPopup();}
            else ui_.SetMessage("Save cancelled or failed; map was not replaced.",true);
        }
        ImGui::SameLine();
        if(ImGui::Button("Don't save",ImVec2(140,0))) {
            transitionApproved_=true;ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if(ImGui::Button("Cancel",ImVec2(110,0)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            transition_=Transition::None;transitionMapPath_.clear();
            unsavedPopupOpened_=false;ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void App::Shutdown() {
    Log::Info("App::Shutdown begin.");
    ImGui_ImplDX11_Shutdown(); ImGui_ImplWin32_Shutdown(); ImGui::DestroyContext();
    ui_.ShutdownObjectPreview(); previewScene_.Shutdown(); scene_.Shutdown(); renderer_.Shutdown();
    if (hwnd_) DestroyWindow(hwnd_); hwnd_ = nullptr; g_app = nullptr;
    if (comInitialized_) CoUninitialize(); comInitialized_ = false;
    Log::Info("App::Shutdown complete.");
}


void App::RefreshTitle() {
    const bool dirty=map_.Dirty();
    if (dirty==lastDirty_) return;
    lastDirty_=dirty;
    const std::wstring title=L"ProTanki Editor PRO 0.5.19";
    SetWindowTextW(hwnd_,title.c_str());
}

void App::ToggleFullscreen() {
    if (!hwnd_) return;
    if (!fullscreen_) {
        windowedStyle_ = static_cast<DWORD>(GetWindowLongPtrW(hwnd_, GWL_STYLE));
        windowedPlacement_.length = sizeof(WINDOWPLACEMENT);
        GetWindowPlacement(hwnd_, &windowedPlacement_);
        MONITORINFO mi{sizeof(MONITORINFO)};
        if (GetMonitorInfoW(MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST), &mi)) {
            SetWindowLongPtrW(hwnd_, GWL_STYLE, static_cast<LONG_PTR>(windowedStyle_ & ~WS_OVERLAPPEDWINDOW));
            SetWindowPos(hwnd_, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top,
                mi.rcMonitor.right-mi.rcMonitor.left, mi.rcMonitor.bottom-mi.rcMonitor.top,
                SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
            fullscreen_ = true;
        }
    } else {
        SetWindowLongPtrW(hwnd_, GWL_STYLE, static_cast<LONG_PTR>(windowedStyle_));
        SetWindowPlacement(hwnd_, &windowedPlacement_);
        SetWindowPos(hwnd_, nullptr, 0,0,0,0, SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOOWNERZORDER|SWP_FRAMECHANGED);
        fullscreen_ = false;
    }
    ui_.SetFullscreenState(fullscreen_);
    Log::Info(std::string("Fullscreen ") + (fullscreen_ ? "enabled" : "disabled"));
}

LRESULT App::Message(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_SIZE: if (w != SIZE_MINIMIZED) renderer_.Resize(LOWORD(l), HIWORD(l)); return 0;
    case WM_DPICHANGED: {
        auto* suggested = reinterpret_cast<RECT*>(l);
        SetWindowPos(h,nullptr,suggested->left,suggested->top,suggested->right-suggested->left,suggested->bottom-suggested->top,SWP_NOZORDER|SWP_NOACTIVATE); return 0;
    }
    case WM_CLOSE: closeRequested_=true; return 0;
    }
    return DefWindowProcW(h,m,w,l);
}
