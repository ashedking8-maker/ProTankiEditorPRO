#include "VerifiedMeshSurface.h"
#include "Native3DSWriter.h"
#include "ReleaseTestCheck.h"
#include <fstream>
#include <iostream>
#include <filesystem>
using V=NativeCollisionImport::V;
int main(int argc,char**argv) {
    PT_REQUIRE(argc==2);const auto dir=std::filesystem::path(argv[1]);
    const auto a=VerifiedMeshSurface::ReadVerified(dir/"land01.3ds"),b=VerifiedMeshSurface::ReadVerified(dir/"land02.3ds");
    PT_REQUIRE(a.Valid()&&a.triangles.size()==18&&b.Valid()&&b.triangles.size()==16);
    PT_REQUIRE(!NativeCollisionImport::Read(dir/"land01.3ds").Valid());
    PT_REQUIRE(VerifiedMeshSurface::Read(dir/"land01.3ds").Valid());
    std::ifstream gold(dir/"original_world_triangles.txt");PT_REQUIRE(bool(gold));
    size_t instances=0,triangles=0;int kind;V pos;float yaw;size_t count;
    while(gold>>kind>>pos.x>>pos.y>>pos.z>>yaw>>count) {
        const auto& source=kind==1?a:b;PT_REQUIRE(source.triangles.size()==count);
        std::vector<VerifiedMeshSurface::Corners> wanted;std::vector<VerifiedMeshSurface::Existing> existing;
        for(const auto& t:source.triangles) {
            VerifiedMeshSurface::Corners expected{};for(auto& v:expected)PT_REQUIRE(bool(gold>>v.x>>v.y>>v.z));
            const auto center=NativeCollisionImport::Add(pos,Native3DSScene::Rotate(t.offset,{0,0,yaw}));
            auto rotation=t.rotation;rotation.z+=yaw;
            const auto actual=VerifiedMeshSurface::World(center,rotation,t.v0,t.v1,t.v2);
            PT_REQUIRE(VerifiedMeshSurface::SameCorners(actual,expected));
            PT_REQUIRE(VerifiedMeshSurface::SameWinding(actual,expected));
            wanted.push_back(actual);existing.push_back({expected,-1});++triangles;
        }
        std::vector<int> matches;std::string error;
        PT_REQUIRE(VerifiedMeshSurface::Plan(wanted,existing,7,matches,error));
        PT_REQUIRE(std::count(matches.begin(),matches.end(),-1)==0);
        for(auto& e:existing)e.owner=7;
        PT_REQUIRE(VerifiedMeshSurface::Plan(wanted,existing,7,matches,error)); // repeated repair
        auto partial=existing;partial.pop_back();
        PT_REQUIRE(VerifiedMeshSurface::Plan(wanted,partial,7,matches,error));
        PT_REQUIRE(std::count(matches.begin(),matches.end(),-1)==1); // no duplicate full set
        PT_REQUIRE(VerifiedMeshSurface::Plan(wanted,{},7,matches,error));
        PT_REQUIRE(std::count(matches.begin(),matches.end(),-1)==static_cast<int>(count));
        auto duplicate=existing;duplicate.push_back(existing[0]);
        PT_REQUIRE(!VerifiedMeshSurface::Plan(wanted,duplicate,7,matches,error));
        auto reversed=existing;std::swap(reversed[0].corners[0],reversed[0].corners[1]);
        PT_REQUIRE(!VerifiedMeshSurface::Plan(wanted,reversed,7,matches,error));
        existing[0].owner=8;PT_REQUIRE(!VerifiedMeshSurface::Plan(wanted,existing,7,matches,error));
        ++instances;
    }
    PT_REQUIRE(instances==13&&triangles==230);
    // An edited visual surface must not inherit the old reference certification.
    Native3DSWriter::Model changed;changed.visualName="RenamedVerifiedTerrain";
    const auto& ref=VerifiedMeshSurface::References()[0];
    for(auto v:ref.vertices)changed.vertices.push_back({v.x,v.z,v.y,0,0});
    for(auto f:ref.faces){changed.indices.push_back(f[0]);changed.indices.push_back(f[2]);changed.indices.push_back(f[1]);}
    changed.parts={{0,changed.indices.size(),"mat","land1.jpg"}};
    Native3DSWriter::Bytes bytes;std::string error;
    const auto tmp=std::filesystem::temp_directory_path()/"fogtown_changed_surface_regression.3ds";
    const auto write=[&]()->bool {
        if(!Native3DSWriter::Write(changed,bytes,error))return false;
        std::ofstream stream(tmp,std::ios::binary|std::ios::trunc);
        stream.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));return bool(stream);
    };
    PT_REQUIRE(write());PT_REQUIRE(VerifiedMeshSurface::ReadVerified(tmp).Valid()); // names do not certify a mesh
    changed.vertices[0].x+=1;
    PT_REQUIRE(write());PT_REQUIRE(!VerifiedMeshSurface::ReadVerified(tmp).Valid());std::filesystem::remove(tmp);
    std::cout<<"PASS: 13 original instances, 230 oriented triangles; missing/partial/idempotent repair; duplicate/owner/winding/edited-source rejection\n";
}
