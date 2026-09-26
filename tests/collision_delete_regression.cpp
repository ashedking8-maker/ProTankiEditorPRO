#include "MapDocument.h"
#include "EditHistory.h"
#include <pugixml.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cmath>

static size_t Count(pugi::xml_node node,const char* tag) {
    size_t value=0;for(auto n:node.children(tag)){(void)n;++value;}return value;
}
static bool Equal(float lhs,float rhs){return std::fabs(lhs-rhs)<0.01f;}
int main(){
    const auto root=std::filesystem::temp_directory_path()/"pt-editor-collision-regression";
    std::error_code ec;std::filesystem::create_directories(root,ec);
    const auto src=root/"input.xml", moved=root/"moved.xml", deleted=root/"deleted.xml", restored=root/"restored.xml";
    // Extra custom sections and the triangle's zero origin must survive unmodified.
    const char* xml=R"XML(<map version="1.0.Light"><static-geometry>
<prop library-name="Tiles" group-name="default" name="NoMatchedPhysics"><rotation><z>0</z></rotation><position><x>10000</x><y>0</y><z>0</z></position></prop>
<prop library-name="Tiles" group-name="default" name="MatchedFloor"><rotation><z>0</z></rotation><position><x>500</x><y>0</y><z>0</z></position></prop>
<prop library-name="Tiles" group-name="default" name="StillPresent"><rotation><z>0</z></rotation><position><x>9000</x><y>0</y><z>0</z></position></prop>
</static-geometry><collision-geometry>
<collision-plane id="floor"><position><x>500</x><y>0</y><z>0</z></position><rotation><z>0</z></rotation><width>500</width><length>500</length></collision-plane>
<collision-plane id="unrelated"><position><x>7000</x><y>0</y><z>0</z></position><width>500</width><length>500</length></collision-plane>
<collision-box id="box"><position><x>500</x><y>0</y><z>0</z></position><rotation><z>0</z></rotation><size><x>500</x><y>500</y><z>300</z></size></collision-box>
<collision-triangle id="separate"><position><x>0</x></position><v0><x>10</x></v0><v1><x>11</x></v1><v2><x>12</x></v2></collision-triangle>
</collision-geometry><way-points><way-point custom="keep"/></way-points></map>)XML";
    {std::ofstream out(src,std::ios::binary);out<<xml;}
    MapDocument map;std::string error;
    if(!map.Load(src,error)){std::cerr<<error;return 1;}
    if(map.Props().size()!=3||map.CollisionPlanes().size()!=2||map.CollisionBoxes().size()!=1||map.CollisionTriangles().size()!=1)return 2;
    std::string reason;
    if(!map.DeletePropWithCollision(0,reason)||map.Props().size()!=2||map.CollisionPlanes().size()!=2)return 3; // decorative prop without an exact-center collider can be deleted
    if(!map.SetPropTransform(0,{600,0,0},{0,0,0.5f}))return 4;
    if(!Equal(map.CollisionPlanes()[0].position.x,600)||!Equal(map.CollisionBoxes()[0].position.x,600)||!Equal(map.CollisionPlanes()[1].position.x,7000))return 5;
    if(!map.SaveLegacyAs(moved,error)){std::cerr<<error;return 6;}
    MapDocument loaded;
    if(!loaded.Load(moved,error)){std::cerr<<error;return 7;}
    if(!Equal(loaded.CollisionPlanes()[0].position.x,600)||!Equal(loaded.CollisionPlanes()[0].rotation.z,0.5f)||!Equal(loaded.CollisionBoxes()[0].position.x,600))return 8;
    auto snapshot=loaded;
    if(!loaded.DeletePropWithCollision(0,reason)||loaded.Props().size()!=1||loaded.CollisionPlanes().size()!=1||!loaded.CollisionBoxes().empty()||loaded.CollisionTriangles().size()!=1)return 9;
    EditHistory history;history.PushSnapshot(std::move(snapshot),loaded);
    if(!loaded.SaveLegacyAs(deleted,error)){std::cerr<<error;return 10;}
    pugi::xml_document doc;if(!doc.load_file(deleted.c_str()))return 11;
    auto rootNode=doc.child("map"),collision=rootNode.child("collision-geometry");
    if(Count(rootNode.child("static-geometry"),"prop")!=1||Count(collision,"collision-plane")!=1||Count(collision,"collision-box")!=0||Count(collision,"collision-triangle")!=1)return 12;
    if(std::string(collision.child("collision-plane").attribute("id").as_string())!="unrelated"||
       !rootNode.child("way-points").child("way-point").attribute("custom"))return 13;
    std::vector<size_t> changed;if(!history.Undo(loaded,changed)||loaded.Props().size()!=2||loaded.CollisionPlanes().size()!=2||loaded.CollisionBoxes().size()!=1)return 14;
    if(!loaded.SaveLegacyAs(restored,error)){std::cerr<<error;return 15;}
    MapDocument reread;
    if(!reread.Load(restored,error)||reread.CollisionPlanes().size()!=2||!Equal(reread.CollisionPlanes()[0].position.x,600))return 16;
    // A co-located decoration may be deleted, retaining the other object's collider.
    const auto shared=root/"shared.xml", sharedOut=root/"shared-out.xml";
    {std::ofstream out(shared,std::ios::binary);out<<R"XML(<map version="1.0.Light"><static-geometry>
<prop library-name="Tiles" name="A"><position><x>5</x></position></prop>
<prop library-name="Tiles" name="B"><position><x>5</x></position></prop>
</static-geometry><collision-geometry><collision-box id="shared"><position><x>5</x></position></collision-box></collision-geometry>
<spawn-points/><ctf-flags/><dom-keypoints/><bonus-regions/><special-geometry/><way-points/></map>)XML";}
    MapDocument ambiguous;
    if(!ambiguous.Load(shared,error))return 17;
    if(!ambiguous.DeletePropWithCollision(0,reason)||ambiguous.Props().size()!=1||ambiguous.CollisionBoxes().size()!=1)return 18;
    if(reason.find("Preserved shared collision")==std::string::npos)return 19;
    // Deleting the last co-located object then removes the shared origin collider.
    if(!ambiguous.DeletePropsWithCollision({0,0},reason)||!ambiguous.Props().empty()||!ambiguous.CollisionBoxes().empty())return 20;
    if(!ambiguous.SaveLegacyAs(sharedOut,error))return 21;
    pugi::xml_document sharedXml;
    if(!sharedXml.load_file(sharedOut.c_str())||Count(sharedXml.child("map").child("static-geometry"),"prop")||
       Count(sharedXml.child("map").child("collision-geometry"),"collision-box"))return 22;
    // Full-delete is explicit and must remove triangle collision as well.
    MapDocument clearAll;
    if(!clearAll.Load(src,error)||!clearAll.DeletePropsWithCollision({0,1,2},reason))return 23;
    if(!clearAll.Props().empty()||!clearAll.CollisionPlanes().empty()||
       !clearAll.CollisionBoxes().empty()||!clearAll.CollisionTriangles().empty())return 24;
    const auto emptyOut=root/"empty.xml";
    if(!clearAll.SaveLegacyAs(emptyOut,error))return 25;
    pugi::xml_document cleared;
    if(!cleared.load_file(emptyOut.c_str())||Count(cleared.child("map").child("collision-geometry"),"collision-triangle")||
       !cleared.child("map").child("way-points").child("way-point"))return 26;
    std::cout<<"PASS: decorative deletion, shared-origin preservation/final deletion, full static collision clearing, XML roundtrip and undo.\n";
    return 0;
}
