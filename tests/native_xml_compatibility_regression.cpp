#include "MapDocument.h"
#include "NativeCollisionImport.h"
#include "ReleaseTestCheck.h"
#include <pugixml.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
static bool Eq(float a,float b){return std::fabs(a-b)<.001f;}
int main(){
    const auto temp=std::filesystem::temp_directory_path()/
        ("ptpro-xml-compat-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(temp);
    struct Cleanup{std::filesystem::path p;~Cleanup(){std::error_code ec;std::filesystem::remove_all(p,ec);}}cleanup{temp};
    NativeCollisionImport::Result source;
    VerifiedCollisionTemplates::Plane p;p.offset={20,30,40};p.rotation={.3f,.2f,.1f};p.width=50;p.length=60;source.planes.push_back(p);
    NativeCollisionImport::Result::Box b;b.offset={90,80,70};b.rotation={-.4f,.5f,-.6f};b.size={10,20,30};source.boxes.push_back(b);
    VerifiedCollisionTemplates::Triangle t;t.offset={-10,-20,-30};t.rotation={.7f,-.8f,.9f};t.v0={-4,-3,0};t.v1={4,-3,0};t.v2={0,6,0};source.triangles.push_back(t);
    MapDocument map;map.CreateBlank();std::string error;
    for(int i=0;i<3;++i){PropInstance prop;prop.library="Test";prop.group="g";prop.name="n";prop.position.x=1000.f*i;
        PT_REQUIRE(map.AddProp(prop)==static_cast<size_t>(i));PT_REQUIRE(map.AddImportedCollisionForProp(i,source));}
    PT_REQUIRE(map.SaveLegacyAs(temp/"map.xml",error));
    pugi::xml_document xml;PT_REQUIRE(xml.load_file((temp/"map.xml").c_str()));
    auto collision=xml.child("map").child("collision-geometry");
    for(const auto* name:{"collision-plane","collision-box","collision-triangle"}){
        auto rotation=collision.child(name).child("rotation");
        PT_REQUIRE(rotation.child("x")&&rotation.child("y")&&rotation.child("z"));
    }
    PT_REQUIRE(Eq(collision.child("collision-plane").child("rotation").child("x").text().as_float(),.3f));
    PT_REQUIRE(Eq(collision.child("collision-box").child("rotation").child("y").text().as_float(),.5f));
    PT_REQUIRE(Eq(collision.child("collision-triangle").child("rotation").child("x").text().as_float(),.7f));
    MapDocument loaded;PT_REQUIRE(loaded.Load(temp/"map.xml",error));
    for(size_t i=0;i<3;++i)PT_REQUIRE(loaded.BindImportedCollisionForProp(i,source));
    std::vector<CollisionPlane> planes;std::vector<CollisionBox> boxes;std::vector<CollisionTriangle> triangles;
    PT_REQUIRE(!loaded.CopyNativeCollisionForProps({2,1},planes,boxes,triangles));
    PT_REQUIRE(loaded.CopyNativeCollisionForProps({1,2},planes,boxes,triangles));
    PT_REQUIRE(planes.size()==2&&boxes.size()==2&&triangles.size()==2);
    PT_REQUIRE(planes[0].authoredOwnerIndex==0&&planes[1].authoredOwnerIndex==1);
    PT_REQUIRE(planes[0].originalXml==loaded.CollisionPlanes()[1].originalXml);
    PT_REQUIRE(Eq(triangles[0].rotation.y,-.8f));
    std::vector<PropInstance> props{loaded.Props()[1],loaded.Props()[2]};
    for(auto& prop:props)prop.position.y+=500;
    for(auto& c:planes)c.position.y+=500;for(auto& c:boxes)c.position.y+=500;for(auto& c:triangles)c.position.y+=500;
    std::vector<int> inserted;
    PT_REQUIRE(loaded.AppendLosslessNativeStaticClone(props,planes,boxes,triangles,inserted,error));
    PT_REQUIRE(inserted.size()==2&&inserted[0]==3&&inserted[1]==4);
    PT_REQUIRE(loaded.DeletePropWithCollision(3,error)); // owned subset can be edited individually
    PT_REQUIRE(loaded.Props().size()==4&&loaded.CollisionPlanes().size()==4&&loaded.CollisionBoxes().size()==4&&loaded.CollisionTriangles().size()==4);
    PT_REQUIRE(loaded.HasNativeCollisionForProp(3));
    auto pos=loaded.Props()[3].position;pos.x+=100;
    PT_REQUIRE(loaded.SetPropTransform(3,pos,loaded.Props()[3].rotation));
    loaded.SetCollisionOwnershipUnresolved(3,true);pos.x+=100;
    PT_REQUIRE(!loaded.SetPropTransform(3,pos,loaded.Props()[3].rotation));
    PT_REQUIRE(!loaded.DeletePropWithCollision(3,error));
    PT_REQUIRE(!loaded.CopyNativeCollisionForProps({3},planes,boxes,triangles));
    PT_REQUIRE(loaded.CopyNativeCollisionForProps({0,1,2,3},planes,boxes,triangles)); // full-map preservation remains available
    loaded.SetCollisionOwnershipUnresolved(3,false);
    PT_REQUIRE(loaded.SaveLegacyAs(temp/"copy.xml",error));
    MapDocument back;PT_REQUIRE(back.Load(temp/"copy.xml",error));
    PT_REQUIRE(back.CollisionTriangles().size()==4&&Eq(back.CollisionTriangles()[3].rotation.x,.7f));
    {std::ofstream f(temp/"v3.xml");f<<"<map version=\"3.0\"><static-geometry/></map>";}
    PT_REQUIRE(!back.Load(temp/"v3.xml",error)&&error.find("Unsupported map version")!=std::string::npos);
    MapDocument large;large.CreateBlank();
    std::vector<PropInstance> many(2000);std::vector<CollisionBox> solids(2000);
    for(size_t i=0;i<many.size();++i){many[i].library="UnavailableLibrary";many[i].group="g";many[i].name="MissingMesh";many[i].position.x=100.f*static_cast<float>(i);
        solids[i].position=many[i].position;solids[i].size={20,20,20};solids[i].authoredOwnerIndex=static_cast<int>(i);}
    std::vector<int> added;
    PT_REQUIRE(large.AppendLosslessNativeStaticClone(many,{},solids,{},added,error));
    std::vector<size_t> all;for(size_t i=0;i<many.size();++i)all.push_back(i);
    PT_REQUIRE(large.CopyNativeCollisionForProps(all,planes,boxes,triangles)&&boxes.size()==2000);
    MapDocument destination;destination.CreateBlank();
    PT_REQUIRE(destination.AppendLosslessNativeStaticClone(large.Props(),planes,boxes,triangles,added,error));
    PT_REQUIRE(destination.Props().size()==2000&&destination.CollisionBoxes().size()==2000);
    PT_REQUIRE(destination.SaveLegacyAs(temp/"large.xml",error));
    MapDocument largeRead;PT_REQUIRE(largeRead.Load(temp/"large.xml",error));
    PT_REQUIRE(largeRead.Props().size()==2000&&largeRead.CollisionBoxes().size()==2000);
    std::cout<<"Native XML: tilted plane/box/triangle rotations, rebind, owned subset copy/remap/delete/move, unresolved guards and version rejection PASS\n";
}
