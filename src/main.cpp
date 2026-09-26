#include "App.h"
#include "Logger.h"
#include "SplashScreen.h"
#include <windows.h>
#include <exception>

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    // Show the native image immediately; the main editor window is still unopened.
    SplashScreen::Open(instance);
    Log::Initialize();
    Log::Info("ProTanki Editor PRO process starting.");

    int result = 1;
    try {
        App app;
        if (app.Initialize(instance, show)) {
            result = app.Run();
            app.Shutdown();
        } else {
            Log::Error("Application initialization failed.");
        }
    } catch (const std::exception& e) {
        Log::Error(std::string("Unhandled C++ exception: ") + e.what());
    } catch (...) {
        Log::Error("Unhandled unknown C++ exception.");
    }

    SplashScreen::Close();
    Log::Info("ProTanki Editor PRO process exiting with code " + std::to_string(result) + ".");
    Log::Shutdown();
    return result;
}
