#include "CollisionPlacementPolicy.h"
#include "NativeCollisionImport.h"
#include "ReleaseTestCheck.h"
#include <filesystem>
#include <iostream>
int main(int argc,char** argv) {
    PT_REQUIRE(argc==2);
    // Every non-sprite mesh with failed collision authoring is rejected,
    // independent of library identity, slope, helper error or bounds.
    for(int flag : {-1,0,1}) {
        PT_REQUIRE(!CollisionPlacementPolicy::MayPlaceWithoutCollision(false,false,flag));
        PT_REQUIRE(CollisionPlacementPolicy::MayPlaceWithoutCollision(false,true,flag)==(flag!=1));
        PT_REQUIRE(CollisionPlacementPolicy::MayPlaceWithoutCollision(true,false,flag)==(flag!=1));
    }
    const auto root=std::filesystem::path(argv[1]);
    const auto floor=NativeCollisionImport::Read(root/"fogtown_ground/t11.3ds");
    PT_REQUIRE(!floor.Valid() && floor.error==NativeCollisionImport::NoNativeHelpersError);
    PT_REQUIRE(!CollisionPlacementPolicy::MayPlaceWithoutCollision(false,false,-1));
    // This differently shaped fixture is NOT the Fogtown land01 from the log.
    // Preserve supported helper import instead of replacing it with a mesh/AABB.
    const auto terrain=NativeCollisionImport::Read(root/"native_helpers/land01_original.3ds");
    PT_REQUIRE(terrain.Valid() && terrain.triangles.size()==136);
    std::cout<<"PASS: mesh failures blocked; explicit decoration/sprite policy; native terrain helpers preserved\n";
}
