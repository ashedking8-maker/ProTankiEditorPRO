#include "MeshDerivedCollision.h"
#include "NativeCollisionImport.h"
#include "ReleaseTestCheck.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>

namespace fs=std::filesystem;
using namespace NativeCollisionImport;
int main(int argc,char**argv) {
    PT_REQUIRE(argc==3);
    const fs::path models=argv[1], helpers=argv[2];
    for(const auto& [name,faceCount] : std::vector<std::pair<std::string,size_t>>{
        {"env5.3ds",10},{"env6.3ds",10},{"v1.3ds",38},{"peter.3ds",3}}) {
        const auto path=models/name;
        Native3DSScene::Scene scene;Native3DSScene::Selection selected;std::string error;
        PT_REQUIRE(Native3DSScene::Read(path,scene,error));
        PT_REQUIRE(Native3DSScene::Resolve(scene,"",selected,error));
        const auto& node=scene.nodes[selected.node];
        PT_REQUIRE(node.faces.size()>=faceCount);
        const auto result=MeshDerivedCollision::Read(path);
        PT_REQUIRE(result.Valid());PT_REQUIRE(result.meshDerived);
        PT_REQUIRE(result.triangles.size()+result.ignoredDegenerateFaces==node.faces.size());
        PT_REQUIRE((name!="peter.3ds") || result.ignoredDegenerateFaces>0);
        const auto* frame=Native3DSScene::SelectedFrame(scene,selected);
        size_t tri=0;
        for(const auto& face:node.faces) {
            V source[3]{};
            for(int i=0;i<3;++i) {
                PT_REQUIRE(Native3DSScene::LocalVertex(node,frame,node.vertices[face[i]],source[i]));
                if(frame)source[i]=Add(Native3DSScene::Rotate(source[i],frame->rotation),frame->position);
            }
            if(Norm(Sub(source[1],source[0]))<.01f ||
               Norm(Cross(Sub(source[1],source[0]),Sub(source[2],source[0])))<.01f)continue;
            PT_REQUIRE(tri<result.triangles.size());
            const auto& t=result.triangles[tri++];
            for(int i=0;i<3;++i) {
                const auto local=i==0?t.v0:i==1?t.v1:t.v2;
                const auto restored=Add(Native3DSScene::Rotate(local,t.rotation),t.offset);
                const auto d=Norm(Sub(restored,source[i]));
                if(d>=.09f)std::cerr<<name<<" face="<<tri-1<<" corner="<<i<<" distance="<<d<<"\n";
                PT_REQUIRE(d<.09f);
            }
        }
        PT_REQUIRE(tri==result.triangles.size());
    }
    // Don't replace actual native helper collision with visual triangles.
    const auto helperFile=helpers/"wall_e1.3ds";
    const auto native=NativeCollisionImport::Read(helperFile);
    PT_REQUIRE(native.Valid()&&native.planes.size()==6&&native.triangles.size()==10);
    const auto rejected=MeshDerivedCollision::Read(helperFile);
    PT_REQUIRE(!rejected.Valid()&&!rejected.error.empty());
    // Corruption must fail rather than create collision from arbitrary bytes.
    auto bytesPath=models/"env5.3ds";
    std::ifstream in(bytesPath,std::ios::binary);
    std::vector<char> bytes((std::istreambuf_iterator<char>(in)),{});
    PT_REQUIRE(bytes.size()>10);
    bytes[0]=0;
    const auto tmp=fs::temp_directory_path()/"ptpro_mesh_derived_corrupt_3ds_test.3ds";
    {std::ofstream out(tmp,std::ios::binary|std::ios::trunc);out.write(bytes.data(),static_cast<std::streamsize>(bytes.size()));}
    PT_REQUIRE(!MeshDerivedCollision::Read(tmp).Valid());
    fs::remove(tmp);
    std::cout<<"PASS: Fogtown env5/env6/v1/peter, triangle roundtrip, degenerate faces, native helper priority, corrupt input\n";
}
