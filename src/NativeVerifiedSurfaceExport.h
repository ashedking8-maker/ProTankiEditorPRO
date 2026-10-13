#pragma once
#include "VerifiedMeshSurface.h"
#include "Native3DSWriter.h"
namespace NativeVerifiedSurfaceExport {
// Map-certified helperless terrain can preserve its exact native surface.
// Geometry changes require a separate validated collider authoring algorithm.
inline bool Build(const NativeCollisionImport::Result& certified,
                  const Native3DSWriter::Model& original, const Native3DSWriter::Model& edited,
                  std::vector<Native3DSWriter::Triangle>& triangles,std::string& error) {
    triangles.clear();
    if(!certified.Valid()||certified.triangles.empty()||!certified.boxes.empty()||!certified.planes.empty()) {
        error="No complete map-certified terrain surface.";return false;
    }
    if(original.indices!=edited.indices||original.vertices.size()!=edited.vertices.size()) {
        error="Certified terrain topology changed. Native export refused; save the editable draft instead.";return false;
    }
    for(size_t i=0;i<original.vertices.size();++i) {
        const auto& a=original.vertices[i];const auto& b=edited.vertices[i];
        if(!std::isfinite(b.x)||!std::isfinite(b.y)||!std::isfinite(b.z)||
           std::fabs(a.x-b.x)>.0001f||std::fabs(a.y-b.y)>.0001f||std::fabs(a.z-b.z)>.0001f) {
            error="Certified terrain geometry changed. Its original collision cannot be replaced by draft boxes; save the draft instead.";return false;
        }
    }
    for(const auto& t:certified.triangles) {
        const auto corners=VerifiedMeshSurface::World(t.offset,t.rotation,t.v0,t.v1,t.v2);
        Native3DSWriter::Triangle triangle{};
        for(size_t i=0;i<3;++i)triangle[i]={corners[i].x,corners[i].y,corners[i].z};
        triangles.push_back(triangle);
    }
    error.clear();return true;
}
}
