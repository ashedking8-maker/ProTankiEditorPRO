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
    if(!wall.Valid()||wall.planes.size()!=6||wall.triangles.size()!=10)
        return Fail(2,"original Wall End 1 3DS must provide 6 planes + 10 triangles: "+wall.error);
    if(!broken.Valid()||!broken.planes.empty()||broken.triangles.size()!=6)
        return Fail(3,"original Hs_part06 must provide six triangle helpers: "+broken.error);
    if(!bridge.Valid()||bridge.planes.size()!=2||bridge.triangles.size()!=2)
        return Fail(4,"original Bridge 1 helper count: "+bridge.error);
    // Duplicate or invalid asset chunks must never be inferred as a visual collision.
    const auto missing=NativeCollisionImport::Read(dir/"missing.3ds");
    if(missing.Valid())return Fail(5,"missing model must be rejected");
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
    MapDocument map;map.CreateBlank();
    const NativeCollisionImport::Result* source[]={&wall,&broken,&bridge};
    const char* libs[]={"Concrete Walls","Broken Walls","Industrial Bridge"};
    const char* names[]={"Wall End 1","Hs_part06","Bridge 1"};
    for(int i=0;i<3;++i){
        PropInstance p;p.library=libs[i];p.group="default";p.name=names[i];
        p.position={3500.f+i*2000.f,3250.f,0.f};p.rotation.z=i*1.57079632679f;
        const auto idx=map.AddProp(p);
        if(!map.AddImportedCollisionForProp(idx,*source[i])||!map.HasNativeCollisionForProp(idx))
            return Fail(6,"author native helpers");
        if(map.AddImportedCollisionForProp(idx,*source[i]))return Fail(7,"duplicate native helpers accepted");
    }
    if(map.CollisionPlanes().size()!=8||map.CollisionTriangles().size()!=18)
        return Fail(8,"authored collision counts");
    const auto path=std::filesystem::temp_directory_path()/"protanki-0515-native-helper.xml";
    const auto path2=std::filesystem::temp_directory_path()/"protanki-0515-native-helper-move.xml";
    struct Cleaner{std::filesystem::path a,b;~Cleaner(){std::error_code ec;std::filesystem::remove(a,ec);std::filesystem::remove(b,ec);}} cleaner{path,path2};
    if(!map.SaveLegacyAs(path,error))return Fail(9,"save "+error);
    pugi::xml_document xml;
    if(!xml.load_file(path.c_str()))return Fail(10,"parse saved XML");
    auto collision=xml.child("map").child("collision-geometry");
    if(Count(collision,"collision-plane")!=8||Count(collision,"collision-triangle")!=18)
        return Fail(11,"saved native collision counts");
    MapDocument loaded;if(!loaded.Load(path,error))return Fail(12,"reload "+error);
    if(!loaded.BindImportedCollisionForProp(0,wall)||!loaded.BindImportedCollisionForProp(1,broken)||
       !loaded.BindImportedCollisionForProp(2,bridge))return Fail(13,"complete safe owner rebind after save");
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
       !rotated.BindImportedCollisionForProp(2,bridge))return Fail(18,"post-rotation owner rebind");
    if(!rotated.DeletePropWithCollision(1,error))return Fail(19,"delete entire owned wall: "+error);
    if(rotated.CollisionPlanes().size()!=8||rotated.CollisionTriangles().size()!=12||
       rotated.Props().size()!=2)return Fail(20,"deletion left invisible native helper triangles");
    std::cout<<"PASS original 3DS helpers: Wall End 1 (6+10), Hs_part06 (0+6), Bridge 1 (2+2); "
             <<"native XML roundtrip, owner rebind, transform, deletion.\n";
    return 0;
}
