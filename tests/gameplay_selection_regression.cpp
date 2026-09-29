#include "GameplaySelection.h"
#include "ReleaseTestCheck.h"
#include <iostream>
int main(){
    namespace G=GameplaySelection;
    PT_REQUIRE(G::SpawnVisible(0,"anything")&&G::SpawnVisible(3,"RED"));
    PT_REQUIRE(!G::SpawnVisible(-1,"dm")&&!G::SpawnVisible(1,"red"));
    PT_REQUIRE(G::BonusVisible(3,{"dm","CTF"})&&!G::BonusVisible(2,{"ctf"}));
    PT_REQUIRE(!G::BonusVisible(99,{"dm"}));
    std::vector<G::Item> items;
    for(size_t i=0;i<1000;++i)G::Add(items,{G::Kind::Spawn,i});
    G::Add(items,{G::Kind::Spawn,5});PT_REQUIRE(items.size()==1000);
    G::Add(items,{G::Kind::Spawn,5},true);PT_REQUIRE(items.size()==999);
    G::Add(items,{G::Kind::Flag,5});PT_REQUIRE(items.size()==1000);
    const G::Candidate point{{G::Kind::Spawn,0},{10,20,0},{10,20,0}};
    const G::Candidate volume{{G::Kind::Zone,0},{-50,-50,0},{50,50,100}};
    auto project=[](DirectX::XMFLOAT3 p,float& x,float& y){x=p.x;y=p.y;return p.z>=0;};
    PT_REQUIRE(G::Intersects(point,0,0,30,30,project));
    PT_REQUIRE(G::Intersects(point,30,30,0,0,project));
    PT_REQUIRE(!G::Intersects(point,11,21,50,50,project));
    PT_REQUIRE(G::Intersects(volume,40,40,60,60,project));
    PT_REQUIRE(!G::Intersects(volume,60,60,80,80,project));
    std::cout<<"Gameplay selection: visibility modes, 1000-item set, additive/toggle selection and projected volume rectangles PASS\n";
}
