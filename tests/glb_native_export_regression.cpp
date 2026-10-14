#include "GlbNativeExport.h"
#include "ReleaseTestCheck.h"
#include <chrono>
#include <iostream>
namespace fs=std::filesystem;
int main(int argc,char**argv) {
    PT_REQUIRE(argc==2);
    const fs::path fixtures=argv[1];
    const auto root=fs::temp_directory_path()/("ptpro_glb_test_"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    PT_REQUIRE(fs::create_directories(root));
    struct Clean {fs::path p;~Clean(){std::error_code ec;fs::remove_all(p,ec);}} clean{root};
    const auto model=DraftMeshImport::Load(fixtures/"textured_quad.glb");
    PT_REQUIRE(model.vertices.size()==4 && model.indices.size()==6 && model.parts.size()==1);
    PT_REQUIRE(model.parts[0].embeddedDiffuse && model.parts[0].embeddedDiffuse->size()>8);
    // Top-left UV origin must survive Assimp's glTF convention conversion.
    for(const auto& v:model.vertices) {
        PT_REQUIRE(std::abs(v.uv.x-v.position.x/500.f)<.0001f);
        PT_REQUIRE(std::abs(v.uv.y-v.position.y/500.f)<.0001f);
    }
    auto draft=ObjectDraft::FreshSource(fixtures/"textured_quad.glb","textured_quad");
    draft.boxes[0].min={0,-5,0};draft.boxes[0].max={500,5,500};
    fs::path saved;std::string error;
    if(!GlbNativeExport::Export(draft,root,saved,error)){std::cerr<<error<<'\n';return 1;}
    PT_REQUIRE(fs::is_regular_file(root/"PTPRO_textured_quad.tara"));
    PT_REQUIRE(!fs::exists(saved/"package.tara"));
    std::ifstream tex(saved/"basecolor.png",std::ios::binary);
    const std::vector<std::uint8_t> encoded((std::istreambuf_iterator<char>(tex)),{});
    PT_REQUIRE(encoded==*model.parts[0].embeddedDiffuse);
    auto back=LegacyMeshImport::Load(saved/"ptpro_mesh.3ds");
    PT_REQUIRE(back.indices.size()==model.indices.size());
    for(const auto& v:back.vertices) {
        PT_REQUIRE(std::abs(v.uv.x-v.position.x/500.f)<.0001f);
        PT_REQUIRE(std::abs(v.uv.y-v.position.y/500.f)<.0001f);
    }
    const auto native=NativeCollisionImport::Read(saved/"ptpro_mesh.3ds");
    PT_REQUIRE(native.Valid()&&native.boxes.size()==1&&native.triangles.empty()&&native.planes.empty());
    PT_REQUIRE(std::abs(native.boxes[0].size.x-500)<.001f);
    PT_REQUIRE(std::abs(native.boxes[0].size.y-10)<.001f);
    PT_REQUIRE(std::abs(native.boxes[0].size.z-500)<.001f);
    const auto originalOutput=saved;
    PT_REQUIRE(!GlbNativeExport::Export(draft,root,saved,error)); // no overwrite
    PT_REQUIRE(fs::is_regular_file(originalOutput/"basecolor.png"));
    for(const auto name:{"alpha_quad","tinted_quad"}) {
        auto unsupported=ObjectDraft::FreshSource(fixtures/(std::string(name)+".glb"),name);
        PT_REQUIRE(!GlbNativeExport::Export(unsupported,root,saved,error));
        PT_REQUIRE(!fs::exists(root/("PTPRO_"+std::string(name))));
    }
    auto trigger=draft;trigger.name="trigger";trigger.purpose=ObjectDraft::Purpose::TriggerDraft;
    PT_REQUIRE(!GlbNativeExport::Export(trigger,root,saved,error));
    auto edited=draft;edited.name="edited";
    for(const auto& v:model.vertices)edited.meshVertices.push_back({v.position.x,v.position.y+20,v.position.z});
    edited.meshIndices=model.indices;
    PT_REQUIRE(GlbNativeExport::Export(edited,root,saved,error));
    back=LegacyMeshImport::Load(saved/"ptpro_mesh.3ds");
    for(const auto& v:back.vertices)PT_REQUIRE(std::abs(v.uv.y-(v.position.y-20)/500.f)<.0001f);
    edited.name="missing_uv";edited.meshVertices.push_back({1,2,3});
    PT_REQUIRE(!GlbNativeExport::Export(edited,root,saved,error));
    for(const auto& f:fs::directory_iterator(root))PT_REQUIRE(f.path().filename().string().find(".tmp")==std::string::npos);
    std::cout<<"PASS: embedded texture, UV orientation, native geometry/boxes, TARA, edits and rejection paths\n";
}
