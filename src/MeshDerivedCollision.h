#pragma once
// Last-resort collision authoring for legacy library models which have no
// native helpers and no collision verified against a source map.
// These triangles describe the VISIBLE mesh, NOT its original gameplay
// collision. Callers must distinguish this approximation from source data.
#include "NativeCollisionImport.h"
#include <array>
#include <cmath>

namespace MeshDerivedCollision {
using namespace NativeCollisionImport;
inline Result Read(const std::filesystem::path& file,const std::string& objectName={}) {
    Result out;Native3DSScene::Scene scene;Native3DSScene::Selection selected;
    if(!Native3DSScene::Read(file,scene,out.error)||
       !Native3DSScene::Resolve(scene,objectName,selected,out.error))return out;
    out.visualAnchor=scene.nodes[selected.node].name;
    // The original renderer picks this exact visual mesh. Do not silently
    // discard native helpers (including incorrectly parented ones) or other
    // geometry that might be needed to represent a composite object.
    for(const auto& f:scene.frames) {
        if(Prefix(f.name,"plane")||Prefix(f.name,"box")||Prefix(f.name,"tri")) {
            out.error="Unresolved original collision helper exists; visual-mesh fallback is unsafe.";
            return out;
        }
    }
    for(size_t i=0;i<scene.frames.size();++i)
        if(static_cast<int>(i)!=selected.frame && scene.frames[i].parent==selected.frame) {
            out.error="Multi-part 3DS hierarchy requires original collision data.";
            return out;
        }
    const auto& node=scene.nodes[selected.node];
    if(node.faces.size()>2048 || node.vertices.empty()) {
        out.error="Visual mesh is empty or exceeds 2048 collision triangles.";
        return out;
    }
    const auto* frame=Native3DSScene::SelectedFrame(scene,selected);
    if(frame && (Norm(Sub(frame->scale,{1.f,1.f,1.f}))>.0001f ||
                 !Finite(frame->position) || !Finite(frame->rotation))) {
        out.error="Unsupported 3DS animation scale or transform for visual-derived collision.";
        return out;
    }
    std::vector<V> vertices;vertices.reserve(node.vertices.size());
    for(auto world:node.vertices) {
        V local{};
        if(!Native3DSScene::LocalVertex(node,frame,world,local)) {
            out.error="Cannot normalize visible 3DS mesh vertices.";
            return out;
        }
        if(frame)local=Add(Native3DSScene::Rotate(local,frame->rotation),frame->position);
        if(!Finite(local)) {
            out.error="Visual mesh coordinates out of supported range.";
            return out;
        }
        vertices.push_back(local);
    }
    // Match the exact local triangle convention used by native tri helpers.
    // Reject degenerate/zero-area faces rather than exporting corrupt physics.
    for(const auto& face:node.faces) {
        const V a=vertices[face[0]],b=vertices[face[1]],c=vertices[face[2]];
        const V ab=Sub(b,a),ac=Sub(c,a);
        const V normal=Cross(ab,ac);
        if(Norm(ab)<.01f || Norm(normal)<.01f) {
            // A zero-area face cannot physically collide. Preserve every
            // remaining genuine face and disclose exactly what was dropped.
            ++out.ignoredDegenerateFaces;
            continue;
        }
        // Pick the edge most nearly horizontal for a numerically stable
        // Euler basis. Choosing AB unconditionally becomes singular for the
        // vertical faces found in Fogtown/peter.3ds.
        const std::array<V,3> edges{ab,Sub(c,b),Sub(a,c)};
        V basis=ab;double best=-1.0;
        for(const auto& edge:edges) {
            const double horiz=double(edge.x)*edge.x+double(edge.y)*edge.y;
            const double total=horiz+double(edge.z)*edge.z;
            if(total>1.e-10 && horiz/total>best) {best=horiz/total;basis=edge;}
        }
        const V x=Unit(basis),z=Unit(normal),y=Cross(z,x);
        const V center=Mul(Add(Add(a,b),c),1.f/3.f);
        VerifiedCollisionTemplates::Triangle t;
        t.offset=center;t.rotation=Euler(x,y,z);
        const auto project=[&](V v){v=Sub(v,center);return V{Dot(v,x),Dot(v,y),0.f};};
        t.v0=project(a);t.v1=project(b);t.v2=project(c);
        if(!Finite(t.offset)||!Finite(t.rotation)||!Finite(t.v0)||!Finite(t.v1)||!Finite(t.v2)) {
            out.error="Invalid generated collision transform.";out.triangles.clear();return out;
        }
        out.triangles.push_back(t);
    }
    if(out.triangles.empty())out.error="Visual model contains no collision-capable surfaces.";
    else {out.meshDerived=true;out.error.clear();}
    return out;
}
}
