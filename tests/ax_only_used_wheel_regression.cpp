#include "AxRecentFilter.h"
#include "ReleaseTestCheck.h"
#include <iostream>
#include <string>
#include <vector>

struct Asset { std::string library,group,name; };
struct Recent { std::size_t index; };
struct Prop { std::string library,group,name; };
int main() {
    const std::vector<Asset> assets{
        {"Beach","default","never placed"},
        {"Concrete Tiles","default","Concrete 2x2"},
        {"Beach","default","only clicked"},
        {"Industrial Bridge","default","Bridge 2"},
        {"Beach","another group","Concrete 2x2"},
        {"Concrete Tiles","default","Concrete 2x2"}
    };
    const std::vector<Recent> recent{{0},{1},{2},{3},{4},{99},{5}};
    const std::vector<Prop> placed{{"Concrete Tiles","default","Concrete 2x2"},
                                    {"Industrial Bridge","default","Bridge 2"}};
    using namespace AxRecentFilter;
    const auto all=VisiblePositions(recent,assets,placed,false);
    PT_REQUIRE((all==std::vector<std::size_t>{0,1,2,3,4,6})); // invalid ID skipped
    const auto onlyUsed=VisiblePositions(recent,assets,placed,true);
    PT_REQUIRE((onlyUsed==std::vector<std::size_t>{1,3,6})); // group/name/library are exact
    PT_REQUIRE(onlyUsed[static_cast<std::size_t>(ClampRow(0,onlyUsed.size()))]==1);
    int row=0;
    row=NextRow(row,onlyUsed.size(),-1); PT_REQUIRE(onlyUsed[static_cast<std::size_t>(row)]==3);
    row=NextRow(row,onlyUsed.size(),-1); PT_REQUIRE(onlyUsed[static_cast<std::size_t>(row)]==6);
    row=NextRow(row,onlyUsed.size(),-1); PT_REQUIRE(onlyUsed[static_cast<std::size_t>(row)]==1);
    row=NextRow(row,onlyUsed.size(),+1); PT_REQUIRE(onlyUsed[static_cast<std::size_t>(row)]==6);
    PT_REQUIRE(ClampRow(999,onlyUsed.size())==2);
    const auto empty=VisiblePositions(recent,assets,std::vector<Prop>{},true);
    PT_REQUIRE(empty.empty());
    PT_REQUIRE(ClampRow(99,empty.size())==0);
    PT_REQUIRE(NextRow(99,empty.size(),-1)==0);
    PT_REQUIRE(NextRow(99,empty.size(),+1)==0);
    // An object sharing only a name, not group/library, is not 'used'.
    const auto wrong=VisiblePositions(recent,assets,
                                      std::vector<Prop>{{"Elsewhere","default","Concrete 2x2"}},true);
    PT_REQUIRE(wrong.empty());
    std::cout << "PASS: AX used-only list, Tab+wheel wrap, exact identity and empty filter\n";
    return 0;
}
