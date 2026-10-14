#pragma once
// Static GLB -> isolated legacy diffuse-textured prop. PBR is deliberately not
// claimed: the UI names this conversion and explains its legacy limitations.
#include "DraftMeshImport.h"
#include "ObjectDraft.h"
#include "NativeCollisionImport.h"
#include "Native3DSVisualMetadata.h"
#include "NativeTaraWriter.h"
#include <pugixml.hpp>

namespace GlbNativeExport {
namespace fs=std::filesystem;
inline bool Export(const ObjectDraft::Document& d,const fs::path& root,fs::path& saved,std::string& error) {
    saved.clear();error.clear();
    if(!ObjectDraft::Validate(d,error))return false;
    if(d.purpose==ObjectDraft::Purpose::TriggerDraft) {error="GLB trigger export is unsupported.";return false;}
    if(d.materialOverride.enabled) {error="Native 3DS material overrides cannot be applied to GLB.";return false;}
    if(d.purpose!=ObjectDraft::Purpose::Decorative && d.boxes.empty()) {
        error="A solid GLB requires authored collision boxes. Use Fit selected box to mesh bounds.";return false;
    }
    for(const auto& box:d.boxes)if(box.role!=ObjectDraft::BoxRole::Solid) {
        error="GLB export supports solid boxes only, not trigger regions.";return false;
    }
    LegacyMeshImport::Model imported;
    try {imported=DraftMeshImport::Load(d.model);}
    catch(const std::exception& ex) {error=ex.what();return false;}
    if(!imported.nativeExportProblems.empty()) {error=imported.nativeExportProblems.front();return false;}
    if(imported.parts.empty() || !imported.parts.front().embeddedDiffuse) {
        error="Legacy GLB export requires an embedded base-color PNG/JPEG.";return false;
    }
    const auto image=imported.parts.front().embeddedDiffuse;
    // GLB OPAQUE ignores image alpha; the old game does not. Do not release an
    // unexpectedly transparent prop. RGB/grayscale PNGs and JPEGs are exact.
    if((*image)[0]==137) {
        const auto& b=*image;
        const auto be32=[&](size_t p){return (std::uint32_t(b[p])<<24)|(std::uint32_t(b[p+1])<<16)|
            (std::uint32_t(b[p+2])<<8)|b[p+3];};
        if(b.size()<33 || be32(8)!=13 || (b[25]!=0 && b[25]!=2 && b[25]!=3)) {
            error="Legacy export requires an opaque RGB PNG or JPEG; remove the PNG alpha channel first.";return false;
        }
        for(size_t p=8;p<b.size();) {
            if(b.size()-p<12 || be32(p)>b.size()-p-12) {error="Malformed PNG texture.";return false;}
            if(b[p+4]=='t'&&b[p+5]=='R'&&b[p+6]=='N'&&b[p+7]=='S') {
                error="PNG transparency cannot preserve GLB OPAQUE in the legacy game. Use RGB PNG/JPEG.";return false;
            }
            p+=12+be32(p);
        }
    }
    // Original editor variants replace the whole mesh texture. Multiple image
    // materials therefore need an atlas; never mislabel them as variants.
    for(const auto& part:imported.parts) {
        if(part.doubleSided!=imported.parts.front().doubleSided) {
            error="Legacy single-atlas export requires consistent double-sided material settings.";return false;
        }
        if(!part.embeddedDiffuse || *part.embeddedDiffuse!=*image) {
            error="GLB uses multiple base-color images. Bake one texture atlas before legacy export.";return false;
        }
        const auto& c=part.color;
        if(!part.opaque || c.r!=1 || c.g!=1 || c.b!=1 || c.a!=1) {
            error="Legacy export requires opaque, white-factor materials. Bake color/alpha into an opaque base-color image first.";return false;
        }
    }
    Native3DSWriter::Model model;model.visualName="ptpro_mesh";
    if(!d.meshVertices.empty() && d.meshVertices.size()!=imported.vertices.size()) {
        error="Added/removed GLB vertices have no authored UVs. Keep the original vertex count.";return false;
    }
    for(size_t i=0;i<imported.vertices.size();++i) {
        const auto& v=imported.vertices[i];
        const auto p=d.meshVertices.empty()?std::array<float,3>{v.position.x*d.scale,v.position.y*d.scale,v.position.z*d.scale}:d.meshVertices[i];
        model.vertices.push_back({p[0],p[1],p[2],v.uv.x,v.uv.y});
    }
    model.indices=d.meshIndices.empty()?imported.indices:d.meshIndices;
    const std::string texture=(*image)[0]==137?"basecolor.png":"basecolor.jpg";
    model.parts.push_back({0,model.indices.size(),"basecolor",texture});
    Native3DSWriter::MaterialOverride shading;shading.twoSided=imported.parts.front().doubleSided;
    Native3DSWriter::Bytes material;
    if(!Native3DSWriter::OverrideMaterial(Native3DSWriter::Material(model.parts.front()),shading,material)) {
        error="Cannot create legacy GLB material.";return false;
    }
    model.nativeMaterials.push_back(std::move(material));
    const std::uint32_t smooth=d.smoothingMode==2?0u:d.smoothingMode==3?(1u<<(d.smoothingGroup-1)):1u;
    model.smoothingGroups.assign(model.indices.size()/3,smooth);
    if(d.purpose!=ObjectDraft::Purpose::Decorative)
        for(const auto& box:d.boxes)model.boxes.push_back({box.min,box.max});
    Native3DSWriter::Bytes bytes;
    if(!Native3DSWriter::Write(model,bytes,error))return false;
    std::error_code ec;
    const auto library="PTPRO_"+ObjectDraft::Slug(d.name);
    if(!Native3DSWriter::SafeAscii(library,60)) {error="Use a shorter ASCII object name.";return false;}
    if(!fs::is_directory(root,ec)||ec) {error="Select a writable Library directory.";return false;}
    const auto final=root/library,taraFinal=root/(library+".tara");
    if(fs::exists(final,ec)||ec||fs::exists(taraFinal,ec)||ec) {error="Output already exists; rename the object.";return false;}
    fs::path temp;
    for(unsigned i=0;i<1024;++i) {
        temp=root/(".ptpro_glb_"+library+"_"+std::to_string(i)+".tmp");
        ec.clear();if(fs::create_directory(temp,ec))break;
        if(ec||i==1023) {error="Cannot create GLB staging folder.";return false;}
    }
    struct Cleanup {fs::path p;~Cleanup(){std::error_code ignored;fs::remove_all(p,ignored);}} cleanup{temp};
    const auto write=[&](const fs::path& path,const auto& data) {
        std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(data.data()),static_cast<std::streamsize>(data.size()));
        out.flush();return bool(out);
    };
    if(!write(temp/"ptpro_mesh.3ds",bytes)||!write(temp/texture,*image)) {error="Cannot write GLB library files.";return false;}
    pugi::xml_document xml;
    auto lib=xml.append_child("library");lib.append_attribute("name")=library.c_str();
    auto group=lib.append_child("prop-group");group.append_attribute("name")="default";
    auto prop=group.append_child("prop");prop.append_attribute("name")=d.name.c_str();
    auto mesh=prop.append_child("mesh");mesh.append_attribute("file")="ptpro_mesh.3ds";mesh.append_attribute("object")="ptpro_mesh";
    auto variant=mesh.append_child("texture");variant.append_attribute("name")="default";variant.append_attribute("diffuse-map")=texture.c_str();
    if(!xml.save_file((temp/"library.xml").c_str())) {error="Cannot write GLB library metadata.";return false;}
    const auto collision=NativeCollisionImport::Read(temp/"ptpro_mesh.3ds");
    if((!model.boxes.empty() && (!collision.Valid()||collision.boxes.size()!=model.boxes.size())) ||
       (model.boxes.empty() && collision.error!=NativeCollisionImport::NoNativeHelpersError)) {
        error="GLB collision round-trip failed.";return false;
    }
    Native3DSVisualMetadata::Visual visual;
    if(!Native3DSVisualMetadata::Read(temp/"ptpro_mesh.3ds","ptpro_mesh",visual,error))return false;
    if(visual.vertices.size()!=model.vertices.size()||visual.faces.size()!=model.indices.size()/3 ||
       visual.materials.size()!=1||visual.materials.front().texture!=texture) {error="GLB native visual round-trip failed.";return false;}
    // The archive is staged INSIDE our private folder, then published together.
    const auto archive=temp/"package.tara";
    if(!NativeTaraWriter::PackDirectory(temp,archive,error))return false;
    fs::rename(temp,final,ec);
    if(ec) {error="Cannot finalize GLB library: "+ec.message();return false;}
    cleanup.p=final;
    fs::rename(final/"package.tara",taraFinal,ec);
    if(ec) {error="Cannot finalize GLB TARA: "+ec.message();return false;}
    cleanup.p.clear();saved=final;error.clear();return true;
}
}
