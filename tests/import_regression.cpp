#include "LegacyMeshImport.h"
#include <iostream>
#include <array>
#include <stdexcept>
using namespace LegacyMeshImport;
void Require(bool ok,const char* message) { if (!ok) throw std::runtime_error(message); }
std::array<float,6> Bounds(const Model& m) {
    std::array<float,6> b{1e30f,1e30f,1e30f,-1e30f,-1e30f,-1e30f};
    for (const auto& v:m.vertices) for(int i=0;i<3;++i) {
        b[i]=std::min(b[i],v.position[i]);b[i+3]=std::max(b[i+3],v.position[i]);
    }
    return b;
}
void Check(const std::filesystem::path& path,const std::array<float,6>& expected,size_t triangles,size_t ignoredHelpers) {
    const auto model=Load(path);const auto b=Bounds(model);
    for(int i=0;i<6;++i) Require(std::abs(b[i]-expected[i])<0.1f,"Incorrect local bounds");
    Require(!model.parts.empty(),"No material parts");
    Require(model.indices.size()/3==triangles,"Collision/helper triangles leaked into render mesh");
    Require(model.ignoredMeshNodes==ignoredHelpers,"Unexpected helper-node count");
    bool textureFound=false;
    size_t count=0;
    for(const auto& p:model.parts) {
        count+=p.indexCount;
        Require(size_t(p.firstIndex)+p.indexCount<=model.indices.size(),"Invalid part range");
        if(!p.diffuse.empty()) { Require(std::filesystem::exists(p.diffuse),"Missing material texture");textureFound=true; }
    }
    Require(textureFound,"Embedded diffuse texture not resolved");
    Require(count==model.indices.size(),"Lost visual faces");
    for(auto i:model.indices) Require(i<model.vertices.size(),"Invalid vertex index");
    std::cout<<path.filename()<<" anchor="<<model.anchor<<" helpers="<<model.ignoredMeshNodes<<" triangles="<<triangles<<" bounds=";
    for(auto v:b)std::cout<<v<<",";std::cout<<" PASS\n";
}
int main(int argc,char** argv) {
    try {
        const std::filesystem::path root=argc>1?argv[1]:"tests/fixtures";
        Check(root/"LandTiles/tile_01.3ds",{-250,0,-250,250,0,250},2,1);
        Check(root/"IndustrialBridge/brid_7.3ds",{-250,250,-250,250,600,250},8,6);
        // This deliberately asymmetric mesh catches the old mirrored-Z basis:
        // (x,z,-y) produced [-140,250] in internal Z; (x,z,y) must produce
        // [-250,140] while X/Y and the original visual/helper counts stay fixed.
        Check(root/"Stuffs/chest02.3ds",{-250,0,-250,140,260,140},43,4);
        bool rejected=false;
        try { Load(root/"missing.3ds"); } catch(const std::exception&) {rejected=true;}
        Require(rejected,"Missing mesh must fail");
    } catch(const std::exception& e) {std::cerr<<e.what()<<"\n";return 1;}
    return 0;
}
