#pragma once
// Reference: all 11 land 1 and 2 land 2 instances in the supplied original
// map_fogtown.xml. Every visual face matches a same-winding native triangle.
// Only certified LOCAL geometry is eligible, never a name/bounds heuristic.
#include "NativeCollisionImport.h"
#include "SourceMapMeshCollision.h"
#include <map>
namespace VerifiedMeshSurface {
using namespace NativeCollisionImport;
struct Mesh { std::vector<V> vertices; std::vector<std::array<std::uint16_t,3>> faces; };
inline const std::array<Mesh,2>& References() {
    static const std::array<Mesh,2> meshes{{
        Mesh{{{1500.0f,-500.000336f,250.0f},{405.587494f,33.1906433f,165.530304f},{1000.0f,-606.817017f,250.0f},{1500.0f,-1000.0f,300.000031f},{-695.705505f,1151.55725f,3.60962295e-05f},{1500.0f,1500.0f,6.10351781e-05f},{-1000.0f,1500.0f,6.10351781e-05f},{1500.0f,1000.0f,3.0517589e-05f},{1500.0f,499.999847f,45.0f},{-398.154114f,818.221558f,45.0f},{1500.0f,-0.000488281104f,150.0f},{1001.0f,-1000.0f,250.0f},{500.0f,-1000.0f,150.0f},{-500.0f,-1000.0f,0.0f},{-1000.0f,-1000.0f,0.0f},{0.0f,-1000.0f,45.0f}},
             {{5,6,4},{0,2,3},{10,9,1},{7,5,4},{8,9,10},{7,4,8},{8,4,9},{1,0,10},{0,1,2},{2,11,3},{2,12,11},{2,1,12},{9,12,1},{13,15,9},{9,14,13},{4,14,9},{6,14,4},{9,15,12}}},
        Mesh{{{1500.0f,-500.000336f,0.0f},{403.248962f,9.87886238f,94.780159f},{1000.0f,-606.817017f,50.4541016f},{1500.0f,-1000.0f,0.0f},{-1000.0f,1500.0f,300.0f},{1500.0f,1500.0f,300.0f},{0.0f,-1000.0f,150.0f},{1500.0f,1000.0f,250.0f},{1500.0f,499.999847f,150.0f},{-242.396988f,555.228333f,244.364258f},{1500.0f,-0.000488281104f,45.0f},{1000.0f,-1000.0f,0.0f},{500.0f,-1000.0f,45.0f},{-500.0f,-1000.0f,250.0f},{-1000.0f,-1000.0f,300.0f}},
             {{9,6,12},{0,2,3},{10,9,1},{7,5,4},{8,9,10},{7,4,8},{8,4,9},{1,0,10},{0,1,2},{2,11,3},{2,12,11},{2,1,12},{9,12,1},{13,6,9},{9,14,13},{4,14,9}}},
    }}; return meshes;
}
inline bool ExpectedIdentity(const std::string& library,const std::string& group,const std::string& name) {
    return library=="Fogtown" && group=="l" && (name=="land 1" || name=="land 2");
}
inline Result ReadVerified(const std::filesystem::path& file,const std::string& objectName={}) {
    Result out;out.error="Mesh does not match a source-map verified surface.";
    Native3DSScene::Scene scene;Native3DSScene::Selection selected;std::string error;
    if(!Native3DSScene::Read(file,scene,error)||!Native3DSScene::Resolve(scene,objectName,selected,error)) {
        out.error=error;return out;
    }
    // No fallback for a model with children: native helpers retain precedence.
    for(size_t i=0;i<scene.frames.size();++i)
        if(static_cast<int>(i)!=selected.frame && scene.frames[i].parent==selected.frame)return out;
    const auto* frame=Native3DSScene::SelectedFrame(scene,selected);
    if(frame && (Norm(frame->rotation)>.00001f || Norm(Sub(frame->scale,{1,1,1}))>.00001f))return out;
    const auto& node=scene.nodes[selected.node];
    std::vector<V> vertices;
    for(auto v:node.vertices){V local{};if(!Native3DSScene::LocalVertex(node,frame,v,local))return out;vertices.push_back(local);}
    bool certified=false;
    for(const auto& reference:References()) {
        if(reference.vertices.size()!=vertices.size()||reference.faces!=node.faces)continue;
        bool equal=true;
        for(size_t i=0;i<vertices.size();++i)if(Norm(Sub(vertices[i],reference.vertices[i]))>.01f){equal=false;break;}
        if(equal){certified=true;break;}
    }
    if(!certified)return out;
    for(const auto& face:node.faces) {
        const V a=vertices[face[0]],b=vertices[face[1]],c=vertices[face[2]];
        V x=Sub(b,a),z=Cross(x,Sub(c,a));
        if(Norm(x)<.01f||Norm(z)<.01f){out.triangles.clear();return out;}
        x=Unit(x);z=Unit(z);const V y=Cross(z,x),center=Mul(Add(Add(a,b),c),1.f/3.f);
        const auto local=[&](V v){v=Sub(v,center);return V{Dot(v,x),Dot(v,y),0};};
        VerifiedCollisionTemplates::Triangle t;t.offset=center;t.rotation=Euler(x,y,z);
        t.v0=local(a);t.v1=local(b);t.v2=local(c);out.triangles.push_back(t);
    }
    out.visualAnchor=node.name;out.error.clear();return out;
}
inline Result Read(const std::filesystem::path& file,const std::string& objectName={}) {
    auto original=NativeCollisionImport::Read(file,objectName);
    if(original.error!=NoNativeHelpersError)return original;
    auto verified=ReadVerified(file,objectName);
    if(verified.Valid())return verified;
    const auto mapped=SourceMapMeshCollision::Read(file,objectName);
    return mapped.Valid()?mapped:original;
}
// Same matching policy is used by the document repair and portable regressions.
using Corners=std::array<V,3>;
inline Corners World(V position,V rotation,V a,V b,V c) {
    return {Add(position,Native3DSScene::Rotate(a,rotation)),
            Add(position,Native3DSScene::Rotate(b,rotation)),
            Add(position,Native3DSScene::Rotate(c,rotation))};
}
inline bool SameCorners(const Corners& a,const Corners& b) {
    std::array<bool,3> used{};
    for(auto v:a){bool found=false;for(size_t i=0;i<3;++i)if(!used[i]&&Norm(Sub(v,b[i]))<.09f){used[i]=true;found=true;break;}if(!found)return false;}
    return true;
}
inline bool SameWinding(const Corners& a,const Corners& b) {
    const V na=Cross(Sub(a[1],a[0]),Sub(a[2],a[0])),nb=Cross(Sub(b[1],b[0]),Sub(b[2],b[0]));
    return Norm(na)>.01f&&Norm(nb)>.01f&&Dot(Unit(na),Unit(nb))>.999f;
}
struct Existing {Corners corners;int owner{-1};};
// All-or-nothing: -1 denotes a missing triangle, >=0 a unique existing match.
inline bool Plan(const std::vector<Corners>& wanted,const std::vector<Existing>& existing,
                 int owner,std::vector<int>& matches,std::string& error) {
    matches.clear();error.clear();
    using Key=std::array<long long,3>;
    const auto key=[](const Corners& t){
        const auto center=Mul(Add(Add(t[0],t[1]),t[2]),1.f/3.f);
        return Key{static_cast<long long>(std::floor(center.x)),static_cast<long long>(std::floor(center.y)),static_cast<long long>(std::floor(center.z))};
    };
    std::map<Key,std::vector<size_t>> cells;
    for(size_t i=0;i<existing.size();++i)cells[key(existing[i].corners)].push_back(i);
    for(const auto& triangle:wanted) {
        int match=-1;
        const auto cell=key(triangle);std::vector<size_t> candidates;
        for(int x=-1;x<=1;++x)for(int y=-1;y<=1;++y)for(int z=-1;z<=1;++z) {
            const auto it=cells.find(Key{cell[0]+x,cell[1]+y,cell[2]+z});
            if(it!=cells.end())candidates.insert(candidates.end(),it->second.begin(),it->second.end());
        }
        for(size_t i:candidates)if(SameCorners(triangle,existing[i].corners)) {
            if(match>=0 || (existing[i].owner>=0 && existing[i].owner!=owner) ||
               !SameWinding(triangle,existing[i].corners)) {
                error="Duplicate, reversed or differently owned surface collision; repair refused.";matches.clear();return false;
            }
            match=static_cast<int>(i);
        }
        if(match>=0&&std::find(matches.begin(),matches.end(),match)!=matches.end()) {
            error="Surface template contains duplicate triangles.";matches.clear();return false;
        }
        matches.push_back(match);
    }
    for(size_t i=0;i<existing.size();++i)if(existing[i].owner==owner &&
        std::find(matches.begin(),matches.end(),static_cast<int>(i))==matches.end()) {
        error="Existing owner has additional collision; repair refused.";matches.clear();return false;
    }
    return true;
}
} // namespace VerifiedMeshSurface
