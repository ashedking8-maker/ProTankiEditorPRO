#pragma once
#include <filesystem>
#include <string>
#include <utility>

namespace BugReport {
struct Result {bool accepted{};std::string message;};
bool Configured();
// Subject/details are deliberately user-authored. Only voluntarily selected
// latest log snippets are sent. No XML, 3DS, screenshot or crash dump.
Result Submit(std::string subject,std::string description,bool attachLogs);
std::string RedactLog(std::string text);
std::string EscapeJson(const std::string& value);
} // namespace BugReport
