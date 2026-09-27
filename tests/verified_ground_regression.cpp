#include "MapDocument.h"
#include "VerifiedGroundCollision.h"
#include <pugixml.hpp>
#include <array>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>

static bool Eq(float a,float b){return std::fabs(a-b)<.1f;}
static int Fail(int line){std::cerr<<"VerifiedGround regression failure at line "<<line<<'\n';return line;}
#define CHECK(v) do {if(!(v))return Fail(__LINE__);}while(false)
int main(int argc,char** argv) {
    CHECK(argc==4);
    const std::filesystem::path assetDir=argv[1],reference=argv[2];
    const std::array<bool,6> all{true,true,true,true,true,true};
    const std::array<const char*,6> names= {"t11","t21","t22","t32","t33","t55"};
    for(const auto* name:names) {
        const auto* spec=VerifiedGroundCollision::Find("Fogtown","l",name);
        CHECK(spec!=nullptr);
        const auto probe=VerifiedGroundCollision::Inspect(assetDir/(std::string(name)+".3ds"),spec);
        CHECK(probe.flatRectangle && probe.matchesNativeReference);
    }
    CHECK(!VerifiedGroundCollision::Inspect(assetDir/"t11.3ds",
        VerifiedGroundCollision::Find("Fogtown","l","t22")).matchesNativeReference);
    CHECK(VerifiedGroundCollision::Find("Fogtown","w","wt1")==nullptr);
    CHECK(!VerifiedGroundCollision::Inspect(assetDir/"missing.3ds",
        VerifiedGroundCollision::Find("Fogtown","l","t22")).matchesNativeReference);

    std::string err;MapDocument original;
    CHECK(original.Load(reference,err));
    CHECK(original.Props().size()==6&&original.CollisionPlanes().size()==6);
    for(size_t i=0;i<6;++i){
        CHECK(original.Props()[i].name==names[i]);
        CHECK(original.HasVerifiedGroundSurfaceForProp(i));
        CHECK(original.HasNativeCollisionForProp(i)); // safely rebound by exact template
    }
    size_t added=0,unresolved=0;
    CHECK(original.RepairVerifiedGroundSurfaces(all,added,unresolved)&&added==0&&unresolved==0);

    MapDocument missing;missing.CreateBlank();
    for(const auto& p:original.Props()){CHECK(missing.AddProp(p)<6);}
    CHECK(missing.CollisionPlanes().empty());
    CHECK(missing.RepairVerifiedGroundSurfaces(all,added,unresolved));
    CHECK(added==6&&unresolved==0&&missing.CollisionPlanes().size()==6);
    CHECK(missing.RepairVerifiedGroundSurfaces(all,added,unresolved)&&added==0&&unresolved==0);
    const auto path=std::filesystem::temp_directory_path()/"ptpro-verified-ground-0528.xml";
    CHECK(missing.SaveLegacyAs(path,err));
    pugi::xml_document saved;CHECK(saved.load_file(path.c_str()));
    auto collision=saved.child("map").child("collision-geometry");
    size_t planeCount=0;
    for(auto c:collision.children("collision-plane")) {(void)c;++planeCount;}
    CHECK(planeCount==6);
    std::array<bool,6> ids{};
    for(auto c:collision.children("collision-plane")) {
        const int id=c.attribute("id").as_int(-1);CHECK(id>=0&&id<6&&!ids[static_cast<size_t>(id)]);
        ids[static_cast<size_t>(id)]=true;
    }
    MapDocument roundTrip;CHECK(roundTrip.Load(path,err));
    for(size_t i=0;i<6;++i)CHECK(roundTrip.HasNativeCollisionForProp(i)&&
        roundTrip.HasVerifiedGroundSurfaceForProp(i));
    // Exact user reproduction: 15 props but all eleven flat Fogtown tiles
    // lack their native XML planes. Save-time preflight must repair ONLY those
    // missing planes while retaining the NuBu2 building and 13 old colliders.
    MapDocument repro;CHECK(repro.Load(argv[3],err));
    CHECK(repro.Props().size()==15&&repro.CollisionPlanes().size()==3&&
          repro.CollisionBoxes().size()==10);
    CHECK(repro.RepairVerifiedGroundSurfaces(all,added,unresolved));
    CHECK(added==11&&unresolved==0&&repro.CollisionPlanes().size()==14&&
          repro.CollisionBoxes().size()==10);
    CHECK(repro.Props()[8].library=="NuBu 2 Winter"&&repro.Props()[8].name=="NuBu 2");
    CHECK(repro.RepairVerifiedGroundSurfaces(all,added,unresolved)&&added==0&&unresolved==0);
    const auto reproPath=std::filesystem::temp_directory_path()/"ptpro-verified-ground-user-repro-0528.xml";
    CHECK(repro.SaveLegacyAs(reproPath,err));
    MapDocument reproRound;CHECK(reproRound.Load(reproPath,err));
    CHECK(reproRound.Props().size()==15&&reproRound.CollisionPlanes().size()==14&&
          reproRound.CollisionBoxes().size()==10);
    for(size_t i=0;i<reproRound.Props().size();++i)
        if(VerifiedGroundCollision::Find(reproRound.Props()[i].library,
            reproRound.Props()[i].group,reproRound.Props()[i].name))
            CHECK(reproRound.HasVerifiedGroundSurfaceForProp(i));
    std::error_code reproEc;std::filesystem::remove(reproPath,reproEc);
    // Offset-plane t21: moving/turning must rotate its center with its owner.
    const auto first=roundTrip.CollisionPlanes()[1].position;
    const auto p=roundTrip.Props()[1];auto at=p.position;auto rot=p.rotation;
    at.x+=1000;rot.z+=1.5707963267948966f;
    CHECK(roundTrip.SetPropTransform(1,at,rot));
    CHECK(Eq(roundTrip.CollisionPlanes()[1].position.x,first.x+1250));
    CHECK(Eq(roundTrip.CollisionPlanes()[1].position.y,first.y+250));
    CHECK(roundTrip.SaveLegacyAs(path,err));
    MapDocument moved;CHECK(moved.Load(path,err));
    CHECK(moved.HasNativeCollisionForProp(1)&&moved.HasVerifiedGroundSurfaceForProp(1));
    std::string why;CHECK(moved.DeletePropWithCollision(1,why));
    CHECK(moved.CollisionPlanes().size()==5&&moved.Props().size()==5);
    CHECK(moved.SaveLegacyAs(path,err));
    MapDocument afterDelete;CHECK(afterDelete.Load(path,err));
    CHECK(afterDelete.CollisionPlanes().size()==5);
    // Known original ground cannot be auto-authored if its external mesh has
    // not been verified by the editor (caller must roll back a partial repair).
    MapDocument failClosed;failClosed.CreateBlank();
    PropInstance naked;naked.library="Fogtown";naked.group="l";naked.name="t22";
    failClosed.AddProp(naked);
    const std::array<bool,6> none{};
    CHECK(!failClosed.RepairVerifiedGroundSurfaces(none,added,unresolved));
    CHECK(added==0&&unresolved==1&&failClosed.CollisionPlanes().empty());
    // A duplicate at exactly the same transform may not get a second owner.
    MapDocument duplicate;duplicate.CreateBlank();
    duplicate.AddProp(naked);duplicate.AddProp(naked);
    CHECK(!duplicate.AddVerifiedGroundSurfaceForProp(0));
    CHECK(!duplicate.RepairVerifiedGroundSurfaces(all,added,unresolved));
    CHECK(added==0&&unresolved==2&&duplicate.CollisionPlanes().empty());
    // Ambiguous saved source ownership is never guessed on reload.
    pugi::xml_document sourceDoc;CHECK(sourceDoc.load_file(reference.c_str()));
    auto sourceGeometry=sourceDoc.child("map").child("static-geometry");
    auto addedDuplicate=sourceGeometry.append_copy(sourceGeometry.child("prop"));
    CHECK(addedDuplicate);
    const auto ambiguousPath=std::filesystem::temp_directory_path()/"ptpro-ambiguous-floor-0528.xml";
    CHECK(sourceDoc.save_file(ambiguousPath.c_str()));
    MapDocument ambiguous;CHECK(ambiguous.Load(ambiguousPath,err));
    CHECK(ambiguous.HasVerifiedGroundSurfaceForProp(0));
    CHECK(!ambiguous.HasNativeCollisionForProp(0));
    auto movedPos=ambiguous.Props()[0].position;
    movedPos.x+=500;
    CHECK(!ambiguous.SetPropTransform(0,movedPos,ambiguous.Props()[0].rotation));
    CHECK(!ambiguous.DeletePropWithCollision(0,why));
    std::error_code ec;std::filesystem::remove(ambiguousPath,ec);std::filesystem::remove(path,ec);
    std::cout<<"6 original Fogtown ground 3DS matches; user 15-prop/11-floor regression, native XML, offsets, IDs, moves and delete OK\n";
}
