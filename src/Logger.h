#pragma once
#include <filesystem>
#include <string>
#include <string_view>

namespace Log {

enum class Level { Debug, Info, Warning, Error };

bool Initialize();
void Shutdown();
void Write(Level level, std::string_view message);
void Debug(std::string_view message);
void Info(std::string_view message);
void Warning(std::string_view message);
void Error(std::string_view message);
void Flush();

const std::filesystem::path& SessionFile();
const std::filesystem::path& LogDirectory();

std::string PathUtf8(const std::filesystem::path& path);
std::string WideUtf8(const wchar_t* value);

} // namespace Log
