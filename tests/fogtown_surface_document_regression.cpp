#include "MapDocument.h"
#include "EditHistory.h"
#include "VerifiedMeshSurface.h"
#include "ReleaseTestCheck.h"
#include <filesystem>
#include <iostream>
int main(int argc,char**argv) {
    PT_REQUIRE(argc==2);const auto dir=std::filesystem::path(argv[1]);std::string error;
    const auto source=VerifiedMeshSurface::ReadVerified(dir/"land01.3ds");PT_REQUIRE(source.Valid());
    MapDocument map;PT_REQUIRE(map.Load(dir/"missing_surface.xml",error));
    PT_REQUIRE(map.Props().size()==6&&map.CollisionTriangles().size()==200);
    const auto original=map;size_t added=0;
    PT_REQUIRE(map.RepairVerifiedMeshSurfaceForProp(4,source,added,error)&&added==18);
    PT_REQUIRE(map.RepairVerifiedMeshSurfaceForProp(5,source,added,error)&&added==18);
    PT_REQUIRE(map.CollisionTriangles().size()==236);
    PT_REQUIRE(map.RepairVerifiedMeshSurfaceForProp(4,source,added,error)&&added==0);
    EditHistory history;history.PushSnapshot(original,map);std::vector<size_t> changed;
    PT_REQUIRE(history.Undo(map,changed)&&map.CollisionTriangles().size()==200);
    PT_REQUIRE(history.Redo(map,changed)&&map.CollisionTriangles().size()==236);
    const auto path=std::filesystem::temp_directory_path()/"fogtown_surface_document_test.xml";
    PT_REQUIRE(map.SaveLegacyAs(path,error));MapDocument loaded;PT_REQUIRE(loaded.Load(path,error));
    PT_REQUIRE(loaded.CollisionTriangles().size()==236);
    PT_REQUIRE(loaded.BindImportedCollisionForProp(4,source));
    PT_REQUIRE(loaded.BindImportedCollisionForProp(5,source));
    PT_REQUIRE(loaded.RepairVerifiedMeshSurfaceForProp(4,source,added,error)&&added==0);
    const auto before=loaded.CollisionTriangles()[200].position;
    auto position=loaded.Props()[4].position;position.x+=500;position.z+=300;
    PT_REQUIRE(loaded.SetPropTransform(4,position,loaded.Props()[4].rotation));
    PT_REQUIRE(std::abs(loaded.CollisionTriangles()[200].position.x-before.x-500)<.02f);
    PT_REQUIRE(std::abs(loaded.CollisionTriangles()[200].position.z-before.z-300)<.02f);
    std::vector<CollisionPlane> planes;std::vector<CollisionBox> boxes;std::vector<CollisionTriangle> triangles;
    PT_REQUIRE(loaded.CopyNativeCollisionForProps({4},planes,boxes,triangles)&&triangles.size()==18);
    PT_REQUIRE(loaded.DeletePropWithCollision(4,error)&&loaded.CollisionTriangles().size()==218);
    std::filesystem::remove(path);
    MapDocument reference;PT_REQUIRE(reference.Load(dir/"original_surface_instances.xml",error));
    PT_REQUIRE(reference.CollisionTriangles().size()==230);
    for(size_t i=0;i<reference.Props().size();++i) {
        const auto s=VerifiedMeshSurface::ReadVerified(dir/(reference.Props()[i].name=="land 1"?"land01.3ds":"land02.3ds"));
        PT_REQUIRE(reference.RepairVerifiedMeshSurfaceForProp(i,s,added,error)&&added==0);
    }
    PT_REQUIRE(reference.CollisionTriangles().size()==230);
    std::cout<<"PASS: source-map no-op, 200->236 repair, repeat, Undo/Redo, save/reload, move/copy/delete ownership\n";
}
