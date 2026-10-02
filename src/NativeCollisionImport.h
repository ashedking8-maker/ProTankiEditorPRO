#pragma once
// Pure CPU 3DS helper-geometry reader for native map collision authoring.
// It reads ORIGINAL vertices/faces, never infers colliders from the visual mesh.
// Unsupported/corrupt helpers fail closed instead of creating plausible but
// unverified invisible walls. GLB authoring drafts do not use this game exporter.
#include "VerifiedCollisionTemplates.h"
#include "Native3DSScene.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
#include <iterator>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

namespace NativeCollisionImport {
using V = DirectX::XMFLOAT3;
inline constexpr const char* NoNativeHelpersError="No native plane/box/triangle helpers in this model.";
struct Result {
    std::vector<VerifiedCollisionTemplates::Plane> planes;
    std::vector<VerifiedCollisionTemplates::Triangle> triangles;
    struct Box { V offset{}, rotation{}, size{}; };
    std::vector<Box> boxes;
    size_t occlusionHelpers{};
    std::string visualAnchor;
    std::string error;
    bool Valid() const { return error.empty() && (!planes.empty() || !triangles.empty() || !boxes.empty()); }
};
inline V Add(V a,V b) {return {a.x+b.x,a.y+b.y,a.z+b.z};}
inline V Sub(V a,V b) {return {a.x-b.x,a.y-b.y,a.z-b.z};}
inline V Mul(V a,float v) {return {a.x*v,a.y*v,a.z*v};}
inline float Dot(V a,V b) {return a.x*b.x+a.y*b.y+a.z*b.z;}
inline V Cross(V a,V b) {return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
inline float Norm(V a) {return std::sqrt(Dot(a,a));}
inline bool Finite(V a) {return std::isfinite(a.x)&&std::isfinite(a.y)&&std::isfinite(a.z) &&
    std::fabs(a.x)<1.e7f&&std::fabs(a.y)<1.e7f&&std::fabs(a.z)<1.e7f;}
inline V Unit(V a) {return Mul(a,1.f/Norm(a));}
inline bool Prefix(std::string a,const char* b) {
    std::transform(a.begin(),a.end(),a.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
    return a.rfind(b,0)==0;
}
// Columns of a RIGHT-HANDED local-to-world rotation matrix, with world Z up.
// The native format's transform is Rx * Ry * Rz in DirectX row-vector order.
inline V Euler(V x,V y,V n) {
    const float pitch=std::asin(std::clamp(-x.z,-1.f,1.f));
    if (std::fabs(std::cos(pitch))<1.e-5f)
        return {0.f,pitch,std::atan2(-y.x,y.y)};
    return {std::atan2(y.z,n.z),pitch,std::atan2(x.y,x.x)};
}
using Node=Native3DSScene::Node;
inline std::vector<Node> ReadNodes(const std::filesystem::path& file,std::string& error){
    Native3DSScene::Scene scene;if(!Native3DSScene::Read(file,scene,error))return {};
    return std::move(scene.nodes);
}
// The original exports direct children of the library-selected visual object.
// Box/Plane/Tri classify those children, NOT all objects in a flat 3DS list.
inline Result Read(const std::filesystem::path& file,const std::string& objectName={}) {
    Result result;Native3DSScene::Scene scene;Native3DSScene::Selection visual;
    if(!Native3DSScene::Read(file,scene,result.error)||!Native3DSScene::Resolve(scene,objectName,visual,result.error))return result;
    result.visualAnchor=scene.nodes[visual.node].name;
    const auto fail=[&](std::string why){result.error=std::move(why);result.planes.clear();result.boxes.clear();result.triangles.clear();return result;};
    // No animation hierarchy means every mesh is a root, not a helper child.
    for(size_t fi=0;fi<scene.frames.size();++fi){
        const auto& frame=scene.frames[fi];if(frame.parent!=visual.frame||static_cast<int>(fi)==visual.frame)continue;
        if(Prefix(frame.name,"occ")){++result.occlusionHelpers;continue;}
        const bool plane=Prefix(frame.name,"plane"),box=Prefix(frame.name,"box"),tri=Prefix(frame.name,"tri");
        if(!plane&&!box&&!tri)continue;
        const Node* n=nullptr;
        for(const auto& node:scene.nodes)if(node.name==frame.name){if(n)return fail("Ambiguous 3DS helper object: "+frame.name);n=&node;}
        if(!n||n->faces.empty()||n->vertices.empty())return fail("Missing 3DS helper mesh: "+frame.name);
        std::vector<V> v;v.reserve(n->vertices.size());
        for(V world:n->vertices){V local{};if(!Native3DSScene::LocalVertex(*n,&frame,world,local))return fail("Invalid 3DS helper local frame: "+frame.name);v.push_back(local);}
        // Collision*.parse applies helper rotation and position. It does not
        // multiply by helper.scale; local vertices already came through inverse(0x4160).
        const auto rotate=[&](V p){return Native3DSScene::Rotate(p,frame.rotation);};
        const auto place=[&](V p){return Add(rotate(p),frame.position);};
        const auto rotation=[&](V x,V y,V z){return Euler(rotate(x),rotate(y),rotate(z));};
        if(box){
            V lo=v.front(),hi=lo;
            for(V p:v){lo={std::min(lo.x,p.x),std::min(lo.y,p.y),std::min(lo.z,p.z)};hi={std::max(hi.x,p.x),std::max(hi.y,p.y),std::max(hi.z,p.z)};}
            Result::Box out;out.size=Sub(hi,lo);out.offset=place(Mul(Add(lo,hi),.5f));out.rotation=frame.rotation;
            if(out.size.x<.01f||out.size.y<.01f||out.size.z<.01f)return fail("Degenerate 3DS box helper: "+frame.name);
            if(!Finite(out.offset)||!Finite(out.rotation)||!Finite(out.size))return fail("Out-of-range normalized 3DS box helper: "+frame.name);
            result.boxes.push_back(out);
        }else{
            // Mesh._faces is keyed by numeric face ids; the first face supplies
            // the basis in CollisionRect/Triangle.parse, not the four-vertex AABB.
            const auto f=n->faces.front();const V a=v[f[0]],b=v[f[1]],c=v[f[2]];
            if(tri){
                V x=Sub(b,a),z=Cross(x,Sub(c,a));
                if(Norm(x)<.01f||Norm(z)<.01f)return fail("Degenerate triangle helper: "+frame.name);
                x=Unit(x);z=Unit(z);const V y=Cross(z,x),center=Mul(Add(Add(a,b),c),1.f/3.f);
                auto local=[&](V p){p=Sub(p,center);return V{Dot(p,x),Dot(p,y),0};};
                VerifiedCollisionTemplates::Triangle out;out.offset=place(center);out.rotation=rotation(x,y,z);
                out.v0=local(a);out.v1=local(b);out.v2=local(c);
                if(!Finite(out.offset)||!Finite(out.rotation)||!Finite(out.v0)||!Finite(out.v1)||!Finite(out.v2))
                    return fail("Out-of-range normalized 3DS triangle helper: "+frame.name);
                result.triangles.push_back(out);
            }else{
                const std::array<V,3> points{a,b,c},edges{Sub(b,a),Sub(c,b),Sub(a,c)};
                const std::array<float,3> lengths{Norm(edges[0]),Norm(edges[1]),Norm(edges[2])};
                size_t diagonal=0;for(size_t k=1;k<3;++k)if(lengths[k]>lengths[diagonal])diagonal=k;
                const size_t xi=(diagonal+2)%3,yi=(diagonal+1)%3;
                V x=edges[xi],y=Mul(edges[yi],-1.f);float width=lengths[xi],length=lengths[yi];
                if(width<.01f||length<.01f||Norm(Cross(x,y))<.01f)return fail("Degenerate plane helper: "+frame.name);
                const V center=Add(points[(diagonal+2)%3],Mul(Add(x,y),.5f));
                x=Unit(x);y=Unit(y);V z=Cross(x,y);
                // The original expects a right-triangle half of a rectangle.
                if(std::fabs(Dot(x,y))>.003f)return fail("Nonrectangular 3DS plane helper: "+frame.name);
                VerifiedCollisionTemplates::Plane out;out.offset=place(center);out.rotation=rotation(x,y,Unit(z));out.width=width;out.length=length;
                if(!Finite(out.offset)||!Finite(out.rotation)||!std::isfinite(out.width)||!std::isfinite(out.length)||
                   out.width>1.e7f||out.length>1.e7f)return fail("Out-of-range normalized 3DS plane helper: "+frame.name);
                result.planes.push_back(out);
            }
        }
        if(result.planes.size()+result.boxes.size()+result.triangles.size()>2048)return fail("Excessive 3DS helper count.");
    }
    if(result.planes.empty()&&result.boxes.empty()&&result.triangles.empty())result.error=NoNativeHelpersError;
    return result;
}
} // namespace NativeCollisionImport
