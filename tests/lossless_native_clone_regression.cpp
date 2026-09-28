#include "MapDocument.h"
#include <pugixml.hpp>
#include <filesystem>
#include <cmath>
#include <fstream>
#include <iostream>
#include <set>
#include <string>

#define CHECK(x) do{if(!(x)){std::cerr<<"Lossless native clone regression failed line "<<__LINE__<<": "<<#x<<'\n';return __LINE__;}}while(false)

int main(int argc,char** argv) {
    CHECK(argc==2);
    const auto fixture=std::filesystem::path(argv[1]);
    MapDocument map;
    std::string err;
    CHECK(map.Load(fixture/"source.xml",err));
    CHECK(map.Props().size()==2);
    CHECK(map.CollisionPlanes().size()==1 && map.CollisionBoxes().size()==1 && map.CollisionTriangles().size()==1);

    auto props=map.Props();
    auto planes=map.CollisionPlanes();
    auto boxes=map.CollisionBoxes();
    auto triangles=map.CollisionTriangles();
    constexpr float dx=1000.f,dy=-500.f,dz=75.f;
    for(auto& p:props){p.position.x+=dx;p.position.y+=dy;p.position.z+=dz;}
    for(auto& c:planes){c.position.x+=dx;c.position.y+=dy;c.position.z+=dz;}
    for(auto& c:boxes){c.position.x+=dx;c.position.y+=dy;c.position.z+=dz;}
    for(auto& c:triangles){c.position.x+=dx;c.position.y+=dy;c.position.z+=dz;}

    std::vector<int> inserted;
    CHECK(map.AppendLosslessNativeStaticClone(std::move(props),std::move(planes),std::move(boxes),std::move(triangles),inserted,err));
    CHECK(inserted.size()==2);
    CHECK(map.Props().size()==4);
    CHECK(map.CollisionPlanes().size()==2 && map.CollisionBoxes().size()==2 && map.CollisionTriangles().size()==2);

    const auto out=std::filesystem::temp_directory_path()/"ptpro-lossless-native-clone.xml";
    CHECK(map.SaveLegacyAs(out,err));

    pugi::xml_document doc;
    CHECK(doc.load_file(out.c_str(),pugi::parse_full));
    const auto root=doc.child("map");
    auto geom=root.child("static-geometry");
    auto p0=geom.child("prop");
    auto p1=p0.next_sibling("prop");
    auto p2=p1.next_sibling("prop");
    auto p3=p2.next_sibling("prop");
    CHECK(p0&&p1&&p2&&p3&&!p3.next_sibling("prop"));
    CHECK(std::string(p2.attribute("opaque-attr").value())=="keep-me");
    CHECK(std::string(p2.child("future-extension").attribute("mode").value())=="legacy");
    CHECK(std::string(p2.child("future-extension").child("nested").text().as_string())=="opaque-data");
    CHECK(std::string(p2.child("with_collision").text().as_string())=="maybe");
    CHECK(std::abs(p2.child("position").child("x").text().as_float()-1100.f)<.01f);
    CHECK(std::abs(p2.child("position").child("y").text().as_float()+300.f)<.01f);
    CHECK(std::abs(p2.child("position").child("z").text().as_float()-375.f)<.01f);

    auto collision=root.child("collision-geometry");
    CHECK(std::string(collision.attribute("opaque-section-attr").value())=="section-stays");
    auto plane0=collision.child("collision-plane");
    auto plane1=plane0.next_sibling("collision-plane");
    CHECK(plane0&&plane1&&!plane1.next_sibling("collision-plane"));
    CHECK(std::string(plane1.attribute("opaque-plane").value())=="preserve");
    CHECK(std::string(plane1.child("future-plane-data").attribute("answer").value())=="42");
    CHECK(std::abs(plane1.child("position").child("x").text().as_float()-1100.f)<.01f);
    CHECK(std::abs(plane1.child("rotation").child("x").text().as_float()-0.1f)<.001f);
    CHECK(std::abs(plane1.child("rotation").child("y").text().as_float()-0.2f)<.001f);

    auto box0=collision.child("collision-box");
    auto box1=box0.next_sibling("collision-box");
    CHECK(box0&&box1&&!box1.next_sibling("collision-box"));
    CHECK(std::string(box1.attribute("opaque-box").value())=="preserve");
    CHECK(std::string(box1.child("future-box-data").text().as_string())=="box-opaque");

    auto tri0=collision.child("collision-triangle");
    auto tri1=tri0.next_sibling("collision-triangle");
    CHECK(tri0&&tri1&&!tri1.next_sibling("collision-triangle"));
    CHECK(std::string(tri1.attribute("opaque-triangle").value())=="preserve");
    CHECK(std::string(tri1.child("future-triangle-data").attribute("flag").value())=="yes");

    std::set<std::string> ids;
    for(auto n:collision.children())if(n.type()==pugi::node_element && std::string(n.name()).rfind("collision-",0)==0) {
        const std::string id=n.attribute("id").value();CHECK(!id.empty());CHECK(ids.insert(id).second);
    }
    CHECK(ids.size()==6);

    MapDocument reopened;
    CHECK(reopened.Load(out,err));
    CHECK(reopened.Props().size()==4);
    CHECK(reopened.CollisionPlanes().size()==2 && reopened.CollisionBoxes().size()==2 && reopened.CollisionTriangles().size()==2);
    std::error_code ec;std::filesystem::remove(out,ec);
    std::cout<<"Lossless full-static clone preserved opaque prop/collider XML, transformed positions, and unique IDs\n";
    return 0;
}
