#pragma once
// Explicit, isolated native 3DS library export for edited legacy 3DS drafts.
// No GLB/trigger export is implied: those formats need separate game proofs.
// The original source, existing library folders and map XML are NEVER replaced.
#include "AssetRegistry.h"
#include "LegacyMeshImport.h"
#include "NativeCollisionImport.h"
#include "Native3DSWriter.h"
#include "Native3DSVisualMetadata.h"
#include "NativeTerrainDelta.h"
#include "NativeExportCollisionPolicy.h"
#include "NativeTaraWriter.h"
#include "ObjectDraft.h"
#include <pugixml.hpp>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace NativeObjectExport {
namespace fs=std::filesystem;
inline bool SameBytes(const fs::path& a,const fs::path& b){
    std::error_code ec;
    if(fs::file_size(a,ec)!=fs::file_size(b,ec)||ec)return false;
    std::ifstream one(a,std::ios::binary),two(b,std::ios::binary);
    if(!one||!two)return false;
    return std::equal(std::istreambuf_iterator<char>(one),std::istreambuf_iterator<char>(),
                      std::istreambuf_iterator<char>(two),std::istreambuf_iterator<char>());
}
inline std::string Lower(std::string x){return LegacyMeshImport::Lower(std::move(x));}
inline bool Export(const ObjectDraft::Document& d,const AssetRegistry& assets,
                   fs::path& saved,std::string& error){
    saved.clear();
    if(!ObjectDraft::Validate(d,error))return false;
    if(ObjectDraft::IsGlb(d.model)){
        error="GLB-to-native 3DS/textured material conversion has not been verified. Use an original 3DS template.";return false;
    }
    if(!d.libraryTemplateXml||d.templateLibrary.empty()||!d.excludedTemplateFields.empty()){
        error="Select a complete original 3DS library template without excluding fields.";return false;
    }
    if(d.purpose==ObjectDraft::Purpose::TriggerDraft){
        error="Trigger draft has no verified native 3DS gameplay mapping; export refused.";return false;
    }
    for(const auto& box:d.boxes)if(box.role!=ObjectDraft::BoxRole::Solid){
        error="Trigger collision boxes are not native solid Box helpers; export refused.";return false;
    }
    const auto* source=assets.Find(d.templateLibrary,d.templateGroup,d.templateName);
    if(!source||source->mesh.empty()||!fs::is_regular_file(source->mesh)||!SameBytes(d.model,source->mesh)){
        error="Original 3DS template is missing or source bytes differ. Re-select the original Library object.";return false;
    }
    if(source->textures.empty()) {error="The original prop has no verified named texture variants.";return false;}
    pugi::xml_document templateDoc;
    const auto parse=templateDoc.load_buffer(d.templatePropXml.data(),d.templatePropXml.size());
    const auto prop=templateDoc.child("prop"),meshNode=prop.child("mesh");
    if(!parse||!prop||!meshNode||!prop.attribute("name")||!meshNode.attribute("file")||
       std::string(prop.attribute("name").value())!=d.templateName||
       Lower(fs::path(meshNode.attribute("file").value()).filename().string())!=Lower(source->mesh.filename().string())){
        error="The selected library prop and 3DS source do not agree.";return false;
    }
    // The source might carry unrecognized gameplay fields. Never copy them to
    // a new object by accident: the editable draft only specifies mesh/boxes.
    for(auto a:prop.attributes())if(std::string(a.name())!="name"){
        error="The template contains extra gameplay attributes; native conversion is not verified.";return false;
    }
    for(auto n:prop.children())if(n!=meshNode){
        error="The template contains extra gameplay nodes; native conversion is not verified.";return false;
    }
    for(auto a:meshNode.attributes())if(std::string(a.name())!="file"){
        error="The source mesh has extra attributes requiring separate conversion.";return false;
    }
    size_t listedTextures=0;for(auto ignored:meshNode.children("texture")){(void)ignored;++listedTextures;}
    if(source->textures.size()!=listedTextures){
        error="The selected template texture list differs from the current Library.";return false;
    }
    for(auto n:meshNode.children())if(std::string(n.name())!="texture"){
        error="The source mesh has unsupported non-texture metadata.";return false;
    }
    std::error_code ec;
    const auto root=assets.Root();
    if(!fs::is_directory(root,ec)||ec){error="Select a writable native Library directory.";return false;}
    const std::string slug=ObjectDraft::Slug(d.name);
    const std::string library="PTPRO_"+slug;
    if(!Native3DSWriter::SafeAscii(library,60)){
        error="Choose a shorter ASCII name for a native 3DS library.";return false;
    }
    const auto final=root/library;
    if(fs::exists(final,ec)||ec||fs::exists(root/(library+".tara"),ec)){error="Custom library or TARA already exists. Rename the draft; nothing was overwritten.";return false;}
    LegacyMeshImport::Model imported;
    try{imported=LegacyMeshImport::Load(d.model);}
    catch(const std::exception& ex){error=std::string("Source 3DS import failed: ")+ex.what();return false;}
    if(imported.vertices.empty()||imported.indices.empty()||imported.parts.empty()||
       imported.vertices.size()>65535||imported.indices.size()/3>65535){
        error="Source model exceeds the 3DS 16-bit limits or has no visible triangles.";return false;
    }
    // Read the SOURCE 3DS metadata, not Assimp's generated preview normals:
    // preserve per-face smoothing masks and complete original material chunks.
    const auto sourceCollision=NativeCollisionImport::Read(source->mesh);
    if(sourceCollision.visualAnchor.empty()){
        error="Original 3DS visual anchor could not be resolved.";return false;
    }
    Native3DSVisualMetadata::Visual sourceVisual;
    if(!Native3DSVisualMetadata::Read(source->mesh,sourceCollision.visualAnchor,sourceVisual,error))return false;
    Native3DSWriter::Model sourceModel;
    sourceModel.vertices.reserve(imported.vertices.size());
    for(const auto& v:imported.vertices)
        sourceModel.vertices.push_back({v.position.x,v.position.y,v.position.z,v.uv.x,v.uv.y});
    sourceModel.indices=imported.indices;
    std::vector<std::uint32_t> sourceSmooth;
    std::vector<std::string> sourceFaceMaterial;
    if(!Native3DSVisualMetadata::Match(sourceVisual,sourceModel,sourceSmooth,sourceFaceMaterial,error))return false;
    Native3DSWriter::Model output;
    output.visualName="ptpro_mesh"; // 3DS visual anchor; helper nodes have Box prefix.
    output.vertices.reserve(d.meshVertices.empty()?imported.vertices.size():d.meshVertices.size());
    if(!d.meshVertices.empty()&&d.meshVertices.size()!=imported.vertices.size()){
        error="Edited vertex count differs from original UV layout; export refused to avoid incorrect texturing.";return false;
    }
    const auto& sourceIndices=imported.indices;
    output.indices=d.meshIndices.empty()?sourceIndices:d.meshIndices;
    if(output.indices!=sourceIndices&&imported.parts.size()>1){
        error="Face edits on a multi-material mesh need material reassignment; export refused.";return false;
    }
    for(size_t i=0;i<imported.vertices.size();++i){
        const auto& v=imported.vertices[i];
        const auto p=d.meshVertices.empty()?std::array<float,3>{{v.position.x*d.scale,v.position.y*d.scale,v.position.z*d.scale}}:d.meshVertices[i];
        output.vertices.push_back({p[0],p[1],p[2],v.uv.x,v.uv.y});
    }
    if(output.indices!=sourceIndices&&imported.parts.size()==1){
        output.parts={{0,output.indices.size(),"",""}};
        if(!std::all_of(sourceFaceMaterial.begin(),sourceFaceMaterial.end(),[&](const auto& n){return n==sourceFaceMaterial.front();})){
            error="Edited triangles span different source materials; export refused.";return false;
        }
        if(!std::all_of(sourceSmooth.begin(),sourceSmooth.end(),[&](auto n){return n==sourceSmooth.front();})){
            error="New face smoothing is ambiguous across original groups; export refused.";return false;
        }
        output.smoothingGroups.assign(output.indices.size()/3,sourceSmooth.front());
    } else {
        for(size_t i=0;i<imported.parts.size();++i){
            const auto& part=imported.parts[i];
            output.parts.push_back({part.firstIndex,part.indexCount,"",""});
        }
        output.smoothingGroups=sourceSmooth;
    }
    for(size_t i=0;i<output.parts.size();++i){
        auto& part=output.parts[i];
        const size_t sourcePart=i<imported.parts.size()?i:0;
        const size_t begin=imported.parts[sourcePart].firstIndex/3;
        const size_t count=imported.parts[sourcePart].indexCount/3;
        if(begin>=sourceFaceMaterial.size()||count>sourceFaceMaterial.size()-begin){
            error="Invalid original material/face range.";return false;
        }
        const auto& name=sourceFaceMaterial[begin];
        if(!std::all_of(sourceFaceMaterial.begin()+static_cast<std::ptrdiff_t>(begin),
            sourceFaceMaterial.begin()+static_cast<std::ptrdiff_t>(begin+count),
            [&](const auto& n){return n==name;})){
            error="Assimp material split does not agree with source 3DS material faces.";return false;
        }
        const auto mat=std::find_if(sourceVisual.materials.begin(),sourceVisual.materials.end(),
            [&](const auto& m){return m.name==name;});
        if(mat==sourceVisual.materials.end()||!Native3DSWriter::SafeAscii(name,50)){
            error="Cannot preserve a source 3DS material safely.";return false;
        }
        part.material=name;
        output.nativeMaterials.push_back(mat->raw);
    }
    output.materialOverride=d.materialOverride;
    if(d.smoothingMode==1)output.smoothingGroups.assign(output.indices.size()/3,1u);
    if(d.smoothingMode==2)output.smoothingGroups.assign(output.indices.size()/3,0u);
    if(d.smoothingMode==3)output.smoothingGroups.assign(output.indices.size()/3,1u<<static_cast<unsigned>(d.smoothingGroup-1));
    // A verified, original triangle-only surface is NOT a box. For this
    // constrained heightfield case use original authored helpers, conform them
    // to the edited visual surface and split the peak-containing source face.
    // Unsupported multi-vertex edits fail closed instead of releasing a wall.
    const auto originalCollision=NativeCollisionImport::Read(source->mesh);
    // A malformed/unsupported original collision helper must never be silently
    // replaced by plausible-looking authored boxes. Only a genuinely helperless
    // source can start with explicitly authored draft boxes.
    if(!originalCollision.Valid() &&
       originalCollision.error!=NativeCollisionImport::NoNativeHelpersError){
        error="Original 3DS collision helper is unsupported: "+originalCollision.error;
        return false;
    }
    const bool terrain=originalCollision.Valid()&&!originalCollision.triangles.empty() &&
                       originalCollision.planes.empty()&&originalCollision.boxes.empty();
    // Do not silently replace authored planes or mixed primitives with generic
    // boxes. Existing box-only assets are an explicit reauthoring path, NOT an
    // assertion that arbitrary rotated original boxes have been preserved.
    if(!NativeExportCollisionPolicy::Validate(
        d.purpose==ObjectDraft::Purpose::Decorative,originalCollision.Valid(),
        originalCollision.planes.size(),originalCollision.boxes.size(),
        originalCollision.triangles.size(),
        originalCollision.error==NativeCollisionImport::NoNativeHelpersError,
        d.boxes.size(),error))return false;
    if(d.purpose!=ObjectDraft::Purpose::Decorative){
        if(terrain){
            if(!NativeTerrainDelta::Build(source->mesh,output,output.triangles,error))return false;
        }else{
            for(const auto& b:d.boxes)output.boxes.push_back({b.min,b.max});
        }
    }
    // Resolve every native material's diffuse from the original library, not
    // from draft/source.3ds where the source texture sidecars are absent.
    for(size_t i=0;i<output.parts.size();++i){
        const auto& part=imported.parts[std::min(i,imported.parts.size()-1)];
        const auto wanted=Lower(part.diffuse.filename().string());
        auto texture=std::find_if(source->textures.begin(),source->textures.end(),[&](const TextureVariant& t){
            return Lower(t.diffuse.filename().string())==wanted;
        });
        if(texture==source->textures.end()){
            if(source->textures.size()==1)texture=source->textures.begin();
            else {error="Cannot match a 3DS material to its original texture variant.";return false;}
        }
        output.parts[i].texture=texture->diffuse.filename().string();
        // The source's FULL material chunk is retained, including the native
        // map filename. Do not claim preservation if it references another texture.
        const auto originalMaterial=std::find_if(sourceVisual.materials.begin(),sourceVisual.materials.end(),
            [&](const auto& m){return m.name==output.parts[i].material;});
        if(originalMaterial==sourceVisual.materials.end() ||
           Lower(fs::path(originalMaterial->texture).filename().string())!=Lower(output.parts[i].texture)){
            error="Source material texture differs from library variant; raw material preservation is unsafe.";return false;
        }
        if(!Native3DSWriter::SafeAscii(output.parts[i].texture,120)){
            error="3DS source texture filename is unsupported.";return false;
        }
    }
    Native3DSWriter::Bytes data;
    if(!Native3DSWriter::Write(output,data,error))return false;
    fs::path temp;
    for(unsigned i=0;i<1024;++i){
        temp=root/(".ptpro_native_"+slug+"_"+std::to_string(i)+".tmp");
        ec.clear();if(fs::create_directory(temp,ec))break;
        if(ec||i==1023){error="Could not create native library staging folder.";return false;}
    }
    const auto cleanup=[&]{std::error_code ignored;fs::remove_all(temp,ignored);};
    const auto modelFile=temp/"ptpro_mesh.3ds";
    {std::ofstream out(modelFile,std::ios::binary|std::ios::trunc);
     out.write(reinterpret_cast<const char*>(data.data()),static_cast<std::streamsize>(data.size()));
     out.flush();if(!out){cleanup();error="Could not write native 3DS.";return false;}}
    // Copy all template variants; a new library must be self-contained.
    for(const auto& variant:source->textures){
        const auto name=variant.diffuse.filename().string();
        if(!Native3DSWriter::SafeAscii(name,120)||!fs::is_regular_file(variant.diffuse)){
            cleanup();error="A required original texture is missing/unsafe: "+name;return false;
        }
        if(!fs::exists(temp/name)){
            ec.clear();fs::copy_file(variant.diffuse,temp/name,fs::copy_options::none,ec);
            if(ec){cleanup();error="Cannot copy source texture: "+ec.message();return false;}
        }else if(!SameBytes(variant.diffuse,temp/name)){
            cleanup();error="Texture variants have a conflicting destination filename.";return false;
        }
    }
    // Also copy a material texture that is not exposed as a named variant.
    for(const auto& p:output.parts)if(!fs::is_regular_file(temp/p.texture)){
        cleanup();error="Native material texture was not packaged: "+p.texture;return false;
    }
    pugi::xml_document xml;
    xml.append_child(pugi::node_declaration).append_attribute("version").set_value("1.0");
    auto lib=xml.append_child("library");lib.append_attribute("name").set_value(library.c_str());
    auto group=lib.append_child("prop-group");group.append_attribute("name").set_value("default");
    auto item=group.append_child("prop");item.append_attribute("name").set_value(d.name.c_str());
    auto model=item.append_child("mesh");model.append_attribute("file").set_value("ptpro_mesh.3ds");
    for(const auto& variant:source->textures){
        auto t=model.append_child("texture");t.append_attribute("name").set_value(variant.name.c_str());
        t.append_attribute("diffuse-map").set_value(variant.diffuse.filename().string().c_str());
    }
    const auto libraryXmlPath=temp/"library.xml";
    if(!xml.save_file(libraryXmlPath.c_str(),"    ",pugi::format_default,pugi::encoding_utf8)){
        cleanup();error="Could not write new native library.xml.";return false;
    }
    // Validate with the SAME native reader used when placing an object; a
    // malformed box is never released just because its file was written.
    const auto native=NativeCollisionImport::Read(modelFile);
    if(!output.triangles.empty()){
        if(!native.Valid()||native.triangles.size()!=output.triangles.size()||
           !native.boxes.empty()||!native.planes.empty()){
            cleanup();error="Native terrain helper round-trip failed: "+native.error;return false;
        }
        // Round-trip the actual authored helper coordinates. The native reader
        // computes rotations/local offsets, so inspect source 3DS helper nodes
        // rather than comparing only primitive counts or floating Euler angles.
        std::string triangleParseError;
        const auto writtenNodes=NativeCollisionImport::ReadNodes(modelFile,triangleParseError);
        if(!triangleParseError.empty()){
            cleanup();error="Cannot reread exported terrain: "+triangleParseError;return false;
        }
        size_t triangleIndex=0;
        for(const auto& node:writtenNodes)if(NativeCollisionImport::Prefix(node.name,"tri")){
            if(triangleIndex>=output.triangles.size()||node.vertices.size()!=3||node.faces.size()!=1){
                cleanup();error="Exported terrain helper topology changed.";return false;
            }
            const auto& face=node.faces.front();
            for(size_t corner=0;corner<3;++corner){
                if(face[corner]>=node.vertices.size()){
                    cleanup();error="Exported terrain helper has invalid indices.";return false;
                }
                const auto& got=node.vertices[face[corner]];
                const auto& want=output.triangles[triangleIndex][corner];
                if(std::abs(got.x-want[0])>.05f||std::abs(got.y-want[1])>.05f||
                   std::abs(got.z-want[2])>.05f){
                    cleanup();error="Exported terrain helper vertex changed during 3DS round-trip.";return false;
                }
            }
            ++triangleIndex;
        }
        if(triangleIndex!=output.triangles.size()){
            cleanup();error="Exported terrain helper count differs from the authored geometry.";return false;
        }
    }
    if(!output.boxes.empty()){
        if(!native.Valid()||native.boxes.size()!=output.boxes.size()||!native.planes.empty()||!native.triangles.empty()){
            cleanup();error="Native collision helper round-trip failed: "+native.error;return false;
        }
        for(size_t i=0;i<output.boxes.size();++i){
            const auto& b=output.boxes[i];const auto& c=native.boxes[i];
            const auto close=[](float a,float b){return std::abs(a-b)<.02f;};
            if(!close(c.offset.x,(b.min[0]+b.max[0])*.5f)||
               !close(c.offset.y,(b.min[1]+b.max[1])*.5f)||
               !close(c.offset.z,(b.min[2]+b.max[2])*.5f)||
               !close(c.size.x,b.max[0]-b.min[0])||
               !close(c.size.y,b.max[1]-b.min[1])||
               !close(c.size.z,b.max[2]-b.min[2])){
                cleanup();error="Native box dimensions/placement changed during round-trip.";return false;
            }
        }
    }
    Native3DSVisualMetadata::Visual roundTrip;
    if(!Native3DSVisualMetadata::Read(modelFile,output.visualName,roundTrip,error)||
       roundTrip.faces.size()!=output.smoothingGroups.size()){
        cleanup();error="Native 3DS smoothing/material readback failed: "+error;return false;
    }
    for(size_t i=0;i<roundTrip.faces.size();++i)if(roundTrip.faces[i].smoothing!=output.smoothingGroups[i]){
        cleanup();error="Native 3DS smoothing group changed during round-trip.";return false;
    }
    for(size_t i=0;i<output.parts.size();++i){
        const auto material=std::find_if(roundTrip.materials.begin(),roundTrip.materials.end(),
            [&](const auto& m){return m.name==output.parts[i].material;});
        if(material==roundTrip.materials.end()||
           Lower(fs::path(material->texture).filename().string())!=Lower(output.parts[i].texture)||
           (!output.materialOverride.enabled && material->raw!=output.nativeMaterials[i])){
            cleanup();error="Source 3DS material or texture did not survive native round-trip.";return false;
        }
    }
    try{
        const auto back=LegacyMeshImport::Load(modelFile);
        if(back.vertices.size()!=output.vertices.size()||back.indices.size()!=output.indices.size()){
            cleanup();error="Native visual mesh round-trip changed vertex/face counts.";return false;
        }
    }catch(const std::exception& ex){
        cleanup();error=std::string("Native visual 3DS reimport failed: ")+ex.what();return false;
    }
    // Publish the FOLDER and the game-consumable TARA together; if either
    // staging/rename fails no partially working new library is left behind.
    const auto taraFinal=root/(library+".tara");
    const auto taraStaging=root/(temp.filename().string()+".tara");
    if(fs::exists(taraStaging,ec)||ec){cleanup();error="TARA staging destination exists.";return false;}
    if(!NativeTaraWriter::PackDirectory(temp,taraStaging,error)){cleanup();return false;}
    ec.clear();fs::rename(temp,final,ec);
    if(ec){fs::remove(taraStaging);cleanup();error="Cannot finalize new library: "+ec.message();return false;}
    ec.clear();fs::rename(taraStaging,taraFinal,ec);
    if(ec){std::error_code ignored;fs::remove_all(final,ignored);fs::remove(taraStaging,ignored);
        error="Cannot finalize TARA alongside native Library folder: "+ec.message();return false;}
    saved=final;error.clear();return true;
}
} // namespace NativeObjectExport
