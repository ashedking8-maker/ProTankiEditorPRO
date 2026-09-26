#pragma once
// GTanks TARA variant verified against the original user-supplied library set.
// Header: BE u32 count, then BE u16 filename-length + UTF-8 filename + BE u32 byte-size;
// payloads follow in header order. This is NOT a ZIP and contains no compression.
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace NativeTaraWriter {
using Bytes=std::vector<std::uint8_t>;
using Entry=std::pair<std::string,Bytes>;
inline void U16(Bytes& b,std::uint16_t v){b.push_back(std::uint8_t(v>>8));b.push_back(std::uint8_t(v));}
inline void U32(Bytes& b,std::uint32_t v){for(int shift=24;shift>=0;shift-=8)b.push_back(std::uint8_t(v>>shift));}
inline std::uint32_t Get32(const Bytes& b,size_t at){return (std::uint32_t(b[at])<<24)|(std::uint32_t(b[at+1])<<16)|(std::uint32_t(b[at+2])<<8)|b[at+3];}
inline std::uint16_t Get16(const Bytes& b,size_t at){return std::uint16_t((unsigned(b[at])<<8)|b[at+1]);}
inline bool ValidName(const std::string& name){
    if(name.empty()||name.size()>65535||name=="."||name=="..")return false;
    for(unsigned char c:name)if(c<32||c=='/'||c=='\\'||c==':')return false;
    return true;
}
inline std::string Fold(std::string s){for(char& c:s)if(c>='A'&&c<='Z')c=char(c+('a'-'A'));return s;}
inline bool Encode(const std::vector<Entry>& entries,Bytes& encoded,std::string& error){
    encoded.clear();if(entries.empty()||entries.size()>512){error="Invalid TARA entry count.";return false;}
    std::vector<std::string> names;names.reserve(entries.size());
    std::uint64_t total=4;
    for(const auto& e:entries){
        if(!ValidName(e.first)||e.second.size()>64u*1024u*1024u){error="Unsafe/oversized TARA file.";return false;}
        const auto folded=Fold(e.first);
        if(std::find(names.begin(),names.end(),folded)!=names.end()){error="Duplicate TARA filename.";return false;}
        names.push_back(folded);total+=2+e.first.size()+4+e.second.size();
    }
    if(total>128u*1024u*1024u){error="TARA exceeds 128MB size limit.";return false;}
    encoded.reserve(static_cast<size_t>(total));U32(encoded,static_cast<std::uint32_t>(entries.size()));
    for(const auto& e:entries){U16(encoded,static_cast<std::uint16_t>(e.first.size()));encoded.insert(encoded.end(),e.first.begin(),e.first.end());U32(encoded,static_cast<std::uint32_t>(e.second.size()));}
    for(const auto& e:entries)encoded.insert(encoded.end(),e.second.begin(),e.second.end());
    error.clear();return true;
}
inline bool Decode(const Bytes& encoded,std::vector<Entry>& entries,std::string& error){
    entries.clear();if(encoded.size()<4||encoded.size()>128u*1024u*1024u){error="Invalid TARA size.";return false;}
    size_t pos=4;const auto count=Get32(encoded,0);if(!count||count>512){error="Invalid TARA record count.";return false;}
    std::vector<std::pair<std::string,size_t>> metadata;
    for(std::uint32_t i=0;i<count;++i){
        if(pos+2>encoded.size()){error="Truncated TARA name length.";return false;}
        const auto length=Get16(encoded,pos);pos+=2;
        if(!length||pos+length+4>encoded.size()){error="Truncated TARA record.";return false;}
        std::string name(reinterpret_cast<const char*>(encoded.data()+pos),length);pos+=length;
        const auto size=Get32(encoded,pos);pos+=4;
        if(!ValidName(name)){error="Unsafe TARA entry name.";return false;}
        metadata.emplace_back(std::move(name),size);
    }
    for(const auto& item:metadata){
        if(item.second>encoded.size()-pos){error="Truncated TARA file bytes.";entries.clear();return false;}
        entries.emplace_back(item.first,Bytes(encoded.begin()+static_cast<std::ptrdiff_t>(pos),encoded.begin()+static_cast<std::ptrdiff_t>(pos+item.second)));
        pos+=item.second;
    }
    if(pos!=encoded.size()){error="Unexpected TARA trailing bytes.";entries.clear();return false;}
    error.clear();return true;
}
inline bool PackDirectory(const std::filesystem::path& folder,const std::filesystem::path& target,std::string& error){
    namespace fs=std::filesystem;
    std::error_code ec;std::vector<fs::path> files;
    for(fs::directory_iterator it(folder,ec),end;!ec&&it!=end;it.increment(ec)){
        if(!it->is_regular_file()||it->path().extension()==".tara")continue;
        files.push_back(it->path());
    }
    if(ec){error="Could not list exported library files.";return false;}
    std::sort(files.begin(),files.end(),[](const auto& a,const auto& b){return Fold(a.filename().string())<Fold(b.filename().string());});
    if(std::none_of(files.begin(),files.end(),[](const auto& f){return f.filename()=="library.xml";})){error="TARA is missing library.xml.";return false;}
    std::vector<Entry> entries;entries.reserve(files.size());
    for(const auto& file:files){
        const auto n=file.filename().string();if(!ValidName(n)){error="Invalid library filename.";return false;}
        const auto size=fs::file_size(file,ec);if(ec||size>64u*1024u*1024u){error="Oversized/invalid library file.";return false;}
        std::ifstream in(file,std::ios::binary);if(!in){error="Cannot open library file.";return false;}
        Bytes content((std::istreambuf_iterator<char>(in)),{});
        if(content.size()!=size){error="Short library file read.";return false;}
        entries.emplace_back(n,std::move(content));
    }
    Bytes encoded; if(!Encode(entries,encoded,error))return false;
    std::vector<Entry> check;if(!Decode(encoded,check,error)||check!=entries){error="TARA self-check failed.";return false;}
    std::ofstream out(target,std::ios::binary|std::ios::trunc);
    out.write(reinterpret_cast<const char*>(encoded.data()),static_cast<std::streamsize>(encoded.size()));out.flush();
    if(!out){error="Could not write TARA package.";return false;}
    return true;
}
} // namespace NativeTaraWriter
