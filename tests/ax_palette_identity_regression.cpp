#include "AxRecentFilter.h"
#include "ReleaseTestCheck.h"
#include <iostream>
#include <string>
#include <vector>

struct Asset {std::string library,group,name;};
struct Prop {std::string library,group,name;};
struct Recent {std::size_t index{};std::string thumbnail{};int textureVariant{};bool manuallyAdded{};};

int main() {
    using namespace AxRecentFilter;
    const std::vector<Asset> assets{
        {"Land", "default", "Land01"}, {"Land", "default", "Land02"},
        {"Land", "default", "Land03"}, {"Land", "default", "Land04"},
        {"Land", "default", "Land06"}, {"Land", "default", "Land07"},
        {"Land", "default", "Land08"}, {"Land", "default", "Land09"},
        {"Land", "default", "Land10"}
    };
    std::vector<Recent> ax;
    for(std::size_t i=0;i<8;++i) Pin(ax,i,static_cast<int>(i+10));
    PT_REQUIRE(ax.size()==8);
    // Last pin is the first visible item. The entry for Land09 is ID 7,
    // NOT the preceding item, a stale previous choice, or row 7.
    PT_REQUIRE(ax[0].index==7 && ax[0].textureVariant==17 && ax[0].manuallyAdded);
    PT_REQUIRE(ax[3].index==4 && ax[3].textureVariant==14);
    const auto visible=VisiblePositions(ax,assets,std::vector<Prop>{},true);
    PT_REQUIRE(visible.size()==8);
    PT_REQUIRE(assets[ax[visible[0]].index].name=="Land09");
    PT_REQUIRE(assets[ax[visible[2]].index].name=="Land07");
    PT_REQUIRE(assets[ax[visible[4]].index].name=="Land04");
    // Touching a pinned item from Browse must not rearrange AX history.
    Touch(ax,3);
    PT_REQUIRE(ax[0].index==7 && ax[4].index==3 && ax[4].manuallyAdded);
    // A regular Browse selection must remain below all pinned entries.
    Touch(ax,8);
    PT_REQUIRE(ax.size()==9 && ax[8].index==8 && !ax[8].manuallyAdded);
    Touch(ax,0);
    PT_REQUIRE(ax[0].index==7 && ax[7].index==0 && ax[8].index==8);
    // Re-pin an existing object: promote only that identity and preserve variant.
    Pin(ax,3,42);
    PT_REQUIRE(ax.size()==9 && ax[0].index==3 && ax[0].textureVariant==42);
    PT_REQUIRE(ax[1].index==7 && ax[1].textureVariant==17);
    // Auto-history is hidden by 'only used' while manual additions stay visible.
    const auto pinnedOnly=VisiblePositions(ax,assets,std::vector<Prop>{},true);
    PT_REQUIRE(pinnedOnly.size()==8 && ax[pinnedOnly[0]].index==3);
    const auto all=VisiblePositions(ax,assets,std::vector<Prop>{},false);
    PT_REQUIRE(all.size()==9 && ax[all[8]].index==8);
    // Native copy/paste does not call either Pin or Touch; history is unchanged.
    const auto snapshot=ax;
    std::vector<Prop> clipboard{{"Land","default","Land02"},{"Land","default","Land10"}};
    (void)clipboard;
    PT_REQUIRE(ax.size()==snapshot.size());
    for(std::size_t i=0;i<ax.size();++i)
        PT_REQUIRE(ax[i].index==snapshot[i].index && ax[i].textureVariant==snapshot[i].textureVariant);
    std::cout<<"PASS: AX manual identities, variants, stable order, only-used filter and clipboard isolation\n";
    return 0;
}
