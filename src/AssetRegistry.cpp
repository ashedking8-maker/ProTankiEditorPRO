#include "AssetRegistry.h"
#include "Logger.h"
#include <pugixml.hpp>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

std::string AssetRegistry::Key(const std::string& a, const std::string& b, const std::string& c) {
    std::string k = a + '\x1f' + b + '\x1f' + c;
    std::transform(k.begin(), k.end(), k.begin(), [](unsigned char ch){ return static_cast<char>(std::tolower(ch)); });
    return k;
}

void AssetRegistry::RebuildLookup() {
    lookup_.clear();
    lookup_.reserve(assets_.size());
    for (size_t i = 0; i < assets_.size(); ++i) lookup_.insert_or_assign(Key(assets_[i].library, assets_[i].group, assets_[i].name), i);
}

bool AssetRegistry::Scan(const std::filesystem::path& libraryRoot, std::string& error) {
    // Prepare a complete replacement before mutating the live index. A typo,
    // malformed library.xml or unreadable folder must not discard the user's
    // already loaded library while an edited map is open.
    std::vector<AssetDefinition> candidateAssets;
    size_t candidateLibraryCount = 0;
    Log::Info("Asset scan begin. root=" + Log::PathUtf8(libraryRoot));
    std::error_code ec;
    if (!std::filesystem::is_directory(libraryRoot,ec) || ec) {
        error = "Library directory does not exist or cannot be accessed.";
        Log::Error(error + " root=" + Log::PathUtf8(libraryRoot)); return false;
    }

    for (const auto& entry : std::filesystem::directory_iterator(libraryRoot,ec)) {
        if(ec) break;
        if (!entry.is_directory()) continue;
        const auto xml = entry.path() / "library.xml";
        if (!std::filesystem::exists(xml)) continue;

        std::ifstream input(xml, std::ios::binary);
        if (!input) { Log::Warning("Could not open library.xml: " + Log::PathUtf8(xml)); continue; }
        std::ostringstream rawInput;
        rawInput << input.rdbuf();
        if (input.bad()) { Log::Warning("Could not read library.xml: " + Log::PathUtf8(xml)); continue; }
        auto originalXml=std::make_shared<const std::string>(rawInput.str());
        pugi::xml_document doc;
        if (!doc.load_buffer(originalXml->data(),originalXml->size(),pugi::parse_full,pugi::encoding_auto)) {
            Log::Warning("Could not parse library.xml: " + Log::PathUtf8(xml)); continue;
        }
        auto root = doc.child("library");
        if (!root) { Log::Warning("library.xml missing <library>: " + Log::PathUtf8(xml)); continue; }
        const std::string libName = root.attribute("name").as_string(entry.path().filename().string().c_str());
        ++candidateLibraryCount;
        Log::Debug("Library indexed: " + libName + " from " + Log::PathUtf8(entry.path()));

        for (auto group : root.children("prop-group")) {
            const std::string groupName = group.attribute("name").as_string();
            for (auto prop : group.children("prop")) {
                AssetDefinition a;
                a.library = libName;
                a.group = groupName;
                a.name = prop.attribute("name").as_string();
                a.sourceDir = entry.path();
                a.originalLibraryXml=originalXml;
                std::ostringstream rawProp;
                prop.print(rawProp,"",pugi::format_raw,pugi::encoding_utf8);
                a.originalPropXml=rawProp.str();
                if (auto mesh = prop.child("mesh")) {
                    a.mesh = entry.path() / mesh.attribute("file").as_string();
                    for (auto tex : mesh.children("texture")) {
                        a.textures.push_back({tex.attribute("name").as_string(), entry.path() / tex.attribute("diffuse-map").as_string()});
                    }
                }
                if (auto sprite = prop.child("sprite")) {
                    a.sprite = entry.path() / sprite.attribute("file").as_string();
                    a.spriteOriginY = sprite.attribute("origin-y").as_float(0.5f);
                    a.spriteScale = sprite.attribute("scale").as_float(1.0f);
                }
                candidateAssets.push_back(std::move(a));
            }
        }
    }
    if(ec) {error="Cannot enumerate selected library: "+ec.message(); Log::Error(error); return false;}
    if (candidateAssets.empty()) { error = "No valid library.xml assets were found."; Log::Error(error + " root=" + Log::PathUtf8(libraryRoot)); return false; }

    std::sort(candidateAssets.begin(), candidateAssets.end(), [](const AssetDefinition& a, const AssetDefinition& b) {
        if (a.library != b.library) return a.library < b.library;
        if (a.group != b.group) return a.group < b.group;
        return a.name < b.name;
    });
    assets_=std::move(candidateAssets);
    libraryCount_=candidateLibraryCount;
    root_=libraryRoot;
    RebuildLookup();
    Log::Info("Asset scan complete. libraries=" + std::to_string(libraryCount_) + " assets=" + std::to_string(assets_.size()));
    error.clear();
    return true;
}

const AssetDefinition* AssetRegistry::Find(const std::string& library, const std::string& group, const std::string& name) const {
    const auto it = lookup_.find(Key(library, group, name));
    return it == lookup_.end() ? nullptr : &assets_[it->second];
}
