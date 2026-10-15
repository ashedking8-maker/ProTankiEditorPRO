#pragma once
// Runtime templates proven against source-map geometry, scoped to asset identity AND exact local mesh.
#include "NativeCollisionImport.h"
#include <mutex>
#include <map>
namespace SourceCollisionCatalog {
using namespace NativeCollisionImport;
struct Mesh {
    std::vector<V> vertices;
    std::vector<std::array<std::uint16_t,3>> faces;
};
inline bool Capture(const std::filesystem::path& file,const std::string& object,Mesh& out) {
    out={};Native3DSScene::Scene scene;Native3DSScene::Selection selected;std::string error;
    if(!Native3DSScene::Read(file,scene,error)||!Native3DSScene::Resolve(scene,object,selected,error))return false;
    for(size_t i=0;i<scene.frames.size();++i)
        if(static_cast<int>(i)!=selected.frame && scene.frames[i].parent==selected.frame)return false;
    const auto* frame=Native3DSScene::SelectedFrame(scene,selected);
    if(frame && (Norm(frame->rotation)>.00001f || Norm(Sub(frame->scale,{1,1,1}))>.00001f))return false;
    const auto& n=scene.nodes[selected.node];
    if(n.faces.empty()||n.faces.size()>4096||n.vertices.size()>16384)return false;
    out.faces=n.faces;
    for(auto v:n.vertices) { V local{};if(!Native3DSScene::LocalVertex(n,frame,v,local)||!Finite(local))return false;out.vertices.push_back(local); }
    return true;
}
inline bool SameMesh(const Mesh& a,const Mesh& b) {
    if(a.vertices.size()!=b.vertices.size()||a.faces!=b.faces)return false;
    for(size_t i=0;i<a.vertices.size();++i) {
        const auto x=a.vertices[i],y=b.vertices[i];
        if(x.x!=y.x||x.y!=y.y||x.z!=y.z)return false;
    }
    return true;
}
using Polygon=std::vector<V>;
inline Polygon Corners(const VerifiedCollisionTemplates::Plane& p) {
    Polygon out;
    for(V v: {V{-p.width/2,-p.length/2,0},V{p.width/2,-p.length/2,0},V{p.width/2,p.length/2,0},V{-p.width/2,p.length/2,0}})
        out.push_back(Add(p.offset,Native3DSScene::Rotate(v,p.rotation)));
    return out;
}
inline Polygon Corners(const VerifiedCollisionTemplates::Triangle& t) {
    Polygon out;for(V v:{t.v0,t.v1,t.v2})out.push_back(Add(t.offset,Native3DSScene::Rotate(v,t.rotation)));return out;
}
inline bool SamePolygon(const Polygon& a,const Polygon& b) {
    if(a.size()!=b.size()||a.size()<3)return false;
    const auto na=Cross(Sub(a[1],a[0]),Sub(a[2],a[0])),nb=Cross(Sub(b[1],b[0]),Sub(b[2],b[0]));
    if(Norm(na)<.01f||Norm(nb)<.01f||Dot(Unit(na),Unit(nb))<.999f)return false;
    std::vector<bool> used(b.size());
    for(auto v:a){bool found=false;for(size_t i=0;i<b.size();++i)if(!used[i]&&Norm(Sub(v,b[i]))<.09f){used[i]=true;found=true;break;}if(!found)return false;}
    return true;
}
inline bool Equivalent(const Result& a,const Result& b) {
    if(!a.boxes.empty()||!b.boxes.empty()||a.planes.size()!=b.planes.size()||a.triangles.size()!=b.triangles.size())return false;
    const auto same=[](const auto& x,const auto& y) {
        std::vector<bool> used(y.size());
        for(const auto& item:x){bool found=false;for(size_t i=0;i<y.size();++i)if(!used[i]&&SamePolygon(Corners(item),Corners(y[i]))){used[i]=true;found=true;break;}if(!found)return false;}
        return true;
    };
    return same(a.planes,b.planes)&&same(a.triangles,b.triangles);
}
inline std::string Identity(std::string library,std::string group,std::string name) {
    const auto field=[](std::string v){std::transform(v.begin(),v.end(),v.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});return std::to_string(v.size())+":"+v;};
    return field(std::move(library))+field(std::move(group))+field(std::move(name));
}
struct Entry {Mesh mesh;Result collision;std::string identity;bool conflicted{};};
inline std::vector<Entry>& Entries(){static std::vector<Entry> entries;return entries;}
inline std::mutex& Mutex(){static std::mutex mutex;return mutex;}
// Differing source-map physics for the same exact mesh disables reuse.
inline size_t& Revision(){static size_t revision=0;return revision;}
inline bool Register(Mesh mesh,Result collision,const std::string& identity) {
    if(!collision.Valid()||identity.empty())return false;
    std::lock_guard lock(Mutex());
    for(auto& e:Entries())if(e.identity==identity&&SameMesh(e.mesh,mesh)) {
        if(!Equivalent(e.collision,collision)&&!e.conflicted){e.conflicted=true;++Revision();}
        return false;
    }
    if(Entries().size()>=4096)return false;
    Entries().push_back({std::move(mesh),std::move(collision),identity,false});++Revision();return true;
}
inline Result Lookup(const Mesh& mesh,const std::string& identity) {
    std::lock_guard lock(Mutex());
    for(const auto& e:Entries())if(e.identity==identity&&SameMesh(e.mesh,mesh)) {
        if(!e.conflicted)return e.collision;
        Result r;r.error="Source maps disagree about collision for this mesh.";return r;
    }
    Result r;r.error="No complete source-map collision reference for this mesh.";return r;
}
inline Result Read(const std::filesystem::path& file,const std::string& object={},const std::string& identity={}) {
    Mesh mesh;if(Capture(file,object,mesh))return Lookup(mesh,identity);
    Result r;r.error="Unsupported source-map mesh signature.";return r;
}
}
