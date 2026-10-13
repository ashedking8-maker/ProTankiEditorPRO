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
#include <sstream>

namespace fs=std::filesystem;
struct Scratch {
    fs::path path;
    ~Scratch(){std::error_code ec;fs::remove_all(path,ec);}
};
// 3DS files may retain unused materials (Fogtown has an untextured Default).
// Build sidecars only for materials referenced by this visual's faces. A used
// material without a safe texture remains a test failure, not a silent skip.
static int VisualTextureNames(const Native3DSVisualMetadata::Visual& visual, std::set<std::string>& names) {
    names.clear();
    for(const auto& face:visual.faces) {
        const auto material=std::find_if(visual.materials.begin(),visual.materials.end(),
            [&](const auto& candidate){return candidate.name==face.material;});
        PT_REQUIRE(material!=visual.materials.end());
        const auto filename=fs::path(material->texture).filename().string();
        if(!Native3DSWriter::SafeAscii(filename,120))
            std::cerr<<"Invalid fixture texture for used material: "<<face.material<<'\n';
        PT_REQUIRE(Native3DSWriter::SafeAscii(filename,120));
        names.insert(filename);
    }
    PT_REQUIRE(!names.empty());
    return 0;
}
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
    PT_REQUIRE(VisualTextureNames(meta,textureNames)==0);
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
    // Compare imported LOCAL coordinates, not raw 3DS world coordinates: the
    // original Land01 has a nonzero 3DS pivot while the exported reference is
    // pivot-relative. The verified peak changes X/Z as well as its height, so
    // matching only horizontal coordinates wrongly rejects the real fixture.
    // Preserve the original importer's vertex order for the draft/UV layout.
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
    std::vector<size_t> referenceChanges;
    for(size_t i=0;i<referenceMesh.vertices.size();++i){
        const auto& b=referenceMesh.vertices[i].position;
        bool same=false;
        for(const auto& v:originalMesh.vertices){
            const auto& p=v.position;
            if(std::abs(p.x-b.x)<.05f&&std::abs(p.y-b.y)<.05f&&std::abs(p.z-b.z)<.05f){same=true;break;}
        }
        if(!same)referenceChanges.push_back(i);
    }
    PT_REQUIRE(referenceChanges.size()==1);
    const auto& elevated=referenceMesh.vertices[referenceChanges.front()].position;
    PT_REQUIRE(elevated.y>a.y+.05f);
    ObjectDraft::Document draft;
    draft.name="Land01_peak_0528";
    draft.model=asset->mesh;
    draft.purpose=ObjectDraft::Purpose::DriveableDraft;
    draft.templateLibrary=asset->library;draft.templateGroup=asset->group;
    draft.templateName=asset->name;draft.templatePropXml=asset->originalPropXml;
    draft.libraryTemplateXml=asset->originalLibraryXml;
    draft.meshIndices=originalMesh.indices;
    for(const auto& v:originalMesh.vertices)draft.meshVertices.push_back({v.position.x,v.position.y,v.position.z});
    draft.meshVertices[changed.front()]={elevated.x,elevated.y,elevated.z};
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
    // Map-certified helperless Fogtown surface must not become the default draft box.
    const auto fogDir=root/"FogtownFixture";PT_REQUIRE(fs::create_directories(fogDir));
    PT_REQUIRE(fs::copy_file(fixtures.parent_path()/"fogtown_surface/land01.3ds",fogDir/"land01.3ds"));
    Native3DSVisualMetadata::Visual fogMeta;
    PT_REQUIRE(Native3DSVisualMetadata::Read(fogDir/"land01.3ds","Land",fogMeta,error));
    pugi::xml_document fogXml;auto fogLib=fogXml.append_child("library");fogLib.append_attribute("name")="FogtownFixture";
    auto fogGroup=fogLib.append_child("prop-group");fogGroup.append_attribute("name")="l";
    auto fogProp=fogGroup.append_child("prop");fogProp.append_attribute("name")="land 1";
    auto fogMesh=fogProp.append_child("mesh");fogMesh.append_attribute("file")="land01.3ds";
    std::set<std::string> fogTextures;
    PT_REQUIRE(VisualTextureNames(fogMeta,fogTextures)==0);
    for(const auto& name:fogTextures) {
        std::ofstream tex(fogDir/name,std::ios::binary);tex<<"texture fixture";PT_REQUIRE(bool(tex));
        auto t=fogMesh.append_child("texture");t.append_attribute("name")=name.c_str();t.append_attribute("diffuse-map")=name.c_str();
    }
    PT_REQUIRE(fogXml.save_file((fogDir/"library.xml").string().c_str()));
    PT_REQUIRE(assets.Scan(root,error));const auto* fogAsset=assets.Find("FogtownFixture","l","land 1");PT_REQUIRE(fogAsset);
    auto fogDraft=ObjectDraft::FreshSource(fogAsset->mesh,"Fogtown_preserved");
    fogDraft.templateLibrary=fogAsset->library;fogDraft.templateGroup=fogAsset->group;fogDraft.templateName=fogAsset->name;
    fogDraft.templatePropXml=fogAsset->originalPropXml;fogDraft.libraryTemplateXml=fogAsset->originalLibraryXml;
    if(!NativeObjectExport::Export(fogDraft,assets,output,error)) {
        std::cerr<<"Fogtown end-to-end native export failed: "<<error<<'\n';return 1;
    }
    const auto fogBack=NativeCollisionImport::Read(output/"ptpro_mesh.3ds");
    PT_REQUIRE(fogBack.Valid()&&fogBack.triangles.size()==18&&fogBack.boxes.empty());
    auto changedFog=fogDraft;changedFog.name="Fogtown_changed";changedFog.scale=2;
    PT_REQUIRE(!NativeObjectExport::Export(changedFog,assets,output,error));
    PT_REQUIRE(!fs::exists(root/"PTPRO_Fogtown_changed"));
    auto unknownTexture=fogDraft;unknownTexture.name="Fogtown_extra_texture_metadata";
    pugi::xml_document extra;PT_REQUIRE(extra.load_string(unknownTexture.templatePropXml.c_str()));
    extra.child("prop").child("mesh").child("texture").append_attribute("alpha-map")="alpha.png";
    std::ostringstream serialized;extra.save(serialized);unknownTexture.templatePropXml=serialized.str();
    PT_REQUIRE(!NativeObjectExport::Export(unknownTexture,assets,output,error));
    PT_REQUIRE(!fs::exists(root/"PTPRO_Fogtown_extra_texture_metadata"));
    std::cout<<"PASS: original Land01 -> manual-reference peak -> native 3DS + TARA -> 138 verified helpers, no overwrite\n";
}
