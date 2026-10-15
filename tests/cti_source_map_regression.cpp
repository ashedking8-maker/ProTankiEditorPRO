#include "MapDocument.h"
#include "EditHistory.h"
#include "VerifiedMeshSurface.h"
#include "SourceCollisionLearning.h"
#include "ReleaseTestCheck.h"
#include <filesystem>
#include <fstream>
#include <cstring>
#include <iostream>
int main(int argc,char**argv) {
    PT_REQUIRE(argc==2);const auto dir=std::filesystem::path(argv[1]);std::string error;
    const auto identity=SourceCollisionCatalog::Identity("Fogtown","b","cti");
    MapDocument reference;PT_REQUIRE(reference.Load(dir/"reference.xml",error));
    SourceCollisionCatalog::Mesh mesh;PT_REQUIRE(SourceCollisionCatalog::Capture(dir/"cti.3ds","",mesh));
    SourceCollisionLearning::Index index(reference);
    auto learned=SourceCollisionLearning::Infer(reference,0,mesh,index);PT_REQUIRE(learned.Valid());
    for(size_t i=1;i<reference.Props().size();++i)
        PT_REQUIRE(SourceCollisionCatalog::Equivalent(learned,SourceCollisionLearning::Infer(reference,i,mesh,index)));
    PT_REQUIRE(SourceCollisionCatalog::Register(mesh,learned,identity));
    const auto source=VerifiedMeshSurface::Read(dir/"cti.3ds","",identity);
    PT_REQUIRE(source.Valid()&&source.planes.size()==12&&source.triangles.size()==58&&source.boxes.empty());
    PT_REQUIRE(!NativeCollisionImport::Read(dir/"cti.3ds").Valid());
    PT_REQUIRE(reference.Props().size()==11&&reference.CollisionPlanes().size()==132&&reference.CollisionTriangles().size()==638);
    // Independently copied source XML must bind the complete template at every occurrence.
    for(size_t i=0;i<reference.Props().size();++i)PT_REQUIRE(reference.BindImportedCollisionForProp(i,source));
    MapDocument generated;generated.CreateBlank();
    for(auto prop:reference.Props()) {
        const auto index=generated.AddProp(prop);
        PT_REQUIRE(generated.AddImportedCollisionForProp(index,source));
    }
    PT_REQUIRE(generated.CollisionPlanes().size()==132&&generated.CollisionTriangles().size()==638);
    const auto path=std::filesystem::temp_directory_path()/"cti_source_map_roundtrip.xml";
    PT_REQUIRE(generated.SaveLegacyAs(path,error));MapDocument loaded;PT_REQUIRE(loaded.Load(path,error));
    for(size_t i=0;i<loaded.Props().size();++i)PT_REQUIRE(loaded.BindImportedCollisionForProp(i,source));
    const auto before=loaded;auto p=loaded.Props()[0].position;p.x+=500;p.z+=300;
    auto r=loaded.Props()[0].rotation;r.z+=1.57079632679f;
    PT_REQUIRE(loaded.SetPropTransform(0,p,r));
    std::vector<CollisionPlane> planes;std::vector<CollisionBox> boxes;std::vector<CollisionTriangle> triangles;
    PT_REQUIRE(loaded.CopyNativeCollisionForProps({0},planes,boxes,triangles));
    PT_REQUIRE(planes.size()==12&&triangles.size()==58&&boxes.empty());
    EditHistory history;history.PushSnapshot(before,loaded);std::vector<size_t> changed;
    PT_REQUIRE(history.Undo(loaded,changed));PT_REQUIRE(history.Redo(loaded,changed));
    PT_REQUIRE(loaded.DeletePropWithCollision(0,error));
    PT_REQUIRE(loaded.CollisionPlanes().size()==120&&loaded.CollisionTriangles().size()==580);
    std::filesystem::remove(path);
    // A modified mesh must not silently inherit the old certified physics.
    std::ifstream in(dir/"cti.3ds",std::ios::binary);
    std::vector<char> bytes((std::istreambuf_iterator<char>(in)),{});bool edited=false;
    for(size_t i=0;i+12<bytes.size();++i)if(static_cast<unsigned char>(bytes[i])==0x10&&static_cast<unsigned char>(bytes[i+1])==0x41) {
        uint16_t count=0;std::memcpy(&count,bytes.data()+i+6,2);if(count!=160)continue;
        float x;std::memcpy(&x,bytes.data()+i+8,4);x+=1;std::memcpy(bytes.data()+i+8,&x,4);edited=true;break;
    }
    PT_REQUIRE(edited);const auto altered=std::filesystem::temp_directory_path()/"cti_modified.3ds";
    {std::ofstream out(altered,std::ios::binary);out.write(bytes.data(),static_cast<std::streamsize>(bytes.size()));PT_REQUIRE(bool(out));}
    PT_REQUIRE(!VerifiedMeshSurface::Read(altered,"",identity).Valid());std::filesystem::remove(altered);
    std::cout<<"PASS: 11 original instances; 132 planes + 638 triangles; save/reload; transformed copy; undo/redo/delete; changed-mesh rejection\n";
}
