#pragma once
// CPU-only edge matching in the rendered ground plane (internal X/Z).
// Bounds come from the actual mesh geometry after its prop transform, not
// from a guessed tile size or the object's pivot. Changes only translation.
#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <vector>

namespace GeometrySnap {
struct Rect { float minX{}, maxX{}, minY{}, maxY{}, elevation{}; };
struct Offset { float x{}, y{}; bool xMatched{}, yMatched{}; };
inline Offset Find(const Rect& moving, const std::vector<Rect>& fixed,
                   float tolerance, float clearance=0.f) {
    Offset result;
    if(!(std::isfinite(tolerance) && tolerance>0.f && std::isfinite(clearance) && clearance>=0.f))return result;
    float bestX=tolerance,bestY=tolerance;
    for(const auto& other:fixed) {
        // Do not snap props on different floors or vertically separated tiles.
        if(std::fabs(moving.elevation-other.elevation)>std::max(10.f,tolerance*2.f))continue;
        const float xOverlap=std::min(moving.maxX,other.maxX)-std::max(moving.minX,other.minX);
        const float yOverlap=std::min(moving.maxY,other.maxY)-std::max(moving.minY,other.minY);
        if(yOverlap>std::min(moving.maxY-moving.minY,other.maxY-other.minY)*.1f) {
            for(float dx:{other.maxX+clearance-moving.minX,other.minX-clearance-moving.maxX}) {
                if(std::fabs(dx)<bestX) {bestX=std::fabs(dx);result.x=dx;result.xMatched=true;}
            }
        }
        if(xOverlap>std::min(moving.maxX-moving.minX,other.maxX-other.minX)*.1f) {
            for(float dy:{other.maxY+clearance-moving.minY,other.minY-clearance-moving.maxY}) {
                if(std::fabs(dy)<bestY) {bestY=std::fabs(dy);result.y=dy;result.yMatched=true;}
            }
        }
    }
    return result;
}
} // namespace GeometrySnap
