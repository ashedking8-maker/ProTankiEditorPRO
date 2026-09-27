#include "Native3DSWriter.h"
#include "Native3DSVisualMetadata.h"
#include "ReleaseTestCheck.h"
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>

int main(){
    namespace w=Native3DSWriter;
    namespace v=Native3DSVisualMetadata;
    w::Model m;
    m.visualName="ptpro_mesh";
    m.vertices={{0,0,0,0,0},{10,0,0,1,0},{0,0,10,0,1},{10,0,10,1,1}};
    m.indices={0,1,2,1,3,2};
    m.smoothingGroups={1u,4u};
    m.parts={{0,6,"Original Material","original.jpg"}};
    w::Bytes matName,tex,map,matBody,unknown;
    w::CStr(matName,"Original Material");w::Add(matBody,w::Chunk(0xA000,matName));
    w::CStr(tex,"original.jpg");w::Add(map,w::Chunk(0xA300,tex));w::Add(matBody,w::Chunk(0xA200,map));
    w::U16(unknown,0x1234);w::Add(matBody,w::Chunk(0xBEEF,unknown));
    w::Bytes shade;w::U16(shade,3);w::Add(matBody,w::Chunk(0xA100,shade));
    m.nativeMaterials={w::Chunk(0xAFFF,matBody)};
    w::Bytes bytes;std::string error;
    PT_REQUIRE(w::Write(m,bytes,error));
    const auto scratch=std::filesystem::temp_directory_path()/
        ("ptpro_0526_smoothing_"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".3ds");
    const auto write=[&](const w::Bytes& data){
        std::ofstream out(scratch,std::ios::binary|std::ios::trunc);
        out.write(reinterpret_cast<const char*>(data.data()),static_cast<std::streamsize>(data.size()));
        return static_cast<bool>(out);
    };
    PT_REQUIRE(write(bytes));
    v::Visual raw;PT_REQUIRE(v::Read(scratch,"ptpro_mesh",raw,error));
    PT_REQUIRE(raw.hasSmoothing && raw.faces.size()==2 && raw.faces[0].smoothing==1 && raw.faces[1].smoothing==4);
    PT_REQUIRE(raw.materials.size()==1 && raw.materials[0].raw==m.nativeMaterials.front() &&
        raw.materials[0].name=="Original Material" && raw.materials[0].texture=="original.jpg");
    std::vector<std::uint32_t> groups;std::vector<std::string> materials;
    // Writer reverses winding; mapping MUST NOT depend on face winding or order.
    PT_REQUIRE(v::Match(raw,m,groups,materials,error));
    PT_REQUIRE(groups==m.smoothingGroups && materials.size()==2 &&
        materials[0]=="Original Material" && materials[1]=="Original Material");
    auto swapped=m;swapped.indices={1,3,2,0,1,2};
    PT_REQUIRE(v::Match(raw,swapped,groups,materials,error));
    PT_REQUIRE(groups.size()==2 && groups[0]==4 && groups[1]==1);
    auto invalid=m;invalid.smoothingGroups={1};
    PT_REQUIRE(!w::Write(invalid,bytes,error));
    // Native overrides replace only documented chunks, keeping unknown vendor
    // data and the original texture mapping; opt-out is byte-identical.
    auto edited=m;edited.materialOverride.enabled=true;
    edited.materialOverride.ambient={.25f,.5f,.75f};
    edited.materialOverride.diffuse={.75f,.5f,.25f};
    edited.materialOverride.specular={.1f,.2f,.3f};
    edited.materialOverride.shininess=.4f;edited.materialOverride.transparency=.2f;
    edited.materialOverride.shading=2;edited.materialOverride.twoSided=true;
    PT_REQUIRE(w::Write(edited,bytes,error) && write(bytes));
    v::Visual round;PT_REQUIRE(v::Read(scratch,"ptpro_mesh",round,error));
    PT_REQUIRE(round.materials.size()==1 && round.materials[0].shading==2 &&
        round.materials[0].twoSided && round.materials[0].texture=="original.jpg" &&
        round.materials[0].shininess>.39f && round.materials[0].transparency>.19f);
    PT_REQUIRE(std::search(round.materials[0].raw.begin(),round.materials[0].raw.end(),
        unknown.begin(),unknown.end())!=round.materials[0].raw.end());
    std::error_code ec;std::filesystem::remove(scratch,ec);
    std::cout<<"Native visual smoothing masks, source material preservation, face mapping and opt-in overrides PASS\n";
}
