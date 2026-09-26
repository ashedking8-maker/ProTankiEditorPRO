#include "MapDocument.h"
#include "EditHistory.h"
#include <pugixml.hpp>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

namespace {
bool eq(float x,float y){return std::fabs(x-y)<0.002f;}
size_t count(pugi::xml_node parent,const char* tag){size_t n=0;for(auto v:parent.children(tag)){(void)v;++n;}return n;}
bool check(bool ok,const char* message){if(!ok)std::cerr<<"FAIL native light: "<<message<<'\n';return ok;}
std::string bytes(const std::filesystem::path& file){std::ifstream in(file,std::ios::binary);return {std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>()};}
}
int main(){
    // Real Fogtown field names and one real lamp light position/color. Unknown
    // extension data must remain untouched when the native light is edited.
    const char* original=R"XML(<map version="1.0.Light"><static-geometry/><collision-geometry/><spawn-points/><bonus-regions/><special-geometry/><lights><extension anchor="before"/><light type="omni" color="16771986" intensity="1" attenuationBegin="0.1" attenuationEnd="20" extraLegacy="preserve"><rotation><z>0</z></rotation><position><x>-9250.000</x><y>-4250.000</y><z>600.000</z></position><future-marker preserved="yes"/></light><light type="future" color="16711687" intensity="0.5"><original-data keep="true"/></light><extension anchor="after"/></lights><way-points><way-point custom="untouched"/></way-points></map>)XML";
    const auto folder=std::filesystem::temp_directory_path()/"ptpro-native-light-regression";
    std::error_code ec;std::filesystem::create_directories(folder,ec);
    if(!check(!ec,"create test directory"))return 1;
    const auto input=folder/"light-original.xml",unmodified=folder/"light-unmodified.xml",out=folder/"light-edited.xml",second=folder/"light-second.xml",deleted=folder/"light-deleted.xml",blank=folder/"light-blank.xml";
    {std::ofstream stream(input,std::ios::binary|std::ios::trunc);stream<<original;if(!check(bool(stream),"write fixture"))return 2;}
    MapDocument map;std::string error;
    if(!check(map.Load(input,error),"load Fogtown-schema fixture"))return 3;
    if(!check(map.Stats().lights==2&&map.Lights().size()==2,"both native and unknown light records parsed"))return 4;
    if(!check(map.Lights()[0].type=="omni"&&map.Lights()[0].color==16771986u&&eq(map.Lights()[0].position.x,-9250.f),"original position and packed decimal RGB"))return 5;
    if(!check(map.SaveLegacyAs(unmodified,error),"save without edits"))return 6;
    if(!check(bytes(input)==bytes(unmodified),"unmodified input bytes preserved"))return 7;
    MapDocument before=map;
    auto editable=map.Lights()[0];editable.color=0x3366AAu;editable.intensity=0.75f;editable.position.z=750.f;editable.attenuationEnd=15.f;
    if(!check(map.SetLight(0,editable),"edit supported omni light"))return 8;
    LightMarker invalid=editable;invalid.attenuationEnd=invalid.attenuationBegin;
    if(!check(!map.SetLight(0,invalid),"reject reversed fade interval"))return 9;
    invalid.type="unverified";
    if(!check(map.AddLight(invalid)==map.Lights().size(),"reject invented light types"))return 10;
    LightMarker added;added.position={123.f,-456.f,789.f};added.color=0xFF8800u;
    if(!check(map.AddLight(added)==2,"append new independent native light"))return 11;
    EditHistory history;history.PushSnapshot(std::move(before),map);
    std::vector<size_t> touched;
    if(!check(history.Undo(map,touched)&&map.Lights().size()==2&&eq(map.Lights()[0].position.z,600.f),"undo native light edit"))return 12;
    if(!check(history.Redo(map,touched)&&map.Lights().size()==3&&eq(map.Lights()[0].position.z,750.f),"redo native light edit"))return 13;
    if(!check(map.SaveLegacyAs(out,error),"save modified map"))return 14;
    pugi::xml_document document;
    if(!check(bool(document.load_file(out.c_str())),"read saved XML"))return 15;
    const auto root=document.child("map"),lights=root.child("lights"),first=lights.child("light"),future=first.next_sibling("light"),newLight=future.next_sibling("light");
    if(!check(count(lights,"light")==3&&map.Stats().lights==3,"three saved light records"))return 16;
    if(!check(std::string(lights.first_child().name())=="extension"&&
              std::string(lights.last_child().name())=="light"&&
              std::string(future.next_sibling().name())=="extension"&&
              std::string(future.next_sibling().attribute("anchor").as_string())=="after",
              "preserve unknown sibling order when editing/adding lights"))return 28;
    if(!check(first.attribute("extraLegacy")&&first.child("future-marker").attribute("preserved"),"unknown attributes and children remain on edited light"))return 17;
    if(!check(first.attribute("color").as_uint()==0x3366AAu&&eq(first.attribute("intensity").as_float(),0.75f)&&eq(first.child("position").child("z").text().as_float(),750.f),"edit serialized in original fields"))return 18;
    if(!check(std::string(future.attribute("type").as_string())=="future"&&future.child("original-data").attribute("keep"),"unknown light type passes through"))return 19;
    if(!check(std::string(newLight.attribute("type").as_string())=="omni"&&eq(newLight.child("position").child("x").text().as_float(),123.f),"new light native XML"))return 20;
    if(!check(bool(root.child("way-points").child("way-point").attribute("custom")),"unrelated legacy sections untouched"))return 21;
    // Test rebasing of legacy indices after the first save. Old index -1 must
    // not cause loss of a custom light when another light is edited afterward.
    editable=map.Lights()[2];editable.position.y=-654.f;
    if(!check(map.SetLight(2,editable)&&map.SaveLegacyAs(second,error),"second edit and save"))return 22;
    MapDocument reread;
    if(!check(reread.Load(second,error)&&reread.Lights().size()==3&&eq(reread.Lights()[2].position.y,-654.f),"second round-trip retains correct light"))return 23;
    if(!check(reread.DeleteLight(0)&&reread.SaveLegacyAs(deleted,error),"delete one light"))return 24;
    MapDocument final;
    if(!check(final.Load(deleted,error)&&final.Lights().size()==2&&final.Lights()[0].type=="future"&&eq(final.Lights()[1].position.y,-654.f),"deletion preserves remaining record identity"))return 25;
    MapDocument empty;empty.CreateBlank();empty.AddLight(added);
    if(!check(empty.SaveLegacyAs(blank,error),"blank map supports native lights"))return 26;
    pugi::xml_document blankDoc;
    if(!check(bool(blankDoc.load_file(blank.c_str()))&&count(blankDoc.child("map").child("lights"),"light")==1,"blank map light saved"))return 27;
    std::cout<<"PASS native light XML edit, add, delete, undo, unknown node preservation and repeated save\n";
    return 0;
}
