#pragma once
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

struct TextureVariant { std::string name; std::filesystem::path diffuse; };
struct AssetDefinition {
    std::string library;
    std::string group;
    std::string name;
    std::filesystem::path sourceDir;
    std::filesystem::path mesh;
    std::filesystem::path sprite;
    float spriteOriginY = 0.5f;
    float spriteScale = 1.0f;
    std::vector<TextureVariant> textures;
    // Complete, read-only native library source and this prop definition.
    // Known renderer fields are a projection, not a replacement for the source.
    // All assets from one library share the same XML bytes across copies.
    std::shared_ptr<const std::string> originalLibraryXml;
    std::string originalPropXml;

};

class AssetRegistry {
public:
    bool Scan(const std::filesystem::path& libraryRoot, std::string& error);
    const AssetDefinition* Find(const std::string& library, const std::string& group, const std::string& name) const;
    const std::vector<AssetDefinition>& Assets() const { return assets_; }
    size_t AssetCount() const { return assets_.size(); }
    size_t LibraryCount() const { return libraryCount_; }
    const std::filesystem::path& Root() const { return root_; }

private:
    static std::string Key(const std::string&, const std::string&, const std::string&);
    void RebuildLookup();

    std::filesystem::path root_;
    std::vector<AssetDefinition> assets_;
    std::unordered_map<std::string, size_t> lookup_;
    size_t libraryCount_{};
};
