#include "MapDocument.h"
#include "NativeCollisionImport.h"
#include <pugixml.hpp>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>

static int Fail(int code,const std::string& reason) {
    std::cerr<<"FAIL Native3DSHelperRegression checkpoint "<<code<<": "<<reason<<'\n';return code;
}
static bool Eq(float a,float b,float epsilon=.09f) {return std::fabs(a-b)<epsilon;}
static size_t Count(pugi::xml_node p,const char* name) {size_t n=0;for(const auto x:p.children(name)){(void)x;++n;}return n;}
int main(int argc,char** argv) {
    if(argc!=2)return Fail(1,"Expected fixture directory");
    const auto dir=std::filesystem::path(argv[1]);
    const auto wall=NativeCollisionImport::Read(dir/"wall_e1.3ds");
    const auto broken=NativeCollisionImport::Read(dir/"hs_part6.3ds");
    const auto bridge=NativeCollisionImport::Read(dir/"brid_1.3ds");
    const auto house=NativeCollisionImport::Read(dir/"nubu_3.3ds");
    const auto waffle=NativeCollisionImport::Read(dir/"waffle_wall_1.3ds");
    const auto billboard=NativeCollisionImport::Read(dir/"promotion_bilboard.3ds");
    const auto tower=NativeCollisionImport::Read(dir/"fab_tow.3ds");
    const auto brokenTower=NativeCollisionImport::Read(dir/"fab_tow2.3ds");
    const auto outerWall=NativeCollisionImport::Read(dir/"outer_wall1_ow_1.3ds");
    const auto scaledBox=NativeCollisionImport::Read(dir/"combuild_comb3.3ds");
    if(!outerWall.Valid() || outerWall.planes.size()!=2 || !outerWall.boxes.empty() || !outerWall.triangles.empty())
        return Fail(42,"original Outer Wall 1 has two near-rectangular native plane helpers: "+outerWall.error);
    if(!scaledBox.Valid() || scaledBox.boxes.size()!=1 || !scaledBox.planes.empty() || !scaledBox.triangles.empty() ||
       !Eq(scaledBox.boxes[0].size.x,500.f,.05f) || !Eq(scaledBox.boxes[0].size.y,60.f,.05f) ||
       !Eq(scaledBox.boxes[0].size.z,150.f,.05f))
        return Fail(43,"original ComBuild box with orthogonal scaled frame: "+scaledBox.error);
    if (!tower.Valid() || tower.planes.size()!=6 || !tower.boxes.empty() || !tower.triangles.empty())
        return Fail(40,"original Fabr Tower non-rigid plane matrices must preserve six real rectangles: "+tower.error);
    if (!brokenTower.Valid() || brokenTower.planes.size()!=6 || !brokenTower.boxes.empty() || !brokenTower.triangles.empty())
        return Fail(41,"original Broken Fabr Tower non-rigid plane matrices must preserve six real rectangles: "+brokenTower.error);

    if(!wall.Valid()||wall.planes.size()!=6||wall.triangles.size()!=10)
        return Fail(2,"original Wall End 1 3DS must provide 6 planes + 10 triangles: "+wall.error);
    if(!broken.Valid()||!broken.planes.empty()||broken.triangles.size()!=6)
        return Fail(3,"original Hs_part06 must provide six triangle helpers: "+broken.error);
    if(!bridge.Valid()||bridge.planes.size()!=2||bridge.triangles.size()!=2)
        return Fail(4,"original Bridge 1 helper count: "+bridge.error);
    if(!house.Valid()||house.boxes.size()!=2||!house.planes.empty()||!house.triangles.empty())
        return Fail(27,"original NuBu 3 has two native box helpers: "+house.error);
    if(!Eq(house.boxes[0].size.x,1550.867f,.03f)||
       !Eq(house.boxes[0].size.y,1107.762f,.03f)||
       !Eq(house.boxes[0].size.z,1100.f,.03f))return Fail(28,"original box source bounds");
    if(!waffle.Valid()||waffle.boxes.size()!=1||
       !Eq(waffle.boxes[0].offset.x,0.f,.03f)||
       !Eq(waffle.boxes[0].size.x,500.f,.03f)||
       !Eq(waffle.boxes[0].size.y,30.300f,.03f)||
       !Eq(waffle.boxes[0].rotation.z,0.f,.003f))
        return Fail(33,"nonidentity visual pivot must use local collision basis: "+waffle.error);
    if(!billboard.Valid()||billboard.boxes.size()!=2||
       !Eq(billboard.boxes[0].size.x,45.f,.03f)||
       !Eq(billboard.boxes[0].size.y,100.f,.03f)||
       !Eq(billboard.boxes[0].rotation.z,-1.57079632679f,.003f))
        return Fail(34,"rotated box must retain local sizes and yaw: "+billboard.error);
    // Duplicate or invalid asset chunks must never be inferred as a visual collision.
    const auto missing=NativeCollisionImport::Read(dir/"missing.3ds");
    if(missing.Valid())return Fail(5,"missing model must be rejected");
    const auto scaled=NativeCollisionImport::Read(dir/"invalid_scaled_visual_frame.3ds");
    if(scaled.Valid()||scaled.error.find("scale/shear/mirror")==std::string::npos)
        return Fail(39,"non-rigid original 3DS frame must fail closed");
    // Independent GTanks Editor / ProTLVK native XML (not authored by this writer).
    // Equivalent plane orientations and triangle-local vertex orderings must bind
    // via transformed world geometry without ever fabricating new colliders.
    std::string error;
    MapDocument original;
    if(!original.Load(dir/"concrete_wall_end1_original_map.xml",error))
        return Fail(21,"load original editor reference: "+error);
    if(original.Props().size()!=1||original.CollisionPlanes().size()!=6||
       original.CollisionTriangles().size()!=10)return Fail(22,"original reference counts");
    if(!original.BindImportedCollisionForProp(0,wall))return Fail(23,"bind original source geometry");
    if(original.BindImportedCollisionForProp(0,wall))return Fail(24,"original owner rebound twice");
    const auto oldOriginal=original.CollisionTriangles()[0].position;
    auto originalPos=original.Props()[0].position,originalRot=original.Props()[0].rotation;
    originalPos.x+=500.f;originalRot.z+=1.57079632679f;
    if(!original.SetPropTransform(0,originalPos,originalRot)||
       Eq(original.CollisionTriangles()[0].position.x,oldOriginal.x))
        return Fail(25,"original triangle did not follow its wall");
    std::string deletion;
    if(!original.DeletePropWithCollision(0,deletion)||!original.CollisionPlanes().empty()||
       !original.CollisionTriangles().empty())return Fail(26,"original helper collision survives wall delete");
    MapDocument originalHouse;
    if(!originalHouse.Load(dir/"nubu3_original_map.xml",error)||
       originalHouse.Props().size()!=1||originalHouse.CollisionBoxes().size()!=2)
        return Fail(29,"read original NuBu 3 game XML: "+error);
    if(!originalHouse.BindImportedCollisionForProp(0,house))
        return Fail(30,"original NuBu 3 boxes must bind at exact original positions");
    const auto previousHouse=originalHouse.CollisionBoxes()[0].position;
    auto housePos=originalHouse.Props()[0].position;
    housePos.x+=500.f;
    if(!originalHouse.SetPropTransform(0,housePos,originalHouse.Props()[0].rotation)||
       !Eq(originalHouse.CollisionBoxes()[0].position.x,previousHouse.x+500.f))
        return Fail(31,"original house collision boxes must follow moved prop");
    if(!originalHouse.DeletePropWithCollision(0,error)||!originalHouse.CollisionBoxes().empty())
        return Fail(32,"house deletion leaves invisible boxes");
    for(const auto* item : {"waffle_wall1_original_map.xml","billboard_original_map.xml"}) {
        const bool isWaffle=std::string(item)=="waffle_wall1_original_map.xml";
        const auto& imported=isWaffle?waffle:billboard;
        const size_t want=isWaffle?1u:2u;
        MapDocument reference;
        if(!reference.Load(dir/item,error)||reference.Props().size()!=1||reference.CollisionBoxes().size()!=want)
            return Fail(35,std::string("load original rotated-box map: ")+item+" "+error);
        if(!reference.BindImportedCollisionForProp(0,imported))
            return Fail(36,std::string("bind original rotated helper box: ")+item);
        auto next=reference.Props()[0].position;
        const auto rot=reference.Props()[0].rotation;
        const auto oldBox=reference.CollisionBoxes()[0].position;
        next.x+=500.f;
        if(!reference.SetPropTransform(0,next,rot)||
           !Eq(reference.CollisionBoxes()[0].position.x,oldBox.x+500.f))
            return Fail(37,std::string("rotated helper did not follow prop: ")+item);
        if(!reference.DeletePropWithCollision(0,error)||!reference.CollisionBoxes().empty())
            return Fail(38,std::string("rotated helper left invisible box: ")+item);
    }
    MapDocument map;map.CreateBlank();
    const NativeCollisionImport::Result* source[]={&wall,&broken,&bridge,&house};
    const char* libs[]={"Concrete Walls","Broken Walls","Industrial Bridge","NuBu 3"};
    const char* names[]={"Wall End 1","Hs_part06","Bridge 1","NuBu 3"};
    for(int i=0;i<4;++i){
        PropInstance p;p.library=libs[i];p.group="default";p.name=names[i];
        p.position={3500.f+i*2000.f,3250.f,0.f};p.rotation.z=i*1.57079632679f;
        const auto idx=map.AddProp(p);
        if(!map.AddImportedCollisionForProp(idx,*source[i])||!map.HasNativeCollisionForProp(idx))
            return Fail(6,"author native helpers");
        if(map.AddImportedCollisionForProp(idx,*source[i]))return Fail(7,"duplicate native helpers accepted");
    }
    if(map.CollisionPlanes().size()!=8||map.CollisionTriangles().size()!=18||map.CollisionBoxes().size()!=2)
        return Fail(8,"authored collision counts");
    const auto path=std::filesystem::temp_directory_path()/"protanki-0515-native-helper.xml";
    const auto path2=std::filesystem::temp_directory_path()/"protanki-0515-native-helper-move.xml";
    struct Cleaner{std::filesystem::path a,b;~Cleaner(){std::error_code ec;std::filesystem::remove(a,ec);std::filesystem::remove(b,ec);}} cleaner{path,path2};
    if(!map.SaveLegacyAs(path,error))return Fail(9,"save "+error);
    pugi::xml_document xml;
    if(!xml.load_file(path.c_str()))return Fail(10,"parse saved XML");
    auto collision=xml.child("map").child("collision-geometry");
    if(Count(collision,"collision-plane")!=8||Count(collision,"collision-triangle")!=18||Count(collision,"collision-box")!=2)
        return Fail(11,"saved native collision counts");
    MapDocument loaded;if(!loaded.Load(path,error))return Fail(12,"reload "+error);
    if(!loaded.BindImportedCollisionForProp(0,wall)||!loaded.BindImportedCollisionForProp(1,broken)||
       !loaded.BindImportedCollisionForProp(2,bridge)||!loaded.BindImportedCollisionForProp(3,house))return Fail(13,"complete safe owner rebind after save");
    auto pos=loaded.Props()[1].position,rot=loaded.Props()[1].rotation;
    const auto oldTri=loaded.CollisionTriangles()[10].position;
    pos.x+=750.f;rot.z+=1.57079632679f;
    if(!loaded.SetPropTransform(1,pos,rot))return Fail(14,"move authored helper owner");
    const auto newTri=loaded.CollisionTriangles()[10].position;
    if(Eq(oldTri.x,newTri.x))return Fail(15,"helper triangle did not follow owner");
    if(!loaded.SaveLegacyAs(path2,error))return Fail(16,"save moved map "+error);
    MapDocument rotated;if(!rotated.Load(path2,error)||rotated.CollisionTriangles().size()!=18)
        return Fail(17,"round-trip moved native shapes");
    if(!rotated.BindImportedCollisionForProp(0,wall)||!rotated.BindImportedCollisionForProp(1,broken)||
       !rotated.BindImportedCollisionForProp(2,bridge)||!rotated.BindImportedCollisionForProp(3,house))return Fail(18,"post-rotation owner rebind");
    if(!rotated.DeletePropWithCollision(1,error))return Fail(19,"delete entire owned wall: "+error);
    if(rotated.CollisionPlanes().size()!=8||rotated.CollisionTriangles().size()!=12||
       rotated.Props().size()!=3)return Fail(20,"deletion left invisible native helper triangles");
    std::cout<<"PASS original 3DS helpers: Wall End 1 (6+10), Hs_part06 (0+6), Bridge 1 (2+2), NuBu 3 (2 boxes), Waffle Wall 1 (rotated visual), Billboard (rotated boxes); "
             <<"native XML roundtrip, owner rebind, transform, deletion.\n";
    return 0;
}
