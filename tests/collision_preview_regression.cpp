#include "CollisionPreview.h"
#include "MapDocument.h"
#include "NativeCollisionImport.h"
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
    // Plane origin (100,200,300), first local corner (-100,-50,0):
    // legacy (0,150,300) -> internal (0,300,150), without negating legacy Y.
    if(!Check(Close(preview.faces[0].position.x,0) &&
              Close(preview.faces[0].position.y,300) &&
              Close(preview.faces[0].position.z,150),
              "plane preserves legacy Y sign in the Z-up to Y-up conversion"))return 5;
    if(!Check(Close(preview.faces[42].position.x,25),"triangle legacy-local vertices honor original transform"))return 6;
    if(!Check(preview.faces[0].color.y>preview.faces[0].color.x,
              "horizontal plane colored green by orientation"))return 7;
    if(!Check(preview.faces[6].color.x>preview.faces[6].color.y,
              "box colored red"))return 8;
    for(const auto& vertex:preview.faces)
        if(!Check(CollisionPreview::Finite(vertex.position),"all GPU vertices finite"))return 9;
    if(!Check(!map.Dirty() && map.Props().size()==1,
              "render preview cannot change props, original XML, or dirty state"))return 10;

    // An exact-origin legacy plane must follow its prop in the document.
    // Geometry View rebuilds its GPU buffers from this newly computed shape.
    PropInstance floor;floor.library="Test";floor.name="Floor";
    floor.position={100.f,200.f,300.f};
    const auto floorIndex=map.AddProp(floor);
    if(!Check(map.SetPropTransform(floorIndex,{400.f,200.f,300.f},floor.rotation),
              "legacy floor transform accepted"))return 11;
    const auto moved=CollisionPreview::Build(map);
    if(!Check(Close(map.CollisionPlanes()[0].position.x,400.f)&&
              Close(moved.faces[0].position.x,300.f)&&
              Close(moved.faces[0].position.x-preview.faces[0].position.x,300.f),
              "live collision mesh comes from updated XML after prop movement"))return 12;
    // A triangle whose ownership is unknown must never be hallucinated as
    // moving with this unrelated visual prop.
    if(!Check(Close(moved.faces[42].position.x,preview.faces[42].position.x),
              "unbound native triangle retains its own XML location"))return 13;

    // Explicitly owned triangle terrain must move with the corresponding prop.
    MapDocument terrain;terrain.CreateBlank();
    PropInstance hill;hill.library="Land Hills";hill.group="default";
    hill.name="Land04";hill.position={1000.f,2000.f,100.f};
    const auto hillIndex=terrain.AddProp(hill);
    NativeCollisionImport::Result native;
    VerifiedCollisionTemplates::Triangle tri;
    tri.v0={-200.f,-200.f,0.f};tri.v1={200.f,-200.f,0.f};tri.v2={0.f,200.f,0.f};
    native.triangles.push_back(tri);
    if(!Check(terrain.AddImportedCollisionForProp(hillIndex,native),
              "owned terrain source triangle installed"))return 14;
    const auto before=CollisionPreview::Build(terrain);
    if(!Check(terrain.SetPropTransform(hillIndex,{1600.f,2000.f,100.f},hill.rotation),
              "owned terrain transform accepted"))return 15;
    const auto after=CollisionPreview::Build(terrain);
    if(!Check(after.triangles==1&&Close(after.faces[0].position.x-before.faces[0].position.x,600.f),
              "owned native triangle follows moved hill in live geometry view"))return 16;
    std::cout<<"PASS: native XML collision diagnostic and live moved plane / owned triangle geometry.\n";
    return 0;
}
