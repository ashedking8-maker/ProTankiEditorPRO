#include "GeometrySnap.h"
#include <cmath>
#include <iostream>
#include <vector>
using GeometrySnap::Rect;
static bool Eq(float a,float b){return std::fabs(a-b)<.001f;}
int main(){
    const std::vector<Rect> ground{{0,500,0,500,0}};
    // 500x500 tile with a 0.2-unit gap in X and a translated pivot.
    const Rect next{500.2f,1000.2f,0,500,0};
    auto s=GeometrySnap::Find(next,ground,1.f);
    if(!s.xMatched||!Eq(s.x,-.2f)||s.yMatched){std::cerr<<"edge adjacency failed\n";return 1;}
    auto clearance=GeometrySnap::Find(next,ground,1.f,.1f);
    if(!clearance.xMatched||!Eq(clearance.x,-.1f)){std::cerr<<"horizontal clearance failed\n";return 2;}
    if(GeometrySnap::Find(Rect{500.2f,1000.2f,0,500,500},ground,1.f).xMatched){std::cerr<<"different floor snapped\n";return 3;}
    if(GeometrySnap::Find(Rect{500.2f,1000.2f,600,1100,0},ground,1.f).xMatched){std::cerr<<"nonoverlapping edge snapped\n";return 4;}
    if(GeometrySnap::Find(Rect{510,1010,0,500,0},ground,1.f).xMatched){std::cerr<<"far edge snapped\n";return 5;}
    std::cout<<"PASS geometry edge snap, clearance, floor and overlap guards\n";
    return 0;
}
