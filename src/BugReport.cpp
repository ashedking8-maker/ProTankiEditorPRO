#include "BugReport.h"
#include "BugReportConfig.h"
#include "Logger.h"
#include <windows.h>
#include <winhttp.h>
#include <bcrypt.h>
#include <shlobj.h>
#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <regex>
#include <sstream>
#include <vector>

namespace BugReport {
namespace fs=std::filesystem;
namespace {
constexpr size_t kMaxSubject=160,kMaxDetails=4000,kMaxLog=32768,kMaxBody=100000;
struct AutoHandle { HINTERNET h{};explicit AutoHandle(HINTERNET value=nullptr):h(value){}~AutoHandle(){if(h)WinHttpCloseHandle(h);}AutoHandle(const AutoHandle&)=delete;AutoHandle& operator=(const AutoHandle&)=delete;};
fs::path ConfigDir(){
    PWSTR path=nullptr;fs::path dir;
    if(SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData,KF_FLAG_CREATE,nullptr,&path))&&path){dir=path;CoTaskMemFree(path);}
    return dir.empty()?fs::path{}:dir/L"GTanksNextEditor";
}
std::string InstallationId(){
    const auto dir=ConfigDir();if(dir.empty())return {};
    std::error_code ec;fs::create_directories(dir,ec);if(ec)return {};
    const auto target=dir/L"bug-report-install-id.txt";
    std::string id;{std::ifstream in(target,std::ios::binary);in>>id;}
    if(id.size()==32&&std::all_of(id.begin(),id.end(),[](unsigned char c){return std::isxdigit(c)!=0;}))return id;
    std::array<unsigned char,16> random{};
    if(BCryptGenRandom(nullptr,random.data(),static_cast<ULONG>(random.size()),BCRYPT_USE_SYSTEM_PREFERRED_RNG)<0)return {};
    std::ostringstream out;out<<std::hex<<std::setfill('0');for(unsigned char c:random)out<<std::setw(2)<<unsigned(c);
    id=out.str();std::ofstream save(target,std::ios::binary|std::ios::trunc);save<<id;save.flush();return save?id:std::string{};
}
std::string RecentLogs(){
    Log::Flush();const auto dir=Log::LogDirectory();
    std::error_code ec;std::vector<fs::path> files;
    for(fs::directory_iterator it(dir,ec),end;!ec&&it!=end;it.increment(ec)){
        if(it->is_regular_file()&&it->path().extension()==L".log")files.push_back(it->path());
    }
    if(ec)return {};
    std::sort(files.begin(),files.end(),[](const auto& a,const auto& b){
        std::error_code ea,eb;return fs::last_write_time(a,ea)>fs::last_write_time(b,eb);
    });
    std::string result;
    for(size_t i=0;i<std::min(size_t{2},files.size());++i){
        std::ifstream in(files[i],std::ios::binary|std::ios::ate);
        if(!in)continue;const auto end=in.tellg();if(end<=0)continue;
        const auto from=std::max<std::streamoff>(0,static_cast<std::streamoff>(end)-static_cast<std::streamoff>(kMaxLog));
        in.seekg(from);
        std::string chunk(static_cast<size_t>(static_cast<std::streamoff>(end)-from),'\0');
        in.read(chunk.data(),static_cast<std::streamsize>(chunk.size()));chunk.resize(static_cast<size_t>(in.gcount()));
        result+="\n--- Recent session log (sanitized, tail) ---\n"+RedactLog(std::move(chunk));
    }
    return result;
}
Result Upload(const std::string& json){
    // Reject missing endpoint and non-HTTPS schemes; never redirect to HTTP.
    const std::string endpoint=kBugReportEndpoint;
    if(endpoint.empty())return {false,"Bug report gateway is not configured. Contact the project maintainer."};
    if(endpoint.rfind("https://",0)!=0||endpoint.size()>2048)return {false,"Bug report endpoint must be HTTPS."};
    const int length=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,endpoint.c_str(),-1,nullptr,0);
    if(length<=1)return {false,"Invalid gateway URL."};
    std::wstring url(static_cast<size_t>(length),L'\0');
    MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,endpoint.c_str(),-1,url.data(),length);url.resize(static_cast<size_t>(length-1));
    URL_COMPONENTS parts{};parts.dwStructSize=sizeof(parts);parts.dwHostNameLength=1;parts.dwUrlPathLength=1;parts.dwExtraInfoLength=1;
    if(!WinHttpCrackUrl(url.c_str(),static_cast<DWORD>(url.size()),0,&parts)||parts.nScheme!=INTERNET_SCHEME_HTTPS)
        return {false,"Invalid HTTPS gateway URL."};
    const std::wstring host(parts.lpszHostName,parts.dwHostNameLength);
    std::wstring path(parts.lpszUrlPath,parts.dwUrlPathLength);
    if(parts.dwExtraInfoLength)path.append(parts.lpszExtraInfo,parts.dwExtraInfoLength);
    AutoHandle session(WinHttpOpen(L"ProTankiEditorPRO-BugReport/0.5.26",WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0));
    if(!session.h)return {false,"Cannot initialize secure report connection."};
    WinHttpSetTimeouts(session.h,4000,4000,6000,6000);
    AutoHandle connection(WinHttpConnect(session.h,host.c_str(),parts.nPort,0));
    if(!connection.h)return {false,"Cannot connect to report gateway."};
    AutoHandle request(WinHttpOpenRequest(connection.h,L"POST",path.c_str(),nullptr,WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE));
    if(!request.h)return {false,"Cannot create secure bug report request."};
    DWORD noRedirect=WINHTTP_DISABLE_REDIRECTS;
    if(!WinHttpSetOption(request.h,WINHTTP_OPTION_DISABLE_FEATURE,&noRedirect,sizeof(noRedirect)))
        return {false,"Cannot enforce secure report redirect policy."};
    const wchar_t* headers=L"Content-Type: application/json; charset=utf-8\r\nAccept: application/json\r\n";
    if(!WinHttpSendRequest(request.h,headers,static_cast<DWORD>(-1),
        const_cast<char*>(json.data()),static_cast<DWORD>(json.size()),static_cast<DWORD>(json.size()),0)||
       !WinHttpReceiveResponse(request.h,nullptr))return {false,"Could not deliver report over HTTPS. Try again later."};
    DWORD status=0,size=sizeof(status);
    if(!WinHttpQueryHeaders(request.h,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX,&status,&size,WINHTTP_NO_HEADER_INDEX))return {false,"No report gateway status."};
    if(status==202)return {true,"Report received. Repeated submissions may be grouped."};
    if(status==400||status==413)return {false,"The report was rejected due to invalid or oversized content."};
    return {false,"Report gateway did not accept the report. Please try later."};
}
} // namespace
bool Configured(){return kBugReportEndpoint[0]!='\0';}
std::string EscapeJson(const std::string& value){
    const char* hex="0123456789abcdef";std::string out;out.reserve(value.size()+16);
    for(unsigned char c:value){switch(c){
        case '"':out+="\\\"";break;case '\\':out+="\\\\";break;case '\n':out+="\\n";break;
        case '\r':out+="\\r";break;case '\t':out+="\\t";break;
        default: if(c<32){out+="\\u00";out+=hex[c>>4];out+=hex[c&15];}else out+=char(c);
    }}return out;
}
std::string RedactLog(std::string text){
    // Privacy-first: remove whole lines containing likely credentials then
    // redact usernames/absolute paths and email addresses. Never upload dumps.
    std::istringstream input(std::move(text));std::string line,result;
    const std::regex email(R"([A-Za-z0-9._%+\-]+@[A-Za-z0-9.\-]+\.[A-Za-z]{2,})");
    const std::regex windowsPath(R"([A-Za-z]:\\[^\s\"<>]+)");
    const std::regex userPath(R"((?:[A-Za-z]:\\)?Users\\[^\\\s]+)",std::regex::icase);
    const std::regex credential(R"((?:password|bearer|authorization|api[_-]?key|access[_-]?token|secret|cookie)[\s=:])",std::regex::icase);
    while(std::getline(input,line)){
        if(std::regex_search(line,credential)){result+="[sensitive log line omitted]\n";continue;}
        line=std::regex_replace(line,email,"[email]");
        line=std::regex_replace(line,windowsPath,"[local path]");
        line=std::regex_replace(line,userPath,"[local user]");
        if(line.size()>2048)line.resize(2048);
        result+=line+"\n";
    }
    if(result.size()>2*kMaxLog)result.erase(0,result.size()-2*kMaxLog);
    return result;
}
Result Submit(std::string subject,std::string description,bool attachLogs){
    if(subject.empty()||subject.size()>kMaxSubject||description.empty()||description.size()>kMaxDetails)
        return {false,"Please enter a subject and a description within the limits."};
    const auto id=InstallationId();if(id.empty())return {false,"Cannot store installation report ID."};
    const std::string logs=attachLogs?RecentLogs():std::string{};
    const std::string json="{\"version\":\"0.5.26\",\"client_id\":\""+EscapeJson(id)+
        "\",\"subject\":\""+EscapeJson(subject)+"\",\"description\":\""+EscapeJson(description)+
        "\",\"logs_opt_in\":"+(attachLogs?"true":"false")+",\"logs\":\""+EscapeJson(logs)+"\"}";
    if(json.size()>kMaxBody)return {false,"Report is too large. Disable logs and retry."};
    return Upload(json);
}
} // namespace BugReport
