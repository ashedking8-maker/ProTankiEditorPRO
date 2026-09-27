#pragma once
// Native Fogtown floor footprints independently cross-checked against the
// ORIGINAL map_fogtown.xml (all instances of each of these six identities).
// The source .3ds visual mesh is checked before authoring: this is NOT a
// generic bounding-box collider for arbitrary props, roofs or decorations.
#include "NativeCollisionImport.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <string>

namespace VerifiedGroundCollision {
struct Spec {const char* name;float x,y,z,width,length,visualWidth,visualLength;};
inline constexpr const char* SupportedNames[]={"t11","t21","t22","t32","t33","t55"};
inline size_t Index(const Spec& spec) {
    for(size_t i=0;i<6;++i)if(std::string(spec.name)==SupportedNames[i])return i;
    return 6;
}
inline const Spec* Find(const std::string& library,const std::string& group,const std::string& name) {
    if(library!="Fogtown"||group!="l")return nullptr;
    static constexpr Spec source[]={{"t11",0,0,0,500,500,500,500},
        {"t21",0,-250,0,500,1000,1000,500},{"t22",0,0,0,1000,1000,1000,1000},
        {"t32",250,0,0,1000,1500,1500,1000},{"t33",-250,-250,0,1500,1500,1500,1500},
        {"t55",-250,250,0,2500,2500,2500,2500}};
    for(const auto& item:source)if(name==item.name)return &item;
    return nullptr;
}
struct Probe {bool flatRectangle{};bool matchesNativeReference{};std::string reason;};
// Detects potential missing floor surfaces in OTHER libraries too, without
// pretending their unverified game collision is known. A decal or roof can be
// deliberately placed visual-only using the explicit UI opt-in.
inline Probe Inspect(const std::filesystem::path& path,const Spec* reference=nullptr) {
    Probe out;
    if(path.empty()||path.extension()!=".3ds") {out.reason="Not an original 3DS mesh.";return out;}
    std::string error;
    const auto nodes=NativeCollisionImport::ReadNodes(path,error);
    if(!error.empty()){out.reason=error;return out;}
    const NativeCollisionImport::Node* mesh=nullptr;
    for(const auto& n:nodes) {
        if(NativeCollisionImport::Prefix(n.name,"plane")||NativeCollisionImport::Prefix(n.name,"tri")||
           NativeCollisionImport::Prefix(n.name,"box")) {
            out.reason="Source 3DS now has collision helpers; do not append a second ground template.";return out;
        }
        if(NativeCollisionImport::Prefix(n.name,"occl"))continue;
        if(n.vertices.empty()||n.faces.empty())continue;
        if(mesh){out.reason="Multiple visual meshes; flat floor cannot be inferred.";return out;}
        mesh=&n;
    }
    if(!mesh||!mesh->hasMatrix) {out.reason="Missing visual mesh/pivot.";return out;}
    // Source vertex coordinates include the 3DS translation; require a rigid,
    // axis-aligned, Z-up source frame and a true, fully triangulated rectangle.
    const auto& m=mesh->matrix;
    for(int i=0;i<9;++i)if(std::fabs(m[static_cast<size_t>(i)]-(i%4==0?1.f:0.f))>.003f) {
        out.reason="Non-axis-aligned visual pivot; no automatic floor.";return out;
    }
    float loX=1.e9f,hiX=-1.e9f,loY=1.e9f,hiY=-1.e9f,loZ=1.e9f,hiZ=-1.e9f;
    for(const auto& v:mesh->vertices) {
        loX=std::min(loX,v.x);hiX=std::max(hiX,v.x);
        loY=std::min(loY,v.y);hiY=std::max(hiY,v.y);
        loZ=std::min(loZ,v.z);hiZ=std::max(hiZ,v.z);
    }
    const float width=hiX-loX,length=hiY-loY;
    if(mesh->vertices.size()>256||mesh->faces.size()>256||width<1||length<1||
       width>10000||length>10000||hiZ-loZ>.02f) {
        out.reason="Mesh is not a bounded flat floor.";return out;
    }
    double area=0;
    for(const auto& f:mesh->faces){
        if(f[0]>=mesh->vertices.size()||f[1]>=mesh->vertices.size()||f[2]>=mesh->vertices.size()) {
            out.reason="Invalid floor triangle indices.";return out;
        }
        const auto a=mesh->vertices[f[0]],b=mesh->vertices[f[1]],c=mesh->vertices[f[2]];
        const double cross=(double(b.x)-a.x)*(double(c.y)-a.y)-(double(b.y)-a.y)*(double(c.x)-a.x);
        if(std::fabs(cross)<.0001) {out.reason="Degenerate floor triangle.";return out;}
        area+=std::fabs(cross)*.5;
    }
    // A mesh with a cut-out has less covered area; overlapping faces have more.
    if(std::fabs(area-double(width)*length)>std::max(1.,double(width)*length*.0001)) {
        out.reason="Visual floor has holes or overlapping faces.";return out;
    }
    out.flatRectangle=true;
    if(!reference){out.reason="Flat visual mesh, but no verified original-game collision template.";return out;}
    if(std::fabs(width-reference->visualWidth)>.1f||std::fabs(length-reference->visualLength)>.1f||
       std::fabs((loX+hiX)*.5f-m[9]-reference->x)>.1f||
       std::fabs((loY+hiY)*.5f-m[10]-reference->y)>.1f||
       std::fabs(loZ-m[11]-reference->z)>.1f) {
        out.reason="Selected 3DS geometry differs from the verified original Fogtown floor.";return out;
    }
    out.matchesNativeReference=true;
    out.reason="Original Fogtown map footprint and selected 3DS geometry agree.";
    return out;
}
} // namespace VerifiedGroundCollision
