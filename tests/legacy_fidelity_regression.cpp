#include "MapDocument.h"
#include <pugixml.hpp>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <iostream>
#include <string>

namespace fs=std::filesystem;
static int Fail(int step,const std::string& why) {
    std::cerr<<"FAIL LegacyFidelityRegression checkpoint "<<step<<": "<<why<<'\n';return step;
}
int main() {
    const fs::path root=fs::temp_directory_path()/"protanki-0516-fidelity";
    std::error_code ec;fs::create_directories(root,ec);
    const fs::path source=root/"original.xml",saved=root/"saved.xml",again=root/"saved-again.xml";
    struct Clean {fs::path root;~Clean(){std::error_code e;fs::remove_all(root,e);}} clean{root};
    const char* fixture=R"XML(<map version="1.0.Light" custom-root="keep-me">
      <static-geometry provenance="untouched">
        <prop library-name="Concrete Tiles" group-name="default" name="Pave 1x1" free="true" legacy-custom="abc">
          <rotation><z>0</z></rotation><texture-name>Pave 1</texture-name>
          <position><x>0</x><y>0</y><z>0</z></position>
          <with_collision>1</with_collision><game-extension priority="7"><value>retained</value></game-extension>
        </prop>
        <legacy-geometry-extension id="between"/>
        <prop library-name="dust2" group-name="roofs" name="rstr">
          <rotation><z>0</z></rotation><texture-name>RT Str</texture-name>
          <position><x>500</x><y>0</y><z>0</z></position><with_collision>0</with_collision>
        </prop>
      </static-geometry>
      <collision-geometry><native-extension id="not-ours"/></collision-geometry>
      <spawn-points/><ctf-flags/><dom-keypoints/><bonus-regions/><special-geometry/><lights/><way-points/>
      <unknown-gameplay payload="unchanged"><data x="9"/></unknown-gameplay>
    </map>)XML";
    {std::ofstream out(source,std::ios::binary);out<<fixture;}
    std::string error;MapDocument m;
    if(!m.Load(source,error))return Fail(1,error);
    // No-op Save As is byte-for-byte, including untouched unsupported XML.
    const auto noChange=root/"noop-exact.xml";
    MapDocument untouched;
    if(!untouched.Load(source,error)||!untouched.SaveLegacyAs(noChange,error))return Fail(31,error);
    std::ifstream noChangeInput(noChange,std::ios::binary);
    const std::string noChangeBytes((std::istreambuf_iterator<char>(noChangeInput)),
        std::istreambuf_iterator<char>());
    if(noChangeBytes!=fixture)return Fail(32,"no-op save changed original map bytes");
    if(m.Props().size()!=2||m.Props()[0].nativeWithCollision!=1||m.Props()[0].nativeFree!=1||
       !m.Props()[0].hasUncopyableMetadata||m.Props()[1].hasUncopyableMetadata||
       m.Props()[1].nativeWithCollision!=0||m.Props()[1].nativeFree!=-1)return Fail(2,"original flags not read");
    if(!m.SetPropTransform(0,{250.f,0.f,0.f},{0.f,0.f,0.f}))return Fail(3,"transform");
    auto copy=m.Props()[1];copy.position.x=1500.f;
    m.AddProp(copy); // clipboard uses the same AddProp path, source index becomes -1.
    if(!m.SaveLegacyAs(saved,error))return Fail(4,error);
    pugi::xml_document doc;if(!doc.load_file(saved.c_str()))return Fail(5,"serialized XML invalid");
    auto map=doc.child("map"),section=map.child("static-geometry");
    auto first=section.child("prop"),extension=first.next_sibling(),second=extension.next_sibling(),third=second.next_sibling();
    if(std::string(extension.name())!="legacy-geometry-extension"||
       std::string(extension.attribute("id").value())!="between"||
       std::string(first.attribute("legacy-custom").value())!="abc"||
       std::string(first.attribute("free").value())!="true"||
       std::string(first.child("with_collision").text().as_string())!="1"||
       std::string(first.child("game-extension").child("value").text().as_string())!="retained"||
       std::string(first.child("texture-name").text().as_string())!="Pave 1"||
       first.child("position").child("x").text().as_float()!=250.f)return Fail(6,"retained metadata/order/transform");
    if(std::string(second.child("with_collision").text().as_string())!="0"||
       std::string(third.child("with_collision").text().as_string())!="0"||
       std::string(third.child("texture-name").text().as_string())!="RT Str"||
       third.attribute("free")||third.child("game-extension"))return Fail(7,"copy lost flag or invented unrelated data");
    if(std::string(map.attribute("custom-root").value())!="keep-me"||
       !map.child("unknown-gameplay").child("data")||
       !map.child("collision-geometry").child("native-extension"))return Fail(8,"unknown sections lost");
    MapDocument reload;if(!reload.Load(saved,error))return Fail(9,error);
    if(reload.Props().size()!=3||reload.Props()[2].nativeWithCollision!=0||
       reload.Props()[0].nativeFree!=1)return Fail(10,"reload lost flags");
    if(!reload.SetPropTransform(2,{1750.f,0.f,0.f},{0.f,0.f,0.f})||
       !reload.SaveLegacyAs(again,error))return Fail(11,error);
    pugi::xml_document repeat;if(!repeat.load_file(again.c_str()))return Fail(12,"second serialization invalid");
    if(std::string(repeat.child("map").child("static-geometry").last_child().child("with_collision").text().as_string())!="0")
        return Fail(13,"second save lost clipboard flag");
    MapDocument unsafe;if(!unsafe.Load(source,error))return Fail(14,error);
    auto unsupported=unsafe.Props()[0];unsupported.position.x=5000.f;
    unsafe.AddProp(unsupported);
    const auto stopped=root/"must-not-exist.xml";
    if(unsafe.SaveLegacyAs(stopped,error)||fs::exists(stopped)||
       error.find("unknown source metadata")==std::string::npos)
        return Fail(15,"unsupported original fields silently lost during copy");
    // Deliberate opaque duplication must retain the WHOLE source subtree, not
    // just known fields; the distinct zero flag avoids inventing a collider.
    const auto opaqueSource=root/"opaque-source.xml";
    std::string opaqueXml=fixture;
    const auto flag=opaqueXml.find("<with_collision>1</with_collision>");
    if(flag==std::string::npos)return Fail(17,"fixture flag missing");
    opaqueXml.replace(flag,std::string("<with_collision>1</with_collision>").size(),
        "<with_collision>0</with_collision>");
    {std::ofstream out(opaqueSource,std::ios::binary);out<<opaqueXml;}
    MapDocument opaque;if(!opaque.Load(opaqueSource,error))return Fail(18,error);
    auto approved=opaque.Props()[0];approved.position={9750.f,1250.f,600.f};
    approved.allowOpaqueMetadataCopy=true;
    opaque.AddProp(approved);
    const auto approvedFile=root/"opaque-copied.xml";
    if(!opaque.SaveLegacyAs(approvedFile,error))return Fail(19,error);
    pugi::xml_document approvedDoc;
    if(!approvedDoc.load_file(approvedFile.c_str()))return Fail(20,"opaque XML invalid");
    const auto clone=approvedDoc.child("map").child("static-geometry").last_child();
    if(std::string(clone.attribute("legacy-custom").value())!="abc" ||
       std::string(clone.attribute("free").value())!="true" ||
       std::string(clone.child("game-extension").attribute("priority").value())!="7" ||
       std::string(clone.child("game-extension").child("value").text().as_string())!="retained" ||
       std::string(clone.child("with_collision").text().as_string())!="0" ||
       clone.child("position").child("x").text().as_float()!=9750.f)
        return Fail(21,"explicitly approved full opaque prop copy lost fields/transform");
    MapDocument opaqueReload;if(!opaqueReload.Load(approvedFile,error))return Fail(22,error);
    if(opaqueReload.Props().size()!=3 || !opaqueReload.Props().back().originalPropXml ||
       opaqueReload.Props().back().originalPropXml->find("game-extension")==std::string::npos)
        return Fail(23,"saved opaque instance lost its complete source snapshot");
    auto further=opaqueReload.Props().back();further.position.x=10000.f;
    opaqueReload.AddProp(further);
    if(opaqueReload.SaveLegacyAs(stopped,error) || fs::exists(stopped) ||
       error.find("unknown source metadata")==std::string::npos)
        return Fail(24,"advanced consent leaked into later clipboard operation");
    // Even with explicit opaque consent, a native with_collision=1 may never be
    // duplicated without a separately owned, complete collider set.
    MapDocument nativeFlag;if(!nativeFlag.Load(source,error))return Fail(25,error);
    auto flagged=nativeFlag.Props()[0];flagged.allowOpaqueMetadataCopy=true;
    nativeFlag.AddProp(flagged);
    if(nativeFlag.SaveLegacyAs(stopped,error)||fs::exists(stopped)||
       error.find("no owned collision primitives")==std::string::npos)
        return Fail(26,"opaque-copy opt-in bypassed native collision guard");
    // Known native fields with malformed values are never duplicated, even
    // when a user has opted in to copying unrelated opaque extensions.
    const auto malformedFile=root/"malformed-source.xml";
    std::string malformedXml=opaqueXml;
    const auto badFlag=malformedXml.find("<with_collision>0</with_collision>");
    if(badFlag==std::string::npos)return Fail(27,"malformed fixture missing flag");
    malformedXml.replace(badFlag,std::string("<with_collision>0</with_collision>").size(),
        "<with_collision>unrecognized</with_collision>");
    {std::ofstream out(malformedFile,std::ios::binary);out<<malformedXml;}
    MapDocument malformed;if(!malformed.Load(malformedFile,error))return Fail(28,error);
    if(!malformed.Props()[0].hasInvalidNativeMetadata)return Fail(29,"malformed native flag accepted");
    auto badCopy=malformed.Props()[0];badCopy.allowOpaqueMetadataCopy=true;
    malformed.AddProp(badCopy);
    if(malformed.SaveLegacyAs(stopped,error)||fs::exists(stopped)||
       error.find("malformed or repeated native prop fields")==std::string::npos)
        return Fail(30,"explicit opt-in bypassed malformed native flag guard");
    MapDocument unbound;unbound.CreateBlank();
    PropInstance fake;fake.library="Concrete Tiles";fake.group="default";fake.name="Pave 1x1";
    fake.nativeWithCollision=1;unbound.AddProp(fake);
    if(unbound.SaveLegacyAs(stopped,error)||fs::exists(stopped)||
       error.find("no owned collision primitives")==std::string::npos)
        return Fail(16,"native collision flag accepted without physical geometry");
    std::cout<<"PASS native source snapshots, explicit opaque subtree copy, approval reset, collision guard, repeated saves.\n";
    return 0;
}
