#include "CollisionPreview.h"
#include "MapDocument.h"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {
bool Close(float a,float b) {return std::fabs(a-b)<0.01f;}
bool Check(bool okay,const char* description) {
    if(!okay) std::cerr<<"FAIL collision diagnostic: "<<description<<'\n';
    return okay;
}
}
int main() {
    const auto file=std::filesystem::temp_directory_path()/"pt-editor-collision-diagnostic.xml";
    // Explicit authored collision: visual props do not determine the diagnostics.
    // An invalid plane is omitted, not emitted as non-finite GPU vertices.
    {std::ofstream out(file,std::ios::binary|std::ios::trunc);
     out<<R"XML(<map version="1.0.Light"><static-geometry>
<prop library-name="Deco" name="DoesNotAffectCollision"><position><x>9000</x></position></prop>
</static-geometry><collision-geometry>
<collision-plane><position><x>100</x><y>200</y><z>300</z></position>
 <width>200</width><length>100</length></collision-plane>
<collision-plane><width>-1</width><length>10</length></collision-plane>
<collision-box><position><x>50</x></position><size><x>20</x><y>40</y><z>60</z></size></collision-box>
<collision-triangle><position><x>25</x></position>
<v0><x>0</x><y>0</y><z>0</z></v0>
<v1><x>100</x><y>0</y><z>0</z></v1>
<v2><x>0</x><y>100</y><z>0</z></v2></collision-triangle>
</collision-geometry></map>)XML";}
    MapDocument map;std::string error;
    if(!Check(map.Load(file,error),"load legacy XML"))return 1;
    if(!Check(!map.Dirty(),"initial document is unchanged"))return 2;
    const auto preview=CollisionPreview::Build(map);
    if(!Check(preview.planes==1 && preview.boxes==1 && preview.triangles==1 && preview.omitted==1,
              "count validated primitives and omission"))return 3;
    if(!Check(preview.faces.size()==45 && preview.edges.size()==38,
              "actual faces and outlines: 1 plane, 1 box and 1 triangle"))return 4;
    if(!Check(Close(preview.faces[0].position.y,300) && Close(preview.faces[0].position.z,-150),
              "plane uses legacy-local Z-up to internal Y-up conversion"))return 5;
    if(!Check(Close(preview.faces[42].position.x,25),"triangle legacy-local vertices honor original transform"))return 6;
    if(!Check(preview.faces[0].color.y>preview.faces[0].color.x,
              "horizontal plane colored green by orientation"))return 7;
    if(!Check(preview.faces[6].color.x>preview.faces[6].color.y,
              "box colored red"))return 8;
    for(const auto& vertex:preview.faces)
        if(!Check(CollisionPreview::Finite(vertex.position),"all GPU vertices finite"))return 9;
    if(!Check(!map.Dirty() && map.Props().size()==1,
              "render preview cannot change props, original XML, or dirty state"))return 10;
    std::cout<<"PASS: native XML collision diagnostic vertices, colors, counts, bounds and read-only rendering.\n";
    return 0;
}
