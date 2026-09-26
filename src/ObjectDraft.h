#pragma once
// An isolated, versioned authoring document. It deliberately does NOT write
// library.xml, 3DS collision helper nodes, or legacy map XML.
#include <array>
#include <algorithm>
#include <cmath>
#include <cctype>
#include <cwctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace ObjectDraft {
namespace fs = std::filesystem;
enum class BoxRole { Solid, Trigger }; // preview annotations, NOT native game flags
// Authoring intention only; none of these values is a native ProTLVK material,
// physics or particle identifier. Native game export remains disabled.
enum class Purpose { Decorative, SolidDraft, DriveableDraft, TriggerDraft };
inline const char* PurposeToken(Purpose purpose) {
    switch(purpose) {
    case Purpose::Decorative:return "decorative";
    case Purpose::SolidDraft:return "solid_draft";
    case Purpose::DriveableDraft:return "driveable_draft";
    case Purpose::TriggerDraft:return "trigger_draft";
    }
    return "solid_draft";
}
inline bool ParsePurpose(const std::string& raw,Purpose& purpose) {
    for(const auto candidate:{Purpose::Decorative,Purpose::SolidDraft,Purpose::DriveableDraft,Purpose::TriggerDraft})
        if(raw==PurposeToken(candidate)) {purpose=candidate;return true;}
    return false;
}
struct Box {
    std::array<float,3> min{-250.f,-250.f,0.f};
    std::array<float,3> max{250.f,250.f,300.f};
    BoxRole role{BoxRole::Solid};
};
struct Document {
    std::string name;
    fs::path model;
    Purpose purpose{Purpose::SolidDraft};
    std::array<float,3> tint{1.f,1.f,1.f}; // authoring tint, NOT a game material export
    std::vector<Box> boxes{Box{}};
    // Internal renderer Y-up vertices. A self-contained edit layer over the source
    // model; this is NOT a game-native mesh or collision helper export.
    float scale{1.f};
    std::vector<std::array<float,3>> meshVertices;
    std::vector<std::uint32_t> meshIndices; // editable triangles of the authoring mesh
    // Optional read-only source library template. Keeps every original XML field,
    // including unknown extensions; NOT an instruction to reuse those fields in
    // a newly exported ProTLVK object (native export remains disabled).
    std::string templateLibrary, templateGroup, templateName, templatePropXml;
    std::shared_ptr<const std::string> libraryTemplateXml;
    // Draft-only inclusion choices. The original XML sidecars remain complete;
    // no game-native export is implied by these selections.
    std::vector<std::string> excludedTemplateFields;
};
inline bool ValidBox(const Box& b) {
    for (int i=0;i<3;++i)
        if (!std::isfinite(b.min[i]) || !std::isfinite(b.max[i]) ||
            b.min[i] >= b.max[i] || std::abs(b.min[i]) > 1000000.f || std::abs(b.max[i]) > 1000000.f) return false;
    return b.role==BoxRole::Solid || b.role==BoxRole::Trigger;
}
inline std::string Slug(const std::string& name) {
    std::string s;
    for (unsigned char c : name) {
        if (std::isalnum(c) && c < 128) s += static_cast<char>(c);
        else if (c==' ' || c=='-' || c=='_') s += '_';
    }
    if (s.size()>64) s.resize(64);
    if (s.empty() || s.find_first_not_of('_')==std::string::npos) return {};
    return s;
}
inline bool IsGlb(const fs::path& model) {
    std::wstring lower=model.extension().wstring();
    std::transform(lower.begin(),lower.end(),lower.begin(),[](wchar_t c){return static_cast<wchar_t>(std::towlower(c));});
    return lower==L".glb";
}
inline bool Validate(const Document& d, std::string& error) {
    if (d.name.empty() || d.name.size()>127 || d.name.find('\n')!=std::string::npos ||
        d.name.find('\r')!=std::string::npos || Slug(d.name).empty()) {
        error="Use a simple object name (maximum 127 characters)."; return false;
    }
    std::wstring lower=d.model.extension().wstring();
    std::transform(lower.begin(),lower.end(),lower.begin(),[](wchar_t c){return static_cast<wchar_t>(std::towlower(c));});
    if (lower!=L".glb" && lower!=L".3ds") { error="Source must be a GLB or legacy 3DS file."; return false; }
    std::error_code ec;
    if (!fs::is_regular_file(d.model,ec) || ec) {error="Source model is missing.";return false;}
    const auto size=fs::file_size(d.model,ec);
    if (ec || size<16 || size>512ull*1024*1024) {error="Source model has an invalid size.";return false;}
    if (d.boxes.size()>128 || (d.boxes.empty() && d.purpose!=Purpose::Decorative)) {
        error="Use 1-128 boxes for physical/trigger drafts, or select Decorative for a box-free object.";return false;
    }
    for (const auto& b:d.boxes) if(!ValidBox(b)) {error="Collision bounds must be finite and strictly increasing.";return false;}
    for (float v:d.tint) if(!std::isfinite(v) || v<0.f || v>1.f) {error="Material tint must be between 0 and 1.";return false;}
    if(!std::isfinite(d.scale) || d.scale<0.001f || d.scale>1000.f) {error="Draft scale must be 0.001 to 1000.";return false;}
    if(d.meshVertices.size()>200000) {error="Draft mesh edit limit is 200,000 vertices.";return false;}
    for(const auto& v:d.meshVertices)for(float coordinate:v)
        if(!std::isfinite(coordinate) || std::abs(coordinate)>1000000.f) {error="Invalid edited mesh coordinate.";return false;}
    if(d.meshIndices.size()>600000 || d.meshIndices.size()%3!=0 ||
       (!d.meshIndices.empty() && d.meshVertices.empty())) {error="Invalid draft triangle count.";return false;}
    for(const auto index:d.meshIndices)if(index>=d.meshVertices.size()) {error="Draft triangle references a missing point.";return false;}
    const bool hasTemplate=static_cast<bool>(d.libraryTemplateXml);
    if(hasTemplate && (d.libraryTemplateXml->empty() || d.libraryTemplateXml->size()>8u*1024u*1024u ||
        d.templatePropXml.empty() || d.templatePropXml.size()>2u*1024u*1024u ||
        d.templateLibrary.empty() || d.templateName.empty())) {
        error="Incomplete or oversized original library template.";return false;
    }
    if(d.excludedTemplateFields.size()>256 || (!hasTemplate && !d.excludedTemplateFields.empty())) {
        error="Invalid draft template selection.";return false;
    }
    for(const auto& field:d.excludedTemplateFields) if(field.empty()||field.size()>256||
        field.find('\n')!=std::string::npos||field.find('\r')!=std::string::npos) {
        error="Invalid draft template field path.";return false;
    }
    if(!hasTemplate && (!d.templatePropXml.empty() || !d.templateLibrary.empty() ||
        !d.templateGroup.empty() || !d.templateName.empty())) {
        error="Incomplete original library template; clear or select the complete source.";return false;
    }
    error.clear();return true;
}
inline bool ValidateGlbContainer(const fs::path& file) {
    std::ifstream in(file,std::ios::binary);
    std::uint32_t header[5]{};
    in.read(reinterpret_cast<char*>(header),sizeof(header));
    std::error_code ec;const auto length=fs::file_size(file,ec);
    return in && !ec && header[0]==0x46546C67u && header[1]==2u &&
        header[2]==length && length>=20 && header[4]==0x4E4F534Au &&
        header[3]<=length-20 && header[3]%4u==0;
}
// Compare full path components, not a naive string prefix ("library" versus
// "library_backup"). Windows path comparisons are case-insensitive.
inline bool IsWithin(const fs::path& candidate,const fs::path& parent) {
    if(candidate.empty() || parent.empty()) return false;
    std::error_code ec;
    const auto child=fs::weakly_canonical(candidate,ec);
    if(ec)return false;
    ec.clear();const auto root=fs::weakly_canonical(parent,ec);
    if(ec)return false;
    auto a=child.begin(),b=root.begin();
    for(;b!=root.end();++a,++b) {
        if(a==child.end())return false;
        auto aa=a->wstring(),bb=b->wstring();
        std::transform(aa.begin(),aa.end(),aa.begin(),[](wchar_t c){return static_cast<wchar_t>(std::towlower(c));});
        std::transform(bb.begin(),bb.end(),bb.begin(),[](wchar_t c){return static_cast<wchar_t>(std::towlower(c));});
        if(aa!=bb)return false;
    }
    return true;
}
// Saves a new self-contained draft under the user-selected ROOT, never into
// the original library automatically. Existing folders are never overwritten.
// Rename commits the whole staging folder, so failed copies leave no partial draft.
inline bool SaveNew(const Document& d,const fs::path& root,fs::path& saved,std::string& error,
                    const fs::path& originalLibraryRoot={}) {
    if (!Validate(d,error)) return false;
    if (IsGlb(d.model) && !ValidateGlbContainer(d.model)) {
        error="Invalid GLB container.";return false;
    }
    std::error_code ec;
    if (!fs::is_directory(root,ec) || ec) {error="Choose an existing output folder.";return false;}
    if(IsWithin(root,originalLibraryRoot)) {error="Do not save drafts inside the original game library.";return false;}
    const fs::path final=root/Slug(d.name);
    if (fs::exists(final,ec) || ec) {error="Draft with that name already exists. Choose a new name.";return false;}
    fs::path temp;
    for (unsigned i=0;i<1024;++i) {
        temp=root/(".draft_"+Slug(d.name)+"_"+std::to_string(i)+".tmp");
        ec.clear();if (fs::create_directory(temp,ec)) break;
        if (ec || i==1023) {error="Could not create draft staging folder.";return false;}
    }
    const auto cleanup=[&](){std::error_code ignored;fs::remove_all(temp,ignored);};
    const std::string sourceName=IsGlb(d.model)?"source.glb":"source.3ds";
    fs::copy_file(d.model,temp/sourceName,fs::copy_options::none,ec);
    if (ec) {cleanup();error="Could not copy source model: "+ec.message();return false;}
    {
        std::ofstream out(temp/"object-draft.txt",std::ios::binary|std::ios::trunc);
        out << "PROTANKI_OBJECT_DRAFT 4\n" << "name " << std::quoted(d.name) << '\n'
            << "purpose " << PurposeToken(d.purpose) << '\n'
            << "source " << sourceName << '\n' << "tint " << std::setprecision(9)
            << d.tint[0]<<' '<<d.tint[1]<<' '<<d.tint[2]<<'\n'
            << "boxes " << d.boxes.size() << '\n';
        for (const Box& b:d.boxes) {
            out << "box " << (b.role==BoxRole::Solid?"solid":"trigger");
            for (float v:b.min)out<<' '<<v;
            for (float v:b.max)out<<' '<<v;
            out<<'\n';
        }
        out << "scale " << d.scale << '\n' << "mesh_vertices " << d.meshVertices.size() << '\n';
        for(const auto& p:d.meshVertices)out << "vertex " << p[0] << ' ' << p[1] << ' ' << p[2] << '\n';
        out << "mesh_indices " << d.meshIndices.size() << '\n';
        for(size_t i=0;i<d.meshIndices.size();i+=3)
            out << "triangle " << d.meshIndices[i] << ' ' << d.meshIndices[i+1] << ' ' << d.meshIndices[i+2] << '\n';
        out << "native_export false\n";
        out.flush();
        if (!out) {cleanup();error="Could not write draft manifest.";return false;}
    }
    if(d.libraryTemplateXml) {
        // Separate sidecars preserve raw source bytes, not a reconstructed or
        // reinterpreted subset. Never modify the game's original library.xml.
        std::ofstream raw(temp/"library-source.xml",std::ios::binary|std::ios::trunc);
        raw.write(d.libraryTemplateXml->data(),static_cast<std::streamsize>(d.libraryTemplateXml->size()));
        std::ofstream prop(temp/"library-prop-template.xml",std::ios::binary|std::ios::trunc);
        prop.write(d.templatePropXml.data(),static_cast<std::streamsize>(d.templatePropXml.size()));
        std::ofstream origin(temp/"library-template-origin.txt",std::ios::binary|std::ios::trunc);
        origin << std::quoted(d.templateLibrary) << '\n' << std::quoted(d.templateGroup) << '\n'
               << std::quoted(d.templateName) << '\n';
        raw.flush();prop.flush();origin.flush();
        if(!raw || !prop || !origin) {cleanup();error="Could not store complete library template sidecars.";return false;}
        if(!d.excludedTemplateFields.empty()) {
            std::ofstream selection(temp/"library-prop-selection.txt",std::ios::binary|std::ios::trunc);
            selection << "PROTANKI_TEMPLATE_SELECTION 1 " << d.excludedTemplateFields.size() << '\n';
            for(const auto& field:d.excludedTemplateFields) selection << std::quoted(field) << '\n';
            selection.flush();
            if(!selection) {cleanup();error="Could not store draft template selection.";return false;}
        }
    }
    ec.clear();fs::rename(temp,final,ec);
    if(ec) {cleanup();error="Could not finalize draft: "+ec.message();return false;}
    saved=final;error.clear();return true;
}
inline bool Load(const fs::path& folder, Document& result,std::string& error) {
    std::ifstream in(folder/"object-draft.txt",std::ios::binary);
    std::string marker, nameKey, sourceKey, sourceName, tintKey, boxesKey, exportKey, exported;
    int version{};size_t count{};Document doc;
    if (!(in>>marker>>version) || marker!="PROTANKI_OBJECT_DRAFT" || (version!=2 && version!=3 && version!=4) ||
        !(in>>nameKey) || nameKey!="name" || !(in>>std::quoted(doc.name))) {
        error="Malformed or unsupported object draft manifest.";return false;
    }
    if(version==4) {
        std::string key,value;
        if(!(in>>key>>value) || key!="purpose" || !ParsePurpose(value,doc.purpose)) {
            error="Unsupported draft-only purpose.";return false;
        }
    }
    if (!(in>>sourceKey>>sourceName) || sourceKey!="source" ||
        (sourceName!="source.glb"&&sourceName!="source.3ds") ||
        !(in>>tintKey>>doc.tint[0]>>doc.tint[1]>>doc.tint[2]) || tintKey!="tint" ||
        !(in>>boxesKey>>count) || boxesKey!="boxes" || count>128 || (count==0&&doc.purpose!=Purpose::Decorative)) {
        error="Malformed or unsupported object draft manifest.";return false;
    }
    doc.boxes.clear();doc.model=folder/sourceName;
    for (size_t i=0;i<count;++i) {
        Box box;std::string key,role;
        if (!(in>>key>>role) || key!="box" || (role!="solid"&&role!="trigger")) {
            error="Malformed collision draft.";return false;
        }
        box.role=role=="solid"?BoxRole::Solid:BoxRole::Trigger;
        for (float& v:box.min)if(!(in>>v)){error="Invalid collision min.";return false;}
        for (float& v:box.max)if(!(in>>v)){error="Invalid collision max.";return false;}
        doc.boxes.push_back(box);
    }
    if(version>=3) {
        std::string scaleKey, meshKey;
        size_t vertices{};
        if(!(in>>scaleKey>>doc.scale) || scaleKey!="scale" ||
           !(in>>meshKey>>vertices) || meshKey!="mesh_vertices" || vertices>200000) {
            error="Malformed mesh edit header.";return false;
        }
        doc.meshVertices.reserve(vertices);
        for(size_t i=0;i<vertices;++i) {
            std::string vertexKey;std::array<float,3> vertex{};
            if(!(in>>vertexKey>>vertex[0]>>vertex[1]>>vertex[2]) || vertexKey!="vertex") {
                error="Malformed mesh vertex edit.";return false;
            }
            doc.meshVertices.push_back(vertex);
        }
        std::string indicesKey;size_t indexCount{};
        if(!(in>>indicesKey>>indexCount) || indicesKey!="mesh_indices" || indexCount>600000 || indexCount%3) {
            error="Malformed mesh triangle header.";return false;
        }
        doc.meshIndices.reserve(indexCount);
        for(size_t i=0;i<indexCount;i+=3) {
            std::string triangleKey;std::uint32_t a{},b{},c{};
            if(!(in>>triangleKey>>a>>b>>c) || triangleKey!="triangle") {
                error="Malformed mesh triangle.";return false;
            }
            doc.meshIndices.insert(doc.meshIndices.end(),{a,b,c});
        }
    }
    if (!(in>>exportKey>>exported) || exportKey!="native_export" || exported!="false") {
        error="Unsupported draft export marker.";return false;
    }
    std::error_code ec;
    const auto librarySidecar=folder/"library-source.xml";
    const auto propSidecar=folder/"library-prop-template.xml";
    const auto originSidecar=folder/"library-template-origin.txt";
    const bool hasAny=fs::exists(librarySidecar,ec)||fs::exists(propSidecar,ec)||fs::exists(originSidecar,ec);
    if(ec) {error="Cannot examine library template sidecars.";return false;}
    if(hasAny) {
        if(!fs::is_regular_file(librarySidecar,ec)||!fs::is_regular_file(propSidecar,ec)||
            !fs::is_regular_file(originSidecar,ec)||ec ||
            fs::file_size(librarySidecar,ec)>8u*1024u*1024u ||
            fs::file_size(propSidecar,ec)>2u*1024u*1024u || ec) {
            error="Incomplete or oversized library template sidecars.";return false;
        }
        std::ifstream raw(librarySidecar,std::ios::binary),prop(propSidecar,std::ios::binary);
        std::ifstream origin(originSidecar,std::ios::binary);
        std::ostringstream sourceBuffer,propBuffer;
        sourceBuffer<<raw.rdbuf();propBuffer<<prop.rdbuf();
        if(!raw || !prop || !(origin>>std::quoted(doc.templateLibrary)
            >>std::quoted(doc.templateGroup)>>std::quoted(doc.templateName))) {
            error="Cannot read complete library template sidecars.";return false;
        }
        doc.libraryTemplateXml=std::make_shared<const std::string>(sourceBuffer.str());
        doc.templatePropXml=propBuffer.str();
        const auto selectionSidecar=folder/"library-prop-selection.txt";
        ec.clear();
        if(fs::exists(selectionSidecar,ec)) {
            if(ec || !fs::is_regular_file(selectionSidecar,ec) ||
                fs::file_size(selectionSidecar,ec)>70000u || ec) {
                error="Invalid template selection sidecar.";return false;
            }
            std::ifstream selection(selectionSidecar,std::ios::binary);
            std::string marker;int format{};size_t fields{};
            if(!(selection>>marker>>format>>fields) || marker!="PROTANKI_TEMPLATE_SELECTION" ||
                format!=1 || fields>256) {error="Invalid template selection manifest.";return false;}
            for(size_t i=0;i<fields;++i) {
                std::string field;
                if(!(selection>>std::quoted(field))) {error="Truncated template selection manifest.";return false;}
                doc.excludedTemplateFields.push_back(std::move(field));
            }
            std::string trailing;
            if(selection>>trailing) {error="Extra template selection data.";return false;}
        } else if(ec) {error="Cannot inspect template selection sidecar.";return false;}
    }
    if(!Validate(doc,error))return false;
    result=std::move(doc);error.clear();return true;
}
} // namespace ObjectDraft
