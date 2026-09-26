#include "MapDocument.h"
#include "EditHistory.h"
#include <pugixml.hpp>
#include <filesystem>
#include <fstream>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

static size_t Count(pugi::xml_node parent, const char* tag) {
    size_t count=0; for (auto n:parent.children(tag)) { (void)n; ++count; } return count;
}
static bool Equal(float a, float b) { return std::abs(a-b)<0.001f; }
static int Fail(int checkpoint) {
    std::cerr << "FAIL FunctionalMapRegression checkpoint " << checkpoint << "\n";
    return checkpoint;
}
int main() {
    const auto folder=std::filesystem::temp_directory_path()/"gtanks-next-functional-regression";
    std::error_code ec; std::filesystem::create_directories(folder,ec);
    const auto input=folder/"input.xml", output=folder/"output.xml", second=folder/"output-again.xml";
    const char* content=R"XML(<map version="1.0.Light">
<static-geometry><prop library-name="Land Tiles" group-name="default" name="Tile"><rotation><z>0.000000</z></rotation><texture-name/><position><x>0.000</x><y>0.000</y><z>0.000</z></position></prop></static-geometry>
<collision-geometry><collision-plane><position><x>5</x></position></collision-plane></collision-geometry>
<spawn-points><spawn-point type="red" extra="preserve"><rotation><z>1.500</z></rotation><position><x>100</x><y>0</y><z>0</z></position></spawn-point><spawn-point type="dm"><position><x>200</x><y>0</y><z>0</z></position></spawn-point></spawn-points>
<bonus-regions><bonus-region name="free-bonus" parachute="true" free="true"><rotation><z>0.000</z></rotation><min><x>0</x><y>0</y><z>0</z></min><max><x>500</x><y>500</y><z>300</z></max><position><x>0</x><y>0</y><z>0</z></position><bonus-type>crystal_100</bonus-type><game-mode>ctf</game-mode><game-mode>dm</game-mode></bonus-region></bonus-regions>
<special-geometry><special-box type="special-geometry"><minX>0</minX><minY>0</minY><minZ>-200</minZ><maxX>500</maxX><maxY>500</maxY><maxZ>200</maxZ><action>kill</action></special-box></special-geometry>
<way-points><way-point custom="untouched"/></way-points>
<ctf-flags><flag-red><x>0</x><y>100</y><z>0</z></flag-red><flag-blue><x>500</x><y>100</y><z>0</z></flag-blue></ctf-flags>
<dom-keypoints><dom-keypoint name="A" free="false" distance="1000"><position><x>500</x><y>500</y><z>300</z></position></dom-keypoint></dom-keypoints>
</map>)XML";
    {std::ofstream stream(input,std::ios::binary);stream<<content;}
    MapDocument map;std::string error;
    if(!map.Load(input,error)){std::cerr<<error<<'\n';return Fail(1);}
    if(map.CtfFlags().size()!=2||map.Spawns().size()!=2||map.Bonuses().size()!=1||map.ControlPoints().size()!=1||map.SpecialBoxes().size()!=1)return Fail(2);
    if(!map.SetFlagPosition(0,{123,456,789})||!map.DeleteFlag(1)||!map.AddFlag("blue",{1500,500,0}))return Fail(3);
    if(!map.DeleteSpawn(1))return Fail(4);
    SpawnMarker spawn;spawn.type="dm";spawn.position={700,800,900};map.AddSpawn(spawn);
    auto point=map.ControlPoints()[0];point.position={111,222,333};point.distance=750;if(!map.SetControlPoint(0,point))return Fail(5);
    auto bonus=map.Bonuses()[0];bonus.min={10,20,30};bonus.max={510,520,330};bonus.name="edited-drop";bonus.bonusType="armorup";bonus.modes={"ctf"};bonus.free=false;bonus.parachute=false;if(!map.SetBonusRegion(0,bonus))return Fail(6);
    BonusRegionMarker newBonus;newBonus.name="free-bonus";newBonus.bonusType="armorup";newBonus.modes={"dm"};newBonus.min={100,100,0};newBonus.max={400,400,300};map.AddBonusRegion(newBonus);
    if(!map.DeleteSpecialBox(0))return Fail(7);
    SpecialBox zone;zone.action="kick";zone.min={-200,-200,-200};zone.max={300,300,400};map.AddSpecialBox(zone);
    if(!map.SaveLegacyAs(output,error)){std::cerr<<error<<'\n';return Fail(8);}
    pugi::xml_document doc;if(!doc.load_file(output.c_str()))return Fail(9);
    const auto root=doc.child("map");
    if(Count(root.child("static-geometry"),"prop")!=1||Count(root.child("collision-geometry"),"collision-plane")!=1)return Fail(10);
    if(!root.child("way-points").child("way-point").attribute("custom")||Count(root.child("ctf-flags"),"flag-blue")!=1)return Fail(11);
    if(!Equal(root.child("ctf-flags").child("flag-red").child("x").text().as_float(),123))return Fail(12);
    const auto spawns=root.child("spawn-points");
    if(Count(spawns,"spawn-point")!=2||std::string(spawns.child("spawn-point").attribute("extra").as_string())!="preserve")return Fail(13);
    if(!Equal(spawns.last_child().child("position").child("x").text().as_float(),700))return Fail(14);
    const auto bonuses=root.child("bonus-regions");
    if(Count(bonuses,"bonus-region")!=2||Count(bonuses.child("bonus-region"),"game-mode")!=1 ||
       std::string(bonuses.child("bonus-region").attribute("name").as_string())!="edited-drop" ||
       std::string(bonuses.child("bonus-region").child("bonus-type").text().as_string())!="armorup" ||
       std::string(bonuses.child("bonus-region").child("game-mode").text().as_string())!="ctf" ||
       bonuses.child("bonus-region").attribute("free").as_bool() ||
       bonuses.child("bonus-region").attribute("parachute").as_bool())return Fail(15);
    if(!Equal(bonuses.child("bonus-region").child("position").child("x").text().as_float(),10))return Fail(16);
    const auto added=bonuses.last_child();
    if(!added.child("position")||!added.child("rotation")||std::string(added.child("bonus-type").text().as_string())!="armorup")return Fail(17);
    if(Count(root.child("special-geometry"),"special-box")!=1||std::string(root.child("special-geometry").child("special-box").child("action").text().as_string())!="kick")return Fail(18);
    if(std::string(root.child("dom-keypoints").child("dom-keypoint").attribute("name").as_string())!="A")return Fail(19);
    // Verify source indexes are rebased after saving and a second edit/save roundtrip remains valid.
    if(!map.SetFlagPosition(0,{222,456,789})||!map.SaveLegacyAs(second,error)){std::cerr<<error<<'\n';return Fail(20);}
    MapDocument reread;if(!reread.Load(second,error)||!Equal(reread.CtfFlags()[0].position.x,222))return Fail(21);
    // A group move is one undo/redo transaction; both objects retain their positions.
    PropInstance extra;extra.library="Land Tiles";extra.group="default";extra.name="Tile";extra.position={500,0,0};const auto newIndex=reread.AddProp(extra);
    EditHistory history;const auto a=reread.Props()[0].position,b=reread.Props()[newIndex].position;
    PropTransformState a0{a,{}},a1{{a.x+500,a.y,a.z},{}},b0{b,{}},b1{{b.x+500,b.y,b.z},{}};
    reread.SetPropTransform(0,a1.position,{});reread.SetPropTransform(newIndex,b1.position,{});
    history.PushBatch({{0,a0,a1,"Group move"},{newIndex,b0,b1,"Group move"}});
    std::vector<size_t> changed;
    if(!history.Undo(reread,changed)||changed.size()!=2||!Equal(reread.Props()[0].position.x,a.x)||!Equal(reread.Props()[newIndex].position.x,b.x))return Fail(22);
    if(!history.Redo(reread,changed)||changed.size()!=2||!Equal(reread.Props()[newIndex].position.x,b.x+500))return Fail(23);
    // Structural operations must be fully reversible: deletion, add, undo, redo.
    // This protects the original XML source reference and unsupported legacy sections.
    MapDocument structural;
    if (!structural.Load(input,error)) return Fail(24);
    EditHistory structuralHistory;
    std::vector<size_t> structuralChanges;
    MapDocument beforeDelete=structural;
    if (!structural.DeleteProp(0) || !structural.Props().empty()) return Fail(25);
    structuralHistory.PushSnapshot(std::move(beforeDelete),structural);
    PropInstance addedProp;
    addedProp.library="Land Tiles"; addedProp.group="default"; addedProp.name="Tile";
    addedProp.position={275,125,-75};
    MapDocument beforeAdd=structural;
    structural.AddProp(addedProp);
    structuralHistory.PushSnapshot(std::move(beforeAdd),structural);
    if (!structuralHistory.Undo(structural,structuralChanges) || !structural.Props().empty()) return Fail(26);
    if (!structuralHistory.Undo(structural,structuralChanges) || structural.Props().size()!=1 ||
        !Equal(structural.Props()[0].position.x,0)) return Fail(27);
    if (!structuralHistory.Redo(structural,structuralChanges) || !structural.Props().empty()) return Fail(28);
    if (!structuralHistory.Redo(structural,structuralChanges) || structural.Props().size()!=1 ||
        !Equal(structural.Props()[0].position.x,275)) return Fail(29);
    const auto structuralOut=folder/"structural.xml";
    if (!structural.SaveLegacyAs(structuralOut,error)) {std::cerr<<error<<'\n';return Fail(30);}
    pugi::xml_document structured;
    if (!structured.load_file(structuralOut.c_str()) ||
        Count(structured.child("map").child("static-geometry"),"prop")!=1 ||
        Count(structured.child("map").child("collision-geometry"),"collision-plane")!=1 ||
        !structured.child("map").child("way-points").child("way-point").attribute("custom")) return Fail(31);
    if (!Equal(structured.child("map").child("static-geometry").child("prop")
        .child("position").child("x").text().as_float(),275)) return Fail(32);

    MapDocument blank;
    blank.CreateBlank("1.0.Light");
    if (!blank.Dirty() || blank.Props().size()!=0 || blank.Path()!=std::filesystem::path{}) return Fail(33);
    const auto blankOut=folder/"blank.xml";
    if (!blank.SaveLegacyAs(blankOut,error)) {std::cerr<<error<<'\n';return Fail(34);}
    pugi::xml_document empty;
    if (!empty.load_file(blankOut.c_str()) ||
        !empty.child("map").child("static-geometry") ||
        !empty.child("map").child("spawn-points") ||
        !empty.child("map").child("special-geometry") || blank.Dirty()) return Fail(35);
    // Native gameplay authoring: an explicitly selected TDM+CTF drop must not
    // accidentally acquire DM, and free/parachute + spawn yaw survive save/load.
    MapDocument authored;
    authored.CreateBlank("1.0.Light");
    BonusRegionMarker onlySelected;
    onlySelected.name="free-bonus";onlySelected.bonusType="damageup";
    onlySelected.min={0,0,0};onlySelected.max={500,500,300};
    onlySelected.modes={"tdm","ctf"};onlySelected.free=false;onlySelected.parachute=false;
    authored.AddBonusRegion(onlySelected);
    SpawnMarker heading;heading.type="dm";heading.rotationZ=0.7853981633974483f;
    authored.AddSpawn(heading);
    const auto explicitPath=folder/"explicit-mode-bonus.xml";
    if(!authored.SaveLegacyAs(explicitPath,error)){std::cerr<<error<<'\n';return Fail(101);}
    pugi::xml_document selectedXml;
    if(!selectedXml.load_file(explicitPath.c_str()))return Fail(102);
    const auto nativeDrop=selectedXml.child("map").child("bonus-regions").child("bonus-region");
    if(Count(nativeDrop,"game-mode")!=2 ||
       std::string(nativeDrop.child("game-mode").text().as_string())!="tdm" ||
       std::string(nativeDrop.child("game-mode").next_sibling("game-mode").text().as_string())!="ctf" ||
       nativeDrop.attribute("free").as_bool() || nativeDrop.attribute("parachute").as_bool() ||
       std::string(nativeDrop.child("bonus-type").text().as_string())!="damageup")return Fail(103);
    MapDocument reloadedExplicit;
    if(!reloadedExplicit.Load(explicitPath,error) || reloadedExplicit.Bonuses().size()!=1 ||
       reloadedExplicit.Bonuses()[0].modes!=onlySelected.modes ||
       reloadedExplicit.Bonuses()[0].free || reloadedExplicit.Bonuses()[0].parachute ||
       std::abs(reloadedExplicit.Spawns()[0].rotationZ-0.7853981633974483f)>0.00001f)return Fail(104);

    // The fixture includes the required static geometry section: this test
    // checks preservation of an omitted game-mode, not malformed map XML.
    // A legacy file with NO game-mode children is preserved verbatim as
    // unspecified, not silently rewritten to DM when another property changes.
    const auto unspecifiedInput=folder/"unknown-mode-input.xml";
    const auto unspecifiedOutput=folder/"unknown-mode-output.xml";
    {std::ofstream file(unspecifiedInput,std::ios::binary);
     file<<R"XML(<map version="1.0.Light"><static-geometry/><collision-geometry/><bonus-regions><bonus-region name="free-bonus"><min/><max/><bonus-type>nitro</bonus-type></bonus-region></bonus-regions></map>)XML";}
    MapDocument unspecified;
    if(!unspecified.Load(unspecifiedInput,error)||unspecified.Bonuses().size()!=1||
       !unspecified.Bonuses()[0].modes.empty())return Fail(105);
    auto originalBonus=unspecified.Bonuses()[0];originalBonus.free=false;
    if(!unspecified.SetBonusRegion(0,originalBonus)||
       !unspecified.SaveLegacyAs(unspecifiedOutput,error))return Fail(106);
    pugi::xml_document unspecifiedXml;
    if(!unspecifiedXml.load_file(unspecifiedOutput.c_str()) ||
       Count(unspecifiedXml.child("map").child("bonus-regions").child("bonus-region"),"game-mode")!=0)return Fail(107);

    // Zone editing is not just a viewport visual: mutate, duplicate, persist,
    // reload, and verify native special-geometry values + undo/redo snapshots.
    MapDocument zones;
    if(!zones.Load(input,error) || zones.SpecialBoxes().size()!=1)return Fail(36);
    EditHistory zoneHistory;std::vector<size_t> zoneChanged;
    const auto zoneBefore=zones.SpecialBoxes()[0];
    auto editedZone=zones.SpecialBoxes()[0];
    editedZone.min={-125.0f,-225.0f,-325.0f};
    editedZone.max={400.0f,500.0f,600.0f};
    editedZone.action="kick";editedZone.free=true;
    if(!zones.SetSpecialBox(0,editedZone))return Fail(37);
    zoneHistory.PushZone(0,zoneBefore,editedZone);
    MapDocument beforeDuplicate=zones;
    auto duplicate=editedZone;duplicate.min.x+=100;duplicate.max.x+=100;
    zones.AddSpecialBox(duplicate);
    zoneHistory.PushSnapshot(std::move(beforeDuplicate),zones);
    if(zones.SpecialBoxes().size()!=2)return Fail(38);
    if(!zoneHistory.Undo(zones,zoneChanged)||zones.SpecialBoxes().size()!=1)return Fail(39);
    if(!zoneHistory.Undo(zones,zoneChanged)||zones.SpecialBoxes()[0].action!="kill")return Fail(40);
    if(!zoneHistory.Redo(zones,zoneChanged)||zones.SpecialBoxes()[0].action!="kick")return Fail(41);
    if(!zoneHistory.Redo(zones,zoneChanged)||zones.SpecialBoxes().size()!=2)return Fail(42);
    const auto zoneOutput=folder/"zone-edits.xml";
    if(!zones.SaveLegacyAs(zoneOutput,error)){std::cerr<<error<<'\n';return Fail(43);}
    pugi::xml_document zoneXml;
    if(!zoneXml.load_file(zoneOutput.c_str()))return Fail(44);
    const auto zoneNode=zoneXml.child("map").child("special-geometry").child("special-box");
    if(Count(zoneXml.child("map").child("special-geometry"),"special-box")!=2 ||
       !Equal(zoneNode.child("minX").text().as_float(),-125.0f) ||
       !Equal(zoneNode.child("maxZ").text().as_float(),600.0f) ||
       std::string(zoneNode.child("action").text().as_string())!="kick" ||
       !zoneNode.attribute("free").as_bool() ||
       !zoneXml.child("map").child("way-points").child("way-point").attribute("custom"))return Fail(45);
    MapDocument zoneReload;
    if(!zoneReload.Load(zoneOutput,error)||zoneReload.SpecialBoxes().size()!=2||
        !zoneReload.SpecialBoxes()[0].free || zoneReload.SpecialBoxes()[0].action!="kick")return Fail(46);
    // A successful SaveAs must install a fresh immutable source XML buffer;
    // an earlier MapDocument snapshot still serializes its original source.
    MapDocument untouched; if(!untouched.Load(input,error))return Fail(47);
    MapDocument originalSnapshot=untouched;
    auto change=untouched.SpecialBoxes()[0];change.action="kick";
    if(!untouched.SetSpecialBox(0,change))return Fail(48);
    const auto mutated=folder/"shared-xml-edited.xml",original=folder/"shared-xml-original.xml";
    if(!untouched.SaveLegacyAs(mutated,error) || !originalSnapshot.SaveLegacyAs(original,error))return Fail(49);
    pugi::xml_document mutatedDoc,originalDoc;
    if(!mutatedDoc.load_file(mutated.c_str())||!originalDoc.load_file(original.c_str()))return Fail(50);
    if(std::string(mutatedDoc.child("map").child("special-geometry").child("special-box").child("action").text().as_string())!="kick" ||
       std::string(originalDoc.child("map").child("special-geometry").child("special-box").child("action").text().as_string())!="kill")return Fail(51);
    // Clipboard regression: native properties must survive a NEW element's source-index reset,
    // serialization and a fresh file load. Do not reinitialize bonuses from palette defaults.
    MapDocument copied;
    if(!copied.Load(input,error))return Fail(108);
    auto copiedBonus=copied.Bonuses().front();copiedBonus.legacySourceIndex=-1;
    copiedBonus.min.x+=1000;copiedBonus.max.x+=1000;
    copied.AddBonusRegion(copiedBonus);
    auto copiedSpawn=copied.Spawns().front();copiedSpawn.legacySourceIndex=-1;
    copiedSpawn.position.x+=1000;copied.AddSpawn(copiedSpawn);
    auto copiedZone=copied.SpecialBoxes().front();copiedZone.legacySourceIndex=-1;
    copiedZone.min.x+=1000;copiedZone.max.x+=1000;copied.AddSpecialBox(copiedZone);
    const auto clipboardOutput=folder/"clipboard-native.xml";
    if(!copied.SaveLegacyAs(clipboardOutput,error))return Fail(109);
    MapDocument pasted;
    if(!pasted.Load(clipboardOutput,error)||pasted.Bonuses().size()!=2||
       pasted.Spawns().size()!=3||pasted.SpecialBoxes().size()!=2)return Fail(110);
    const auto& pastedBonus=pasted.Bonuses().back();
    if(pastedBonus.bonusType!=copiedBonus.bonusType||pastedBonus.name!=copiedBonus.name||
       pastedBonus.modes!=copiedBonus.modes||pastedBonus.free!=copiedBonus.free||
       pastedBonus.parachute!=copiedBonus.parachute||!Equal(pastedBonus.min.x,copiedBonus.min.x)||
       !Equal(pastedBonus.max.x,copiedBonus.max.x))return Fail(111);
    const auto& spawnCopy=pasted.Spawns().back();
    if(!Equal(spawnCopy.rotationZ,copiedSpawn.rotationZ)||
       spawnCopy.type!=copiedSpawn.type||spawnCopy.team!=copiedSpawn.team||
       !Equal(spawnCopy.position.x,copiedSpawn.position.x))return Fail(112);
    const auto& specialCopy=pasted.SpecialBoxes().back();
    if(specialCopy.action!=copiedZone.action||specialCopy.free!=copiedZone.free||
       !Equal(specialCopy.max.x,copiedZone.max.x))return Fail(113);
    pugi::xml_document pastedXml;
    if(!pastedXml.load_file(clipboardOutput.c_str())||
       Count(pastedXml.child("map").child("bonus-regions").last_child(),"game-mode")!=2)return Fail(114);
    std::filesystem::remove_all(folder,ec);
    std::cout<<"Functional XML, compact zone undo/redo, shared-source isolation and blank map regression OK\n";
    return 0;
}
