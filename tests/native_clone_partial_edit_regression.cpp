#include "MapDocument.h"
#include "ReleaseTestCheck.h"
#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace {
bool Close(float a,float b) {return std::fabs(a-b)<.001f;}
}
int main() {
    MapDocument target;target.CreateBlank();
    PropInstance existing;existing.library="Existing";existing.name="Background";
    existing.position={-5000,0,0};
    PT_REQUIRE(target.AddProp(existing)==0);
    std::vector<PropInstance> props(3);
    for(size_t i=0;i<props.size();++i) {
        props[i].library="Canyon";props[i].group="default";
        props[i].name="Prop"+std::to_string(i);
        props[i].position={float(1000+1000*i),0,0};
    }
    // Two props have independently verified complete native collision bundles.
    // A third has unowned native triangles; all are in ONE lossless copied map.
    std::vector<CollisionBox> boxes(2);
    for(int i=0;i<2;++i) {
        boxes[i].authoredOwnerIndex=i;
        boxes[i].position=props[static_cast<size_t>(i)].position;
        boxes[i].size={50,50,50};
    }
    CollisionTriangle opaque;
    opaque.position={3000,0,0};opaque.v0={0,0,0};
    opaque.v1={100,0,0};opaque.v2={0,100,0};
    std::vector<int> inserted;std::string error;
    PT_REQUIRE(target.AppendLosslessNativeStaticClone(props,{},boxes,{opaque},inserted,error));
    PT_REQUIRE(inserted.size()==3 && inserted[0]==1 && inserted[2]==3);
    PT_REQUIRE(target.CollisionBoxes().size()==2 && target.CollisionTriangles().size()==1);
    const auto position=target.Props()[1].position;
    const DirectX::XMFLOAT3 moved{position.x+500,position.y+200,position.z};
    PT_REQUIRE(target.SetPropTransform(1,moved,target.Props()[1].rotation));
    PT_REQUIRE(Close(target.CollisionBoxes()[0].position.x,moved.x));
    PT_REQUIRE(Close(target.CollisionBoxes()[0].position.y,moved.y));
    PT_REQUIRE(Close(target.CollisionTriangles()[0].position.x,3000)); // opaque never moved
    const auto opaqueOwner=target.Props()[3].position;
    PT_REQUIRE(!target.SetPropTransform(3,{4000,0,0},target.Props()[3].rotation));
    PT_REQUIRE(Close(target.Props()[3].position.x,opaqueOwner.x));
    // An ambiguous deletion is ATOMIC; valid owned objects remain editable.
    PT_REQUIRE(!target.DeletePropsWithCollision({1,3},error));
    PT_REQUIRE(error.find("no individually verified collision owner")!=std::string::npos);
    PT_REQUIRE(target.Props().size()==4 && target.CollisionBoxes().size()==2);
    PT_REQUIRE(target.DeletePropsWithCollision({1},error));
    PT_REQUIRE(target.Props().size()==3 && target.CollisionBoxes().size()==1);
    PT_REQUIRE(target.CollisionTriangles().size()==1 &&
               Close(target.CollisionTriangles()[0].position.x,3000));
    // A second verified prop can be removed without wiping the opaque island.
    PT_REQUIRE(target.DeletePropsWithCollision({1},error));
    PT_REQUIRE(target.Props().size()==2 && target.CollisionBoxes().empty());
    // The sole remaining clone member is now the whole remaining batch.
    // Rejecting it here would contradict the full-batch cleanup rule; the
    // ambiguous partial deletion was already checked above (with {1,3}).
    // Removing the LAST remaining clone member is a full batch deletion,
    // so its opaque native collision must disappear with it.
    PT_REQUIRE(target.DeletePropsWithCollision({1},error));
    PT_REQUIRE(target.Props().size()==1 && target.CollisionTriangles().empty());
    PT_REQUIRE(target.Props()[0].name=="Background");

    // Canyon reproducer: two legitimate sprite-only props coexist with
    // unrelated opaque map triangles; neither may modify those triangles.
    MapDocument sprites; sprites.CreateBlank();
    std::vector<PropInstance> vegetation(3);
    vegetation[0].library="Bush";vegetation[0].group="default";vegetation[0].name="Tree 1";
    vegetation[1].library="Bush";vegetation[1].group="default";vegetation[1].name="Bush 1";
    vegetation[2].library="Unknown";vegetation[2].group="default";vegetation[2].name="Unbound Mesh";
    vegetation[0].position={100,0,0};vegetation[1].position={200,0,0};vegetation[2].position={300,0,0};
    // Simulate EditorUi's exact external sprite-only asset verification.
    sprites.AddProp(vegetation[0]);sprites.SetVerifiedCollisionFreeSprite(0,true);
    sprites.AddProp(vegetation[1]);sprites.SetVerifiedCollisionFreeSprite(1,true);
    PT_REQUIRE(sprites.Props()[0].verifiedCollisionFreeSprite);
    PT_REQUIRE(sprites.Props()[1].verifiedCollisionFreeSprite);
    vegetation[0]=sprites.Props()[0];vegetation[1]=sprites.Props()[1];
    std::vector<int> spriteIds;
    PT_REQUIRE(sprites.AppendLosslessNativeStaticClone(vegetation,{}, {},{opaque},spriteIds,error));
    PT_REQUIRE(spriteIds.size()==3 && spriteIds[0]==2);
    PT_REQUIRE(sprites.Props()[2].verifiedCollisionFreeSprite);
    PT_REQUIRE(sprites.Props()[3].verifiedCollisionFreeSprite);
    PT_REQUIRE(!sprites.Props()[4].verifiedCollisionFreeSprite);
    auto newTree=sprites.Props()[2].position;newTree.x+=125;
    PT_REQUIRE(sprites.SetPropTransform(2,newTree,sprites.Props()[2].rotation));
    PT_REQUIRE(Close(sprites.CollisionTriangles()[0].position.x,3000));
    PT_REQUIRE(!sprites.SetPropTransform(4,{400,0,0},sprites.Props()[4].rotation));
    PT_REQUIRE(!sprites.DeletePropsWithCollision({3,4},error)); // atomic mixed ownership
    PT_REQUIRE(sprites.DeletePropsWithCollision({3},error)); // copied Bush 1
    PT_REQUIRE(sprites.DeletePropsWithCollision({2},error)); // copied Tree 1
    PT_REQUIRE(sprites.Props().size()==3);
    PT_REQUIRE(sprites.CollisionTriangles().size()==1); // opaque map triangles untouched
    PT_REQUIRE(sprites.DeletePropsWithCollision({2},error)); // remaining full clone batch
    PT_REQUIRE(sprites.CollisionTriangles().empty());
    MapDocument explicitCollision;explicitCollision.CreateBlank();
    PropInstance collidingSprite;collidingSprite.nativeWithCollision=1;
    explicitCollision.AddProp(collidingSprite);
    explicitCollision.SetVerifiedCollisionFreeSprite(0,true);
    PT_REQUIRE(!explicitCollision.Props()[0].verifiedCollisionFreeSprite);

    // An opaque game flag alone does NOT establish ownership of independent
    // collision XML. Even with with_collision=0 we refuse to invent a match.
    MapDocument visual;visual.CreateBlank();
    props[0].nativeWithCollision=0;
    std::vector<int> visualIds;
    PT_REQUIRE(visual.AppendLosslessNativeStaticClone(props,{}, {},{opaque},visualIds,error));
    PT_REQUIRE(!visual.SetPropTransform(0,{1500,0,0},visual.Props()[0].rotation));
    PT_REQUIRE(!visual.DeletePropsWithCollision({0},error));
    PT_REQUIRE(visual.CollisionTriangles().size()==1);
    PT_REQUIRE(visual.DeletePropsWithCollision({0,1,2},error));
    PT_REQUIRE(visual.Props().empty() && visual.CollisionTriangles().empty());

    // A complete batch with unresolved flags must still be removable as a unit.
    MapDocument unresolved;unresolved.CreateBlank();
    PT_REQUIRE(unresolved.AddProp(existing)==0);props[0].nativeWithCollision=-1;
    props[1].collisionOwnershipUnresolved=true;
    PT_REQUIRE(unresolved.AppendLosslessNativeStaticClone(props,{}, {},{opaque},inserted,error));
    PT_REQUIRE(unresolved.DeletePropsWithCollision({1,2,3},error));
    PT_REQUIRE(unresolved.Props().size()==1 && unresolved.CollisionTriangles().empty());
    std::cout<<"PASS: verified partial lossless clone move/delete, opaque collision preserved, "
                "ambiguous edits atomic, complete batch cleanup and visual-only edits.\n";
    return 0;
}
