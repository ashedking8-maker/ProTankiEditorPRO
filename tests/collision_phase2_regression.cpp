#include "MapDocument.h"
#include "EditHistory.h"
#include <pugixml.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {
size_t Count(pugi::xml_node n, const char* tag) {
    size_t value=0;for(auto x:n.children(tag)){(void)x;++value;}return value;
}
bool Assert(bool ok,const char* what) {
    if(!ok) std::cerr << "FAIL collision phase 2: " << what << '\n';
    return ok;
}
}

int main() {
    const auto dir=std::filesystem::temp_directory_path()/"pt-editor-collision-phase2";
    std::error_code ec;std::filesystem::create_directories(dir,ec);
    const auto input=dir/"source.xml", first=dir/"first.xml", second=dir/"second.xml",
               combined=dir/"combined.xml", cleared=dir/"cleared.xml";
    const char* xml=R"XML(<map version="1.0.Light"><static-geometry>
<prop library-name="Tiles" group-name="default" name="A"><position><x>1000</x><y>0</y><z>0</z></position></prop>
<prop library-name="Decoration" group-name="default" name="B"><position><x>1000</x><y>0</y><z>0</z></position></prop>
<prop library-name="Walls" group-name="default" name="C"><position><x>5000</x><y>0</y><z>0</z></position></prop>
</static-geometry><collision-geometry special="preserve">
<collision-triangle id="triangle"><position><x>0</x><y>0</y><z>0</z></position><v0><x>30</x></v0></collision-triangle>
<vendor-extension id="untouched"/>
<collision-plane id="shared"><position><x>1000</x><y>0</y><z>0</z></position><width>500</width><length>500</length></collision-plane>
<collision-box id="unique"><position><x>5000</x><y>0</y><z>0</z></position><size><x>500</x><y>500</y><z>300</z></size></collision-box>
<collision-plane id="unrelated"><position><x>9000</x><y>0</y><z>0</z></position><width>500</width><length>500</length></collision-plane>
</collision-geometry><way-points><way-point custom="kept"/></way-points></map>)XML";
    {std::ofstream out(input,std::ios::binary);out<<xml;}
    MapDocument map;std::string error,reason;
    if(!Assert(map.Load(input,error),"load"))return 1;
    if(!Assert(!map.DeletePropsWithCollision({0,99},reason) && map.Props().size()==3 && map.CollisionPlanes().size()==2,"invalid bulk selection is atomic"))return 2;
    MapDocument before=map;
    if(!Assert(map.DeletePropWithCollision(0,reason),"delete a shared-origin prop"))return 3;
    if(!Assert(map.Props().size()==2 && map.CollisionPlanes().size()==2 &&
               reason.find("Preserved shared collision")!=std::string::npos,"remaining prop keeps its shared physics"))return 4;
    EditHistory history;history.PushSnapshot(std::move(before),map);
    if(!Assert(map.SaveLegacyAs(first,error),"save after deleting shared origin")) {std::cerr<<error;return 5;}
    pugi::xml_document doc;
    if(!Assert(static_cast<bool>(doc.load_file(first.c_str())),"read exported XML"))return 6;
    auto collision=doc.child("map").child("collision-geometry");
    if(!Assert(std::string(collision.attribute("special").as_string())=="preserve" &&
               Count(collision,"collision-triangle")==1 && Count(collision,"vendor-extension")==1 &&
               Count(collision,"collision-plane")==2 && Count(collision,"collision-box")==1,"unknown collision nodes retained"))return 7;
    // Deleting the last prop at the shared origin can now safely remove its exact-origin plane.
    if(!Assert(map.DeletePropWithCollision(0,reason) && map.Props().size()==1 && map.CollisionPlanes().size()==1,
               "delete last prop at once-shared origin after save without reload"))return 8;
    if(!Assert(map.SaveLegacyAs(second,error),"rebase collision source indices on second save")){std::cerr<<error;return 9;}
    if(!Assert(static_cast<bool>(doc.load_file(second.c_str())),"read second export"))return 10;
    collision=doc.child("map").child("collision-geometry");
    if(!Assert(std::string(collision.child("collision-plane").attribute("id").as_string())=="unrelated" &&
               std::string(collision.child("collision-box").attribute("id").as_string())=="unique",
               "exactly the owned plane was deleted"))return 11;
    const std::vector<std::string> expected={"collision-triangle","vendor-extension","collision-box","collision-plane"};
    std::vector<std::string> actual;
    for(auto node:collision.children())if(node.type()==pugi::node_element)actual.emplace_back(node.name());
    if(!Assert(actual==expected,"original mixed collider and extension order preserved"))return 12;
    if(!Assert(static_cast<bool>(doc.child("map").child("way-points").child("way-point").attribute("custom")),"unknown map sections preserved"))return 13;
    MapDocument grouped;
    if(!Assert(grouped.Load(input,error),"reload original for grouped deletion"))return 14;
    if(!Assert(grouped.DeletePropsWithCollision({1,0,1},reason) && grouped.Props().size()==1 &&
               grouped.CollisionPlanes().size()==1 && grouped.CollisionBoxes().size()==1,"group delete dedup and shared collider removal"))return 15;
    if(!Assert(grouped.SaveLegacyAs(combined,error),"grouped save")){std::cerr<<error;return 16;}
    if(!Assert(static_cast<bool>(doc.load_file(combined.c_str())) &&
               std::string(doc.child("map").child("collision-geometry").child("collision-plane").attribute("id").as_string())=="unrelated",
               "group deletion XML removes shared collision"))return 17;
    if(!Assert(grouped.DeletePropsWithCollision({0},reason) && grouped.Props().empty() &&
               grouped.CollisionPlanes().empty() && grouped.CollisionBoxes().empty() && grouped.CollisionTriangles().empty(),
               "delete all remaining geometry clears all static colliders"))return 18;
    if(!Assert(grouped.SaveLegacyAs(cleared,error),"save emptied map")){std::cerr<<error;return 19;}
    if(!Assert(static_cast<bool>(doc.load_file(cleared.c_str())) &&
               Count(doc.child("map").child("collision-geometry"),"collision-triangle")==0 &&
               Count(doc.child("map").child("collision-geometry"),"vendor-extension")==1,
               "full static clear preserves unknown collision XML"))return 20;
    std::vector<size_t> changes;
    if(!Assert(history.Undo(map,changes) && map.Props().size()==3 && map.CollisionPlanes().size()==2,
               "undo restores shared props and collisions"))return 21;
    std::cout<<"PASS: shared-origin edits, grouped deletion, second save source rebase, mixed collision XML order, unknown elements, full clear and undo.\n";
    return 0;
}
