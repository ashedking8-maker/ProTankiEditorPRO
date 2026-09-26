#pragma once
// Narrow, fail-closed terrain export: original verified triangle-only 3DS helper
// mesh + either unchanged visual geometry or ONE edited visual vertex. It
// conforms source helper heights to visual height deltas and splits the source
// triangle containing a newly introduced peak. Not a general mesh collider.
#include "Native3DSWriter.h"
#include "NativeCollisionImport.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <limits>
#include <string>
#include <vector>

namespace NativeTerrainDelta {
using V=NativeCollisionImport::V;
using Face=std::array<std::uint16_t,3>;
using Tri=Native3DSWriter::Triangle;
inline float XYDist(V a,V b){return std::hypot(a.x-b.x,a.y-b.y);}
inline V Minus(V a,V b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
inline V Cross(V a,V b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
inline float Dot(V a,V b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline bool Barycentric(float x,float y,V a,V b,V c,float& u,float& v,float& w){
    const float det=(b.y-c.y)*(a.x-c.x)+(c.x-b.x)*(a.y-c.y);
    if(std::abs(det)<1.e-5f)return false;
    u=((b.y-c.y)*(x-c.x)+(c.x-b.x)*(y-c.y))/det;
    v=((c.y-a.y)*(x-c.x)+(a.x-c.x)*(y-c.y))/det;w=1.f-u-v;
    return u>=-1.e-5f&&v>=-1.e-5f&&w>=-1.e-5f;
}
inline bool Sample(float x,float y,const std::vector<V>& points,const std::vector<Face>& faces,float& z){
    bool hit=false;z=-std::numeric_limits<float>::infinity();
    for(const auto& face:faces){
        if(face[0]>=points.size()||face[1]>=points.size()||face[2]>=points.size())return false;
        const auto a=points[face[0]],b=points[face[1]],c=points[face[2]];
        float u{},v{},w{};if(!Barycentric(x,y,a,b,c,u,v,w))continue;
        z=std::max(z,u*a.z+v*b.z+w*c.z);hit=true;
    }
    return hit;
}
inline bool Build(const std::filesystem::path& source,const Native3DSWriter::Model& updated,
                  std::vector<Tri>& tris,std::string& error){
    tris.clear();
    const auto native=NativeCollisionImport::Read(source);
    if(!native.Valid()||native.triangles.empty()||!native.planes.empty()||!native.boxes.empty()||native.triangles.size()>1024){
        error="Terrain conversion requires a verified triangle-only original 3DS helper mesh.";return false;
    }
    std::string parseError;const auto nodes=NativeCollisionImport::ReadNodes(source,parseError);
    if(!parseError.empty()){error=parseError;return false;}
    const NativeCollisionImport::Node* visual=nullptr;
    for(const auto& n:nodes)if(n.name==native.visualAnchor){visual=&n;break;}
    if(!visual||!visual->hasMatrix||visual->vertices.size()!=updated.vertices.size()||
       visual->faces.size()!=updated.indices.size()/3){error="Terrain visual topology differs from original.";return false;}
    for(int k=0;k<9;++k)if(std::abs(visual->matrix[static_cast<size_t>(k)]-(k==0||k==4||k==8?1.f:0.f))>.002f){
        error="Terrain mesh uses a rotated/scaled pivot not covered by this exporter.";return false;
    }
    const V pivot{visual->matrix[9],visual->matrix[10],visual->matrix[11]};
    std::vector<V> original,edited;original.reserve(visual->vertices.size());edited.reserve(updated.vertices.size());
    for(V v:visual->vertices)original.push_back(Minus(v,pivot));
    for(const auto& v:updated.vertices)edited.push_back({v.x,v.z,v.y}); // editor Y-up -> original Z-up
    std::vector<bool> used(original.size());std::vector<V> moved;
    for(const auto& v:edited){
        size_t matched=original.size();
        for(size_t i=0;i<original.size();++i)if(!used[i]&&
            XYDist(v,original[i])<.05f&&std::abs(v.z-original[i].z)<.05f){matched=i;break;}
        if(matched<original.size())used[matched]=true;else moved.push_back(v);
    }
    if(moved.size()>1){error="Multiple visual edits need a separately validated terrain collision algorithm; export refused.";return false;}
    if(moved.size()==1 && std::count(used.begin(),used.end(),false)!=1){error="Terrain edited vertex matching is ambiguous.";return false;}
    std::vector<Face> faces=visual->faces;for(const auto& f:faces)for(auto id:f)if(id>=original.size()){
        error="Original terrain has invalid visual faces.";return false;
    }
    // Output face orientation may be reversed to undo the editor handedness;
    // only XY barycentric heights are used, so either consistent winding works.
    std::vector<Face> editedFaces;editedFaces.reserve(updated.indices.size()/3);
    for(size_t i=0;i<updated.indices.size();i+=3){
        if(updated.indices[i]>65535||updated.indices[i+1]>65535||updated.indices[i+2]>65535){
            error="Terrain indices exceed 3DS limits.";return false;
        }
        editedFaces.push_back({static_cast<std::uint16_t>(updated.indices[i]),
                               static_cast<std::uint16_t>(updated.indices[i+1]),static_cast<std::uint16_t>(updated.indices[i+2])});
    }
    // Verify that source triangle frames share the original visual pivot;
    // a shifted helper cannot be safely transformed by a heightfield delta.
    size_t containing=0;bool haveContaining=false;size_t originals=0;
    for(const auto& node:nodes){
        if(!NativeCollisionImport::Prefix(node.name,"tri"))continue;
        ++originals;
        if(!node.hasMatrix||node.vertices.size()!=3||node.faces.size()!=1){error="Unsupported original terrain triangle.";return false;}
        for(int k=0;k<12;++k)if(std::abs(node.matrix[static_cast<size_t>(k)]-visual->matrix[static_cast<size_t>(k)])>.002f){
            error="Terrain helper and visual frames differ.";return false;
        }
        const auto f=node.faces.front();if(f[0]>2||f[1]>2||f[2]>2){error="Invalid original terrain triangle.";return false;}
        std::array<V,3> points{{Minus(node.vertices[f[0]],pivot),Minus(node.vertices[f[1]],pivot),Minus(node.vertices[f[2]],pivot)}};
        if(!moved.empty()){
            float u{},v{},w{};
            if(Barycentric(moved[0].x,moved[0].y,points[0],points[1],points[2],u,v,w)&&std::min({u,v,w})>1.e-4f){
                ++containing;haveContaining=true;
            }
        }
    }
    if(originals!=native.triangles.size()||(!moved.empty()&&(!haveContaining||containing!=1))){
        error="New terrain peak must lie strictly inside exactly one original collision triangle.";return false;
    }
    for(const auto& node:nodes){
        if(!NativeCollisionImport::Prefix(node.name,"tri"))continue;
        const auto face=node.faces.front();
        std::array<V,3> p{{Minus(node.vertices[face[0]],pivot),Minus(node.vertices[face[1]],pivot),Minus(node.vertices[face[2]],pivot)}};
        if(!moved.empty())for(auto& v:p){
            float h0{},h1{};if(!Sample(v.x,v.y,original,faces,h0)||!Sample(v.x,v.y,edited,editedFaces,h1)){
                error="Terrain helper vertex is outside source/edited visible surface.";return false;
            }
            v.z+=h1-h0;
        }
        const auto add=[&](V a,V b,V c){
            tris.push_back({{{a.x,a.y,a.z},{b.x,b.y,b.z},{c.x,c.y,c.z}}});
        };
        float u{},v{},w{};const bool split=!moved.empty()&&
            Barycentric(moved[0].x,moved[0].y,p[0],p[1],p[2],u,v,w)&&std::min({u,v,w})>1.e-4f;
        if(split){add(p[0],p[1],moved[0]);add(p[1],p[2],moved[0]);add(p[2],p[0],moved[0]);}
        else add(p[0],p[1],p[2]);
    }
    if(tris.size()!=native.triangles.size()+(moved.empty()?0u:2u)){
        error="Terrain helper splitting changed the expected count.";tris.clear();return false;
    }
    for(const auto& t:tris){
        const V a{t[0][0],t[0][1],t[0][2]},b{t[1][0],t[1][1],t[1][2]},c{t[2][0],t[2][1],t[2][2]};
        const V normal=Cross(Minus(b,a),Minus(c,a));
        if(!std::isfinite(normal.z)||normal.z<.001f||
           normal.z/std::sqrt(Dot(normal,normal))<.17f){
            error="Terrain collider has a degenerate/inverted/too-steep face; export refused.";tris.clear();return false;
        }
    }
    error.clear();return true;
}
} // namespace NativeTerrainDelta
