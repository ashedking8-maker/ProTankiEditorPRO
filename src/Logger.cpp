#include "Logger.h"
#include <windows.h>
#include <dbghelp.h>
#include <shlobj.h>
#include <fstream>
#include <cstdio>
#include <cstdint>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <vector>

namespace Log {
namespace {
std::mutex g_mutex;
std::ofstream g_file;
std::filesystem::path g_dir;
std::filesystem::path g_session;
LPTOP_LEVEL_EXCEPTION_FILTER g_previousFilter = nullptr;

const char* LevelName(Level level) {
    switch (level) {
    case Level::Debug: return "DEBUG";
    case Level::Info: return "INFO";
    case Level::Warning: return "WARN";
    case Level::Error: return "ERROR";
    }
    return "INFO";
}

std::string Timestamp() {
    SYSTEMTIME st{};
    GetLocalTime(&st);
    char out[64]{};
    std::snprintf(out, sizeof(out), "%04u-%02u-%02u %02u:%02u:%02u.%03u",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    return out;
}

std::filesystem::path LocalAppData() {
    PWSTR value = nullptr;
    std::filesystem::path out;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE, nullptr, &value)) && value) {
        out = value;
        CoTaskMemFree(value);
    }
    if (out.empty()) out = std::filesystem::current_path();
    return out;
}

std::filesystem::path SessionName() {
    SYSTEMTIME st{};
    GetLocalTime(&st);
    wchar_t name[96]{};
    swprintf_s(name, L"GTanksNextEditor-%04u%02u%02u-%02u%02u%02u-pid%lu.log",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, GetCurrentProcessId());
    return name;
}

LONG WINAPI CrashFilter(EXCEPTION_POINTERS* info) {
    // A native dump makes post-save/map-transition faults diagnosable. It is
    // written LOCALLY; nothing is uploaded or sent over the network.
    if (info && info->ExceptionRecord) {
        std::filesystem::path dumpPath=g_session;
        if(!dumpPath.empty()) dumpPath.replace_extension(L".dmp");
        if(!dumpPath.empty()) {
            HANDLE file=CreateFileW(dumpPath.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,
                                    CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
            if(file!=INVALID_HANDLE_VALUE) {
                MINIDUMP_EXCEPTION_INFORMATION exception{};
                exception.ThreadId=GetCurrentThreadId();
                exception.ExceptionPointers=info;
                exception.ClientPointers=FALSE;
                MiniDumpWriteDump(GetCurrentProcess(),GetCurrentProcessId(),file,
                                  MiniDumpNormal,&exception,nullptr,nullptr);
                CloseHandle(file);
            }
        }
        // Avoid deadlocking if the failure itself occurred during a log write.
        if(g_mutex.try_lock()) {
            if(g_file.is_open()) {
                g_file << "UNHANDLED WINDOWS EXCEPTION code=0x" << std::hex
                       << info->ExceptionRecord->ExceptionCode << " address=0x"
                       << reinterpret_cast<uintptr_t>(info->ExceptionRecord->ExceptionAddress)
                       << " crashDump=" << PathUtf8(dumpPath) << std::dec << "\r\n";
                g_file.flush();
            }
            g_mutex.unlock();
        }
    }
    if (g_previousFilter) return g_previousFilter(info);
    return EXCEPTION_CONTINUE_SEARCH;
}
} // namespace

std::string WideUtf8(const wchar_t* value) {
    if (!value || !*value) return {};
    const int count = WideCharToMultiByte(CP_UTF8, 0, value, -1, nullptr, 0, nullptr, nullptr);
    if (count <= 1) return {};
    std::string out(static_cast<size_t>(count), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value, -1, out.data(), count, nullptr, nullptr);
    out.resize(static_cast<size_t>(count - 1));
    return out;
}

std::string PathUtf8(const std::filesystem::path& path) {
    return WideUtf8(path.c_str());
}

bool Initialize() {
    std::lock_guard lock(g_mutex);
    if (g_file.is_open()) return true;
    std::error_code ec;
    g_dir = LocalAppData() / L"GTanksNextEditor" / L"logs";
    std::filesystem::create_directories(g_dir, ec);
    if (ec) return false;
    g_session = g_dir / SessionName();
    g_file.open(g_session, std::ios::binary | std::ios::out | std::ios::trunc);
    if (!g_file) return false;
    g_previousFilter = SetUnhandledExceptionFilter(CrashFilter);
    g_file << "[" << Timestamp() << "] [INFO] [tid=" << GetCurrentThreadId() << "] "
           << "Session log opened. version=0.5.17-lossless-object-snapshots build=" << __DATE__ << " " << __TIME__ << "\r\n";
    g_file.flush();
    return true;
}

void Shutdown() {
    std::lock_guard lock(g_mutex);
    if (g_file.is_open()) {
        g_file << "[" << Timestamp() << "] [INFO] [tid=" << GetCurrentThreadId() << "] Session log closed.\r\n";
        g_file.flush();
        g_file.close();
    }
    SetUnhandledExceptionFilter(g_previousFilter);
    g_previousFilter = nullptr;
}

void Write(Level level, std::string_view message) {
    std::lock_guard lock(g_mutex);
    if (!g_file.is_open()) return;
    g_file << "[" << Timestamp() << "] [" << LevelName(level) << "] [tid=" << GetCurrentThreadId() << "] "
           << message << "\r\n";
    if (level == Level::Warning || level == Level::Error) g_file.flush();
}

void Debug(std::string_view m) { Write(Level::Debug, m); }
void Info(std::string_view m) { Write(Level::Info, m); }
void Warning(std::string_view m) { Write(Level::Warning, m); }
void Error(std::string_view m) { Write(Level::Error, m); }
void Flush() { std::lock_guard lock(g_mutex); if (g_file.is_open()) g_file.flush(); }
const std::filesystem::path& SessionFile() { return g_session; }
const std::filesystem::path& LogDirectory() { return g_dir; }

} // namespace Log
