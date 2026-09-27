#include "MapDocument.h"
#include "NativeCollisionImport.h"
#include <filesystem>
#include <cmath>
#include <iostream>
#include <string>

#define CHECK(x) do{if(!(x)){std::cerr<<"Esplanade regression failed line "<<__LINE__<<": "<<#x<<'\n';return __LINE__;}}while(false)
int main(int argc,char** argv) {
    CHECK(argc==2);
    const auto path=std::filesystem::path(argv[1]);
    const auto wall=NativeCollisionImport::Read(path/"wtile_1.3ds");
    const auto container=NativeCollisionImport::Read(path/"contain.3ds");
    const auto tunnel=NativeCollisionImport::Read(path/"tunnel_1.3ds");
    CHECK(wall.Valid() && wall.visualAnchor=="wtile_1" && wall.planes.size()==1);
    CHECK(container.Valid() && container.visualAnchor=="Box04" && container.boxes.size()==1);
    CHECK(std::fabs(container.boxes[0].size.x-400.f)<.02f);
    CHECK(std::fabs(container.boxes[0].size.y-900.f)<.02f);
    CHECK(std::fabs(container.boxes[0].size.z-300.f)<.02f);
    CHECK(tunnel.Valid() && tunnel.visualAnchor=="Box01" &&
          tunnel.planes.size()==3 && tunnel.boxes.size()==3 && tunnel.triangles.size()==2);
    std::string err;
    MapDocument source;
    CHECK(source.Load(path/"original_collision_excerpt.xml",err));
    CHECK(source.Props().size()==3 && source.CollisionPlanes().size()==2 &&
          source.CollisionBoxes().size()==1);
    CHECK(source.BindImportedCollisionForProp(0,wall));
    CHECK(source.BindImportedCollisionForProp(1,wall));
    CHECK(source.BindImportedCollisionForProp(2,container));
    CHECK(source.HasNativeCollisionForProp(0) && source.HasNativeCollisionForProp(1) &&
          source.HasNativeCollisionForProp(2));
    MapDocument clone;clone.CreateBlank();
    auto first=source.Props()[0];auto second=source.Props()[1];
    auto i0=clone.AddProp(first);
    CHECK(clone.AddImportedCollisionForProp(i0,wall));
    auto i1=clone.AddProp(second);
    CHECK(!clone.AddImportedCollisionForProp(i1,wall)); // No unproved coincident helpers.
    CHECK(!clone.AddImportedCollisionForProp(i1,wall,true)); // Missing staged boundary.
    CHECK(clone.AddImportedCollisionForProp(i1,wall,true,0)); // Verified source pair.
    CHECK(clone.CollisionPlanes().size()==2 && clone.HasNativeCollisionForProp(i0) &&
          clone.HasNativeCollisionForProp(i1));
    auto ic=clone.AddProp(source.Props()[2]);
    CHECK(clone.AddImportedCollisionForProp(ic,container));
    CHECK(clone.CollisionBoxes().size()==1);
    const auto out=std::filesystem::temp_directory_path()/"ptpro-esplanade-0528-copy.xml";
    CHECK(clone.SaveLegacyAs(out,err));
    MapDocument reopened;CHECK(reopened.Load(out,err));
    CHECK(reopened.Props().size()==3 && reopened.CollisionPlanes().size()==2 &&
          reopened.CollisionBoxes().size()==1);
    CHECK(reopened.BindImportedCollisionForProp(0,wall));
    CHECK(reopened.BindImportedCollisionForProp(1,wall));
    CHECK(reopened.BindImportedCollisionForProp(2,container));
    std::string why;
    CHECK(reopened.DeletePropWithCollision(0,why));
    CHECK(reopened.Props().size()==2 && reopened.CollisionPlanes().size()==1 &&
          reopened.HasNativeCollisionForProp(0));
    auto pos=reopened.Props()[0].position;
    pos.x+=500.f;
    const auto oldPlane=reopened.CollisionPlanes()[0].position;
    CHECK(reopened.SetPropTransform(0,pos,reopened.Props()[0].rotation));
    CHECK(std::fabs(reopened.CollisionPlanes()[0].position.x-oldPlane.x-500.f)<.02f);
    CHECK(reopened.SaveLegacyAs(out,err));
    MapDocument final;CHECK(final.Load(out,err));
    CHECK(final.Props().size()==2 && final.CollisionPlanes().size()==1 &&
          final.CollisionBoxes().size()==1);
    std::error_code ec;std::filesystem::remove(out,ec);
    std::cout<<"Esplanade duplicate original WTile 2/2 planes, Container Box04/Box34, "
                "Tunnel Box01 helpers; paste/save/reload/rebind/delete/move OK\n";
    return 0;
}
