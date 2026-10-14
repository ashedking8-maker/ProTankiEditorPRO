#include "BrowseCustomFilters.h"
#include "ReleaseTestCheck.h"
#include <iostream>
namespace fs=std::filesystem;
int main() {
    using namespace BrowseCustomFilters;
    const auto root=fs::temp_directory_path()/("ptpro_custom_filters_"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    PT_REQUIRE(fs::create_directories(root));
    struct Clean {fs::path path;~Clean(){std::error_code ec;fs::remove_all(path,ec);}} clean{root};
    const auto file=root/"filters.xml";std::string error;
    std::set<Key> keys;
    for(int i=0;i<5000;++i)keys.insert({"Winter & town","walls","wall_"+std::to_string(i),"snow"});
    keys.insert({"Winter & town","walls","wall_0","stone"});
    Store store;PT_REQUIRE(store.Add("Moje stromy & skaly",keys,error));
    PT_REQUIRE(store.Filters()[0].assets.size()==5001);
    PT_REQUIRE(store.Filters()[0].Contains({"WINTER & TOWN","WALLS","WALL_42","SNOW"}));
    PT_REQUIRE(!store.Filters()[0].Contains({"winter & town","walls","wall_42","stone"}));
    PT_REQUIRE(!store.Add("  moje stromy & skaly ",keys,error));
    PT_REQUIRE(!store.Add("empty",{},error));
    PT_REQUIRE(store.Save(file,error));
    Store loaded;PT_REQUIRE(loaded.Load(file,error));
    PT_REQUIRE(loaded.Filters().size()==1 && loaded.Filters()[0].name==store.Filters()[0].name);
    PT_REQUIRE(loaded.Filters()[0].assets==store.Filters()[0].assets);
    PT_REQUIRE(loaded.Add("Second",{{"Other library","","Object",""}},error));
    PT_REQUIRE(loaded.Save(file,error)); // replace existing settings atomically
    Store again;PT_REQUIRE(again.Load(file,error) && again.Filters().size()==2);
    PT_REQUIRE(again.Filters()[1].Contains({"other LIBRARY","","object",""}));
    PT_REQUIRE(!again.Save({},error));
    PT_REQUIRE(!again.Save(root,error)); // destination is a directory; preserve file
    Store intact;PT_REQUIRE(intact.Load(file,error)&&intact.Filters().size()==2);
    {std::ofstream corrupt(root/"bad.xml");corrupt<<"<broken>";}
    PT_REQUIRE(!again.Load(root/"bad.xml",error));PT_REQUIRE(again.Filters().size()==2);
    PT_REQUIRE(!again.Erase(99,error));PT_REQUIRE(again.Filters().size()==2);
    PT_REQUIRE(again.Erase(0,error)&&again.Save(file,error));
    PT_REQUIRE(intact.Load(file,error)&&intact.Filters().size()==1&&intact.Filters()[0].name=="Second");
    PT_REQUIRE(again.Erase(0,error)&&again.Save(file,error));
    PT_REQUIRE(intact.Load(file,error)&&intact.Filters().empty());
    for(const auto& item:fs::directory_iterator(root))PT_REQUIRE(item.path().extension()!=".tmp");
    std::cout<<"PASS: 5001 stable variant identities, custom filter persistence/replacement, malformed input and deletion\n";
}
