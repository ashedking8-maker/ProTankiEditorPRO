#include "MapDocument.h"
#include <pugixml.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

static size_t Count(pugi::xml_node parent, const char* name) {
    size_t n=0; for (auto ignored : parent.children(name)) { (void)ignored; ++n; } return n;
}

int main() {
    const auto base = std::filesystem::temp_directory_path() / "gtanks-next-map-edit-regression";
    std::error_code ec; std::filesystem::create_directories(base, ec);
    const auto input = base / "input.xml";
    const auto output = base / "output.xml";
    const char* xml = R"XML(<map version="1.0.Light">
  <static-geometry>
    <prop library-name="Lib" group-name="default" name="A"><rotation><z>0.000000</z></rotation><texture-name>T1</texture-name><position><x>0.000</x><y>0.000</y><z>0.000</z></position></prop>
    <prop library-name="Lib" group-name="default" name="B"><rotation><z>1.000000</z></rotation><texture-name/><position><x>500.000</x><y>0.000</y><z>0.000</z></position></prop>
  </static-geometry>
  <collision-geometry><collision-box><position><x>0</x><y>0</y><z>0</z></position></collision-box></collision-geometry>
  <spawn-points><spawn-point type="dm"><rotation><z>1.570796</z></rotation><position><x>0</x><y>0</y><z>0</z></position></spawn-point><spawn-point type="dom" team="blue"><position><x>500</x><y>0</y><z>100</z></position></spawn-point></spawn-points>
  <bonus-regions><bonus-region name="x"><min><x>0</x><y>0</y><z>0</z></min><max><x>500</x><y>500</y><z>300</z></max><bonus-type>armorup</bonus-type><game-mode>dm</game-mode><game-mode>ctf</game-mode></bonus-region></bonus-regions>
  <special-geometry><special-box><minX>-10</minX><minY>-10</minY><minZ>-10</minZ><maxX>10</maxX><maxY>10</maxY><maxZ>10</maxZ><action>kill</action></special-box></special-geometry>
  <way-points><way-point/></way-points>
  <dom-keypoints><dom-keypoint name="A" distance="1000"><position><x>500</x><y>500</y><z>300</z></position></dom-keypoint></dom-keypoints>
  <ctf-flags><flag-red><x>-1</x><y>0</y><z>0</z></flag-red><flag-blue><x>1</x><y>0</y><z>0</z></flag-blue></ctf-flags>
</map>)XML";
    { std::ofstream f(input, std::ios::binary); f << xml; }

    MapDocument map; std::string error;
    if (!map.Load(input,error)) { std::cerr << "Load failed: " << error << '\n'; return 1; }
    if (map.Props().size()!=2 || map.SpecialBoxes().size()!=1 || map.CtfFlags().size()!=2) return 2;
    if (map.Spawns().size()!=2 || map.Spawns()[0].type!="dm" || map.Spawns()[1].team!="blue") return 21;
    if (map.Bonuses().size()!=1 || map.Bonuses()[0].modes.size()!=2 || map.Bonuses()[0].bonusType!="armorup") return 22;
    if (map.ControlPoints().size()!=1 || map.ControlPoints()[0].name!="A" || map.ControlPoints()[0].distance!=1000.0f) return 23;
    if (!map.DeleteProp(0)) return 3;
    PropInstance add; add.library="Lib2"; add.group="default"; add.name="C"; add.texture="Variant"; add.position={1000,2000,300}; add.rotation={0,0,1.570796f};
    map.AddProp(add);
    if (!map.SaveLegacyAs(output,error)) { std::cerr << "Save failed: " << error << '\n'; return 4; }

    pugi::xml_document doc;
    if (!doc.load_file(output.c_str())) return 5;
    auto root=doc.child("map"); auto geom=root.child("static-geometry");
    if (Count(geom,"prop")!=2) return 6;
    auto first=geom.child("prop"); auto second=first.next_sibling("prop");
    if (std::string(first.attribute("name").as_string())!="B") return 7;
    if (std::string(second.attribute("name").as_string())!="C") return 8;
    if (std::string(second.attribute("library-name").as_string())!="Lib2") return 9;
    if (std::string(second.child("texture-name").text().as_string())!="Variant") return 10;
    if (Count(root.child("collision-geometry"),"collision-box")!=1) return 11;
    if (Count(root.child("special-geometry"),"special-box")!=1) return 12;
    if (Count(root.child("spawn-points"),"spawn-point")!=2) return 13;
    if (Count(root.child("ctf-flags"),"flag-red")!=1 || Count(root.child("ctf-flags"),"flag-blue")!=1) return 14;

    if (Count(root.child("bonus-regions"),"bonus-region")!=1) return 15;
    if (Count(root.child("dom-keypoints"),"dom-keypoint")!=1) return 16;
    if (Count(root.child("bonus-regions").child("bonus-region"),"game-mode")!=2) return 17;

    // Overwriting the loaded original must retain a byte-for-byte backup once.
    if (!map.SetPropTransform(0,{650,0,0},{0,0,1.0f})) return 18;
    if (!map.SaveLegacyAs(input,error)) {std::cerr<<error;return 19;}
    auto backup=input;backup+=L".original.bak";
    std::ifstream backupFile(backup,std::ios::binary);
    if(!backupFile || std::string(std::istreambuf_iterator<char>(backupFile),
                                  std::istreambuf_iterator<char>())!=xml) return 20;
    if (!map.SetPropTransform(0,{750,0,0},{0,0,1.0f}) || !map.SaveLegacy(error)) return 24;
    std::ifstream stillOriginal(backup,std::ios::binary);
    if(!stillOriginal || std::string(std::istreambuf_iterator<char>(stillOriginal),
                                     std::istreambuf_iterator<char>())!=xml) return 25;

    std::filesystem::remove_all(base, ec);
    std::cout << "Map edit regression OK\n";
    return 0;
}
