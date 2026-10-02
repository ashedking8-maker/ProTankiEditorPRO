#pragma once
// CPU-only support-edge snapping of rendered convex footprints. Legacy XY ground plane.
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>
namespace PlacementSnap {
inline float Tolerance(float cell,bool total){return total?std::clamp(cell*.55f,2.f,275.f):std::clamp(cell*.55f,.01f,10.f);}
struct Point { double x{},y{}; bool operator==(const Point&)const=default; };
struct Shape { std::vector<Point> hull; double minZ{},maxZ{}; };
struct Match { bool matched{}; double dx{},dy{}; Point guideA{},guideB{}; };
inline void Consider(Match& best,const Match& candidate){
    if(candidate.matched && (!best.matched || std::hypot(candidate.dx,candidate.dy)<std::hypot(best.dx,best.dy)-1e-6))best=candidate;
}
inline double Dot(Point a,Point b){return a.x*b.x+a.y*b.y;}
inline double Cross(Point a,Point b,Point c){return (b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x);}
inline std::vector<Point> Hull(std::vector<Point> p){
    p.erase(std::remove_if(p.begin(),p.end(),[](Point v){return !std::isfinite(v.x)||!std::isfinite(v.y);}),p.end());
    std::sort(p.begin(),p.end(),[](Point a,Point b){return a.x!=b.x?a.x<b.x:a.y<b.y;});
    p.erase(std::unique(p.begin(),p.end()),p.end());if(p.size()<3)return p;
    std::vector<Point> h(2*p.size());size_t k=0;
    for(auto v:p){while(k>=2&&Cross(h[k-2],h[k-1],v)<=0)--k;h[k++]=v;}
    const size_t lower=k+1;
    for(size_t i=p.size()-1;i-->0;){auto v=p[i];while(k>=lower&&Cross(h[k-2],h[k-1],v)<=0)--k;h[k++]=v;}
    h.resize(k-1);return h;
}
inline Shape Translate(Shape s,double x,double y,double z){for(auto& p:s.hull){p.x+=x;p.y+=y;}s.minZ+=z;s.maxZ+=z;return s;}
inline void Project(const Shape& s,Point axis,double& lo,double& hi){lo=std::numeric_limits<double>::infinity();hi=-lo;for(auto p:s.hull){const auto d=Dot(p,axis);lo=std::min(lo,d);hi=std::max(hi,d);}}
inline Match Find(const Shape& moving,const Shape& fixed,double tolerance,double clearance=.02){
    Match result;
    if(moving.hull.size()<3||fixed.hull.size()<3||!std::isfinite(tolerance)||tolerance<=0||!std::isfinite(clearance)||clearance<0)return result;
    // Allow zero-thickness floors at the same elevation, not separated storeys.
    if(moving.maxZ<fixed.minZ-.01||fixed.maxZ<moving.minZ-.01)return result;
    double best=tolerance;
    for(size_t i=0;i<fixed.hull.size();++i){
        const auto a=fixed.hull[i],b=fixed.hull[(i+1)%fixed.hull.size()];
        const double len=std::hypot(b.x-a.x,b.y-a.y);if(len<1e-8)continue;
        const Point tangent{(b.x-a.x)/len,(b.y-a.y)/len},normal{tangent.y,-tangent.x}; // CCW outward
        double ml,mh;Project(moving,normal,ml,mh);
        const double delta=Dot(a,normal)+clearance-ml;
        if(std::fabs(delta)>best)continue;
        // Only the contacting support vertices can touch this finite edge.
        // Using the whole hull's tangent interval snaps to infinite edge extensions.
        double tl=std::numeric_limits<double>::infinity(),th=-tl;
        for(auto p:moving.hull)if(std::fabs(Dot(p,normal)-ml)<1e-5){const double t=Dot(p,tangent);tl=std::min(tl,t);th=std::max(th,t);}
        const double edgeA=Dot(a,tangent),edgeB=Dot(b,tangent);
        if(std::min(th,std::max(edgeA,edgeB))-std::max(tl,std::min(edgeA,edgeB))< -1e-5)continue;
        // All moving vertices end outside this supporting plane: no overlapping interiors.
        best=std::fabs(delta);result={true,normal.x*delta,normal.y*delta,a,b};
    }
    return result;
}
// Additional DISCRETE nodes are generated from the regular lattice position.
// The raw cursor ranks nodes only; it never supplies their tangential position.
inline Match FindDiscrete(const Shape& moving,const Shape& fixed,double reach,double clearance,
                          Point cursorDelta,double& bestDistanceSquared) {
    Match result;
    if(moving.hull.size()<3||fixed.hull.size()<3||reach<=0||!std::isfinite(reach))return result;
    if(moving.maxZ<fixed.minZ-.01||fixed.maxZ<moving.minZ-.01)return result;
    for(size_t i=0;i<fixed.hull.size();++i){
        const auto a=fixed.hull[i],b=fixed.hull[(i+1)%fixed.hull.size()];
        const double len=std::hypot(b.x-a.x,b.y-a.y);if(len<1e-8)continue;
        const Point tangent{(b.x-a.x)/len,(b.y-a.y)/len},normal{tangent.y,-tangent.x};
        double lo,hi;Project(moving,normal,lo,hi);
        const double delta=Dot(a,normal)+clearance-lo;if(std::fabs(delta)>reach)continue;
        double tl=std::numeric_limits<double>::infinity(),th=-tl;
        for(auto p:moving.hull)if(std::fabs(Dot(p,normal)-lo)<1e-5){const double t=Dot(p,tangent);tl=std::min(tl,t);th=std::max(th,t);}
        if(std::min(th,Dot(b,tangent))-std::max(tl,Dot(a,tangent))< -1e-5)continue;
        const double dx=normal.x*delta,dy=normal.y*delta;
        const double distance=(dx-cursorDelta.x)*(dx-cursorDelta.x)+(dy-cursorDelta.y)*(dy-cursorDelta.y);
        if(distance+1e-6>=bestDistanceSquared)continue; // regular grid wins ties
        bestDistanceSquared=distance;result={true,dx,dy,a,b};
    }
    return result;
}
inline Shape Merge(const std::vector<Shape>& shapes){
    Shape result;result.minZ=std::numeric_limits<double>::infinity();result.maxZ=-result.minZ;
    for(const auto& s:shapes){result.hull.insert(result.hull.end(),s.hull.begin(),s.hull.end());result.minZ=std::min(result.minZ,s.minZ);result.maxZ=std::max(result.maxZ,s.maxZ);}
    result.hull=Hull(std::move(result.hull));return result;
}
inline float Coordinate(float value,float step,bool enabled,float anchor=0.f){return enabled&&std::isfinite(step)&&step>.0001f?anchor+std::round((value-anchor)/step)*step:value;}
inline float Height(float current,float direction,float step,bool snap,float anchor=0.f){return direction==0?current:Coordinate(current+direction*step,step,snap,anchor);}
}
