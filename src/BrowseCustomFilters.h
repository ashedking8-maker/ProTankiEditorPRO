#pragma once
#include <pugixml.hpp>
#include <algorithm>
#include <chrono>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif

namespace BrowseCustomFilters {
inline std::string Fold(std::string text) {
    for(auto& c:text)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return text;
}
struct Key {
    std::string library,group,name,texture;
    bool operator==(const Key&)const=default;
    bool operator<(const Key& b)const {
        return std::tie(library,group,name,texture)<std::tie(b.library,b.group,b.name,b.texture);
    }
};
inline Key Canonical(Key key) {return {Fold(key.library),Fold(key.group),Fold(key.name),Fold(key.texture)};}
inline bool ValidText(const std::string& value,size_t limit,bool empty=false) {
    if((value.empty()&&!empty)||value.size()>limit)return false;
    return std::none_of(value.begin(),value.end(),[](unsigned char c){return c<32;});
}
inline bool ValidKey(const Key& k) {
    return ValidText(k.library,1024)&&ValidText(k.group,1024,true)&&ValidText(k.name,1024)&&ValidText(k.texture,1024,true);
}
struct Filter {
    std::string name;std::vector<Key> assets;
    bool Contains(const Key& key)const {return std::binary_search(assets.begin(),assets.end(),Canonical(key));}
};
class Store {
public:
    const std::vector<Filter>& Filters()const {return filters_;}
    bool Add(std::string name,const std::set<Key>& selected,std::string& error) {
        const auto first=name.find_first_not_of(" \t\r\n"),last=name.find_last_not_of(" \t\r\n");
        name=first==std::string::npos?std::string{}:name.substr(first,last-first+1);
        if(!ValidText(name,127)) {error="Enter a filter name (1-127 UTF-8 bytes).";return false;}
        if(filters_.size()>=128 || selected.empty() || selected.size()>50000) {
            error="Use 1-50000 selected objects and at most 128 filters.";return false;
        }
        for(const auto& filter:filters_)if(Fold(filter.name)==Fold(name)) {
            error="A custom filter with this name already exists.";return false;
        }
        std::set<Key> canonical;
        for(const auto& key:selected) {
            if(!ValidKey(key)) {error="A selected library object has an invalid identity.";return false;}
            canonical.insert(Canonical(key));
        }
        filters_.push_back({std::move(name),{canonical.begin(),canonical.end()}});error.clear();return true;
    }
    bool Erase(size_t index,std::string& error) {
        if(index>=filters_.size()) {error="The selected filter no longer exists.";return false;}
        filters_.erase(filters_.begin()+static_cast<std::ptrdiff_t>(index));error.clear();return true;
    }
    bool Load(const std::filesystem::path& path,std::string& error) {
        std::error_code ec;const bool exists=std::filesystem::exists(path,ec);
        if(ec){error="Cannot access custom filters: "+ec.message();return false;}
        if(!exists){error.clear();return true;}
        const auto size=std::filesystem::file_size(path,ec);
        if(ec||size>32u*1024u*1024u){error="Custom filter file exceeds the size limit or cannot be read.";return false;}
        std::ifstream in(path,std::ios::binary);
        std::string text((std::istreambuf_iterator<char>(in)),{});
        if(!in || text.size()!=size){error="Cannot read custom filters.";return false;}
        pugi::xml_document doc;
        if(!doc.load_buffer(text.data(),text.size()) || std::string(doc.child("browse-filters").attribute("version").value())!="1") {
            error="Custom filter file is malformed or uses an unsupported version.";return false;
        }
        Store candidate;
        for(auto filter:doc.child("browse-filters").children()) {
            if(std::string(filter.name())!="filter") {error="Unrecognized custom filter record.";return false;}
            std::set<Key> keys;
            for(auto asset:filter.children()) {
                if(std::string(asset.name())!="asset") {error="Unrecognized custom filter object.";return false;}
                keys.insert({asset.attribute("library").value(),asset.attribute("group").value(),asset.attribute("name").value(),asset.attribute("texture").value()});
                if(keys.size()>50000){error="Custom filter has too many objects.";return false;}
            }
            if(!candidate.Add(filter.attribute("name").value(),keys,error))return false;
        }
        filters_=std::move(candidate.filters_);error.clear();return true;
    }
    bool Save(const std::filesystem::path& path,std::string& error)const {
        if(path.empty()){error="Cannot find the custom filter settings location.";return false;}
        pugi::xml_document doc;auto root=doc.append_child("browse-filters");root.append_attribute("version")="1";
        for(const auto& filter:filters_) {
            auto n=root.append_child("filter");n.append_attribute("name")=filter.name.c_str();
            for(const auto& key:filter.assets) {
                auto item=n.append_child("asset");item.append_attribute("library")=key.library.c_str();
                item.append_attribute("group")=key.group.c_str();item.append_attribute("name")=key.name.c_str();item.append_attribute("texture")=key.texture.c_str();
            }
        }
        std::ostringstream encoded;doc.save(encoded,"  ",pugi::format_default,pugi::encoding_utf8);
        const auto bytes=encoded.str();
        if(bytes.size()>32u*1024u*1024u){error="Custom filters exceed the file size limit.";return false;}
        std::error_code ec;
        if(!path.parent_path().empty())std::filesystem::create_directories(path.parent_path(),ec);
        if(ec){error="Cannot create custom filter directory: "+ec.message();return false;}
        auto temp=path;temp+="."+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".tmp";
        const auto cleanup=[&]{std::error_code ignored;std::filesystem::remove(temp,ignored);};
        {std::ofstream out(temp,std::ios::binary);out.write(bytes.data(),static_cast<std::streamsize>(bytes.size()));out.flush();
         if(!out){cleanup();error="Cannot write custom filters.";return false;}}
#ifdef _WIN32
        if(!MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) {
            const auto code=GetLastError();cleanup();error="Cannot replace custom filter file (Windows error "+std::to_string(code)+").";return false;
        }
#else
        std::filesystem::rename(temp,path,ec);
        if(ec){cleanup();error="Cannot replace custom filter file: "+ec.message();return false;}
#endif
        error.clear();return true;
    }
private:
    std::vector<Filter> filters_;
};
}
