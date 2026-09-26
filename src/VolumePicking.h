#pragma once
// CPU-only projected volume picking. The selectable area includes faces and
// edges, rather than requiring a click on a one-pixel line or center marker.
#include <array>
#include <algorithm>
#include <cmath>
#include <limits>
namespace VolumePicking {
struct Point {float x{},y{};};
inline float SegmentDistanceSquared(Point p,Point a,Point b) {
    const float dx=b.x-a.x,dy=b.y-a.y,den=dx*dx+dy*dy;
    const float t=den>1e-8f?std::clamp(((p.x-a.x)*dx+(p.y-a.y)*dy)/den,0.f,1.f):0.f;
    const float x=p.x-a.x-t*dx,y=p.y-a.y-t*dy;return x*x+y*y;
}
inline bool InsideQuad(Point p,const std::array<Point,8>& v,const std::array<bool,8>& valid,const std::array<int,4>& face) {
    for(int i:face)if(!valid[static_cast<size_t>(i)])return false;
    bool inside=false;
    for(int i=0,j=3;i<4;j=i++) {
        const auto a=v[static_cast<size_t>(face[i])],b=v[static_cast<size_t>(face[j])];
        if((a.y>p.y)!=(b.y>p.y)) {
            const float intersection=(b.x-a.x)*(p.y-a.y)/(b.y-a.y)+a.x;
            if(p.x<intersection)inside=!inside;
        }
    }
    return inside;
}
inline float Score(Point p,const std::array<Point,8>& v,const std::array<bool,8>& valid) {
    static constexpr std::array<std::array<int,4>,6> faces{{{{0,1,2,3}},{{4,5,6,7}},
        {{0,1,5,4}},{{1,2,6,5}},{{2,3,7,6}},{{3,0,4,7}}}};
    for(const auto& face:faces)if(InsideQuad(p,v,valid,face))return 0.f;
    static constexpr int edges[12][2]={{0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}};
    float best=std::numeric_limits<float>::infinity();
    for(const auto& e:edges)if(valid[e[0]]&&valid[e[1]])
        best=std::min(best,SegmentDistanceSquared(p,v[e[0]],v[e[1]]));
    return best;
}
}
