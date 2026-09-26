#include "VolumePicking.h"
#include <cassert>
#include <iostream>
int main() {
    using namespace VolumePicking;
    std::array<Point,8> v{{{0,0},{100,0},{100,100},{0,100},{20,20},{80,20},{80,80},{20,80}}};
    std::array<bool,8> yes{{true,true,true,true,true,true,true,true}};
    assert(Score({50,50},v,yes)==0.f); // center is clickable
    assert(Score({1,50},v,yes)==0.f); // face interior, not a 1px line
    assert(Score({105,50},v,yes)==25.f); // generous edge hit testing
    assert(Score({250,250},v,yes)>10000.f);
    yes.fill(false);assert(!std::isfinite(Score({50,50},v,yes)));
    std::cout<<"Volume picking: interior, edges and hidden projection PASS\n";
}
