#include "AssetRegistry.h"
#include "LegacyMeshImport.h"
#include "NativeCollisionImport.h"
#include "Native3DSVisualMetadata.h"
#include "NativeObjectExport.h"
#include "NativeTaraWriter.h"
#include "ReleaseTestCheck.h"
#include <pugixml.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>

namespace fs=std::filesystem;
struct Scratch {
    fs::path path;
    ~Scratch(){std::error_code ec;fs::remove_all(path,ec);}
};
int main(int argc,char** argv) {
    PT_REQUIRE(argc==2);
    const fs::path fixtures=argv[1];
    const auto original=fixtures/"land01_original.3ds";
    const auto reference=fixtures/"land01_delta136_game_verified.3ds";
    std::string error;
    PT_REQUIRE(fs::is_regular_file(original)&&fs::is_regular_file(reference));
    Scratch scratch{fs::temp_directory_path()/
        ("ptpro_0528_native_export_"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()))};
    const auto root=scratch.path/"library";
    const auto sourceDir=root/"LandFixture";
    PT_REQUIRE(fs::create_directories(sourceDir));
    PT_REQUIRE(fs::copy_file(original,sourceDir/"land01.3ds"));
    const auto native=NativeCollisionImport::Read(sourceDir/"land01.3ds");
    PT_REQUIRE(native.Valid()&&native.triangles.size()==136&&native.boxes.empty()&&native.planes.empty());
    Native3DSVisualMetadata::Visual meta;
    PT_REQUIRE(Native3DSVisualMetadata::Read(sourceDir/"land01.3ds",native.visualAnchor,meta,error));
    std::set<std::string> textureNames;
    for(const auto& mat:meta.materials) {
        const auto filename=fs::path(mat.texture).filename().string();
        PT_REQUIRE(Native3DSWriter::SafeAscii(filename,120));
        textureNames.insert(filename);
    }
    PT_REQUIRE(!textureNames.empty());
    pugi::xml_document doc;
    auto lib=doc.append_child("library");lib.append_attribute("name")="LandFixture";
    auto group=lib.append_child("prop-group");group.append_attribute("name")="default";
    auto prop=group.append_child("prop");prop.append_attribute("name")="Land01";
    auto mesh=prop.append_child("mesh");mesh.append_attribute("file")="land01.3ds";
    for(const auto& filename:textureNames) {
        auto t=mesh.append_child("texture");t.append_attribute("name")=filename.c_str();
        t.append_attribute("diffuse-map")=filename.c_str();
        // Only a packaging fixture; the exporter's CPU path copies bytes and
        // does not decode the image. Real textures are tested in Windows UI.
        std::ofstream out(sourceDir/filename,std::ios::binary);
        out<<"texture sidecar fixture";
        PT_REQUIRE(static_cast<bool>(out));
    }
    PT_REQUIRE(doc.save_file((sourceDir/"library.xml").string().c_str()));
    AssetRegistry assets;PT_REQUIRE(assets.Scan(root,error));
    const auto* asset=assets.Find("LandFixture","default","Land01");
    PT_REQUIRE(asset&&asset->originalLibraryXml&&asset->textures.size()==textureNames.size());
    const auto originalMesh=LegacyMeshImport::Load(asset->mesh);
    const auto referenceMesh=LegacyMeshImport::Load(reference);
    PT_REQUIRE(originalMesh.vertices.size()==referenceMesh.vertices.size());
    PT_REQUIRE(originalMesh.indices.size()==referenceMesh.indices.size());
    // Identify the SINGLE elevated point from the user-validated reference by
    // its horizontal coordinates; preserve the original importer's vertex order.
    std::vector<size_t> changed;
    for(size_t i=0;i<originalMesh.vertices.size();++i){
        const auto& a=originalMesh.vertices[i].position;
        bool same=false;
        for(const auto& v:referenceMesh.vertices){
            const auto& b=v.position;
            if(std::abs(a.x-b.x)<.05f&&std::abs(a.y-b.y)<.05f&&std::abs(a.z-b.z)<.05f){same=true;break;}
        }
        if(!same)changed.push_back(i);
    }
    PT_REQUIRE(changed.size()==1);
    const auto& a=originalMesh.vertices[changed.front()].position;
    const DirectX::XMFLOAT3* elevated=nullptr;
    for(const auto& v:referenceMesh.vertices){
        const auto& b=v.position;
        if(std::abs(a.x-b.x)<.05f&&std::abs(a.z-b.z)<.05f&&std::abs(a.y-b.y)>.05f){
            PT_REQUIRE(elevated==nullptr);elevated=&b;
        }
    }
    PT_REQUIRE(elevated);
    ObjectDraft::Document draft;
    draft.name="Land01_peak_0528";
    draft.model=asset->mesh;
    draft.purpose=ObjectDraft::Purpose::DriveableDraft;
    draft.templateLibrary=asset->library;draft.templateGroup=asset->group;
    draft.templateName=asset->name;draft.templatePropXml=asset->originalPropXml;
    draft.libraryTemplateXml=asset->originalLibraryXml;
    draft.meshIndices=originalMesh.indices;
    for(const auto& v:originalMesh.vertices)draft.meshVertices.push_back({v.position.x,v.position.y,v.position.z});
    draft.meshVertices[changed.front()]={elevated->x,elevated->y,elevated->z};
    fs::path output;
    if(!NativeObjectExport::Export(draft,assets,output,error)){
        std::cerr<<"Land01 end-to-end native export failed: "<<error<<'\n';return 1;
    }
    PT_REQUIRE(fs::is_directory(output));
    PT_REQUIRE(fs::is_regular_file(root/"PTPRO_Land01_peak_0528.tara"));
    const auto again=NativeCollisionImport::Read(output/"ptpro_mesh.3ds");
    PT_REQUIRE(again.Valid()&&again.triangles.size()==138&&again.boxes.empty()&&again.planes.empty());
    const auto scanned=assets.Find("LandFixture","default","Land01");
    PT_REQUIRE(scanned&&NativeObjectExport::SameBytes(asset->mesh,original)); // untouched original
    auto invalid=draft;invalid.name="invalid_vertices";invalid.meshVertices.pop_back();
    PT_REQUIRE(!NativeObjectExport::Export(invalid,assets,output,error));
    PT_REQUIRE(!fs::exists(root/"PTPRO_invalid_vertices"));
    std::cout<<"PASS: original Land01 -> manual-reference peak -> native 3DS + TARA -> 138 verified helpers, no overwrite\n";
}
