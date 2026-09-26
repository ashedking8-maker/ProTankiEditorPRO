#include "AssetRegistry.h"
#include <iostream>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

int main() {
    namespace fs=std::filesystem;
    const auto unique=std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const fs::path scratch=fs::temp_directory_path()/("protlvk_library_reload_"+unique);
    std::error_code ec;
    auto require=[](bool okay,const char* why) {
        if(!okay) std::cerr<<"FAIL library reload: "<<why<<'\n';
        return okay;
    };
    auto write=[](const fs::path& file,const char* xml) {
        fs::create_directories(file.parent_path());
        std::ofstream stream(file,std::ios::binary); if(!stream) throw std::runtime_error("cannot write fixture");
        stream<<xml; if(!stream) throw std::runtime_error("cannot write fixture");
    };
    const auto first=scratch/"first";
    const auto damaged=scratch/"damaged";
    const auto replacement=scratch/"replacement";
    write(first/"Tiles"/"library.xml",
        "<library name=\"Tiles\"><prop-group name=\"ground\"><prop name=\"Tile\"><mesh file=\"tile.3ds\"/></prop></prop-group></library>");
    write(damaged/"Broken"/"library.xml","<library><broken");
    write(replacement/"Walls"/"library.xml",
        "<library name=\"Walls\"><prop-group name=\"solid\"><prop name=\"Wall\"><mesh file=\"wall.3ds\"/></prop></prop-group></library>");
    AssetRegistry registry;std::string error;
    if(!require(registry.Scan(first,error),"first load"))return 1;
    if(!require(registry.AssetCount()==1 && registry.LibraryCount()==1,"initial asset count"))return 2;
    const auto* initial=registry.Find("Tiles","ground","Tile");
    if(!require(initial!=nullptr,"initial asset lookup"))return 3;
    if(!require(initial->originalLibraryXml && *initial->originalLibraryXml==
        "<library name=\"Tiles\"><prop-group name=\"ground\"><prop name=\"Tile\"><mesh file=\"tile.3ds\"/></prop></prop-group></library>",
        "full library source bytes missing"))return 12;
    if(!require(initial->originalPropXml.find("<mesh file=\"tile.3ds\"")!=std::string::npos,
        "complete prop definition missing"))return 13;
    auto persistentSource=initial->originalLibraryXml;
    // Failed scans must not mutate the usable old library or its root.
    if(!require(!registry.Scan(scratch/"missing",error),"missing folder fails"))return 4;
    if(!require(registry.Root()==first && registry.Find("Tiles","ground","Tile")!=nullptr,"old index after missing folder"))return 5;
    if(!require(!registry.Scan(damaged,error),"malformed XML fails"))return 6;
    if(!require(registry.Root()==first && registry.AssetCount()==1,"old index after malformed XML"))return 7;
    if(!require(registry.Find("Tiles","ground","Tile")->originalLibraryXml==persistentSource,
        "failed rescan replaced original metadata"))return 14;
    if(!require(registry.Scan(replacement,error),"valid replacement load"))return 8;
    if(!require(registry.Root()==replacement && registry.AssetCount()==1,"replacement index count"))return 9;
    if(!require(registry.Find("Walls","solid","Wall")!=nullptr,"replacement lookup"))return 10;
    if(!require(registry.Find("Tiles","ground","Tile")==nullptr,"stale index cleared on success"))return 11;
    if(!require(persistentSource && persistentSource->find("tile.3ds")!=std::string::npos,
        "old read-only library snapshot was mutated on rescan"))return 15;
    fs::remove_all(scratch,ec);
    std::cout<<"PASS: library scanning is transactional\n";
    return 0;
}
