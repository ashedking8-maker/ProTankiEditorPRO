#include "MapWorkspace.h"
#include "ReleaseTestCheck.h"
#include <pugixml.hpp>
#include <chrono>
#include <set>
#include <iostream>
namespace fs=std::filesystem;
int main(int argc,char**argv) {
    PT_REQUIRE(argc==2);
    const fs::path fixture=fs::path(argv[1])/"source.xml";
    const auto root=fs::temp_directory_path()/("ptpro_tabs_"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    PT_REQUIRE(fs::create_directories(root));
    struct Clean {fs::path p;~Clean(){std::error_code ec;fs::remove_all(p,ec);}} clean{root};
    MapWorkspace workspace;MapDocument current;EditHistory history;std::string error;
    PT_REQUIRE(current.Load(fixture,error));
    const auto sourceId=workspace.ActiveId();
    PT_REQUIRE(workspace.FindPath(fixture,current)==sourceId);
    PT_REQUIRE(workspace.FindPath(fixture.parent_path()/"."/"source.xml",current)==sourceId);
    const auto original=current.Props()[0].position;
    const auto rotation=current.Props()[0].rotation;
    auto moved=original;moved.x+=250;
    PT_REQUIRE(current.SetPropTransform(0,moved,rotation));
    history.Push({0,{original,rotation},{moved,rotation},"source move"});
    // Clipboard owns detached XML and colliders across switching/closing source.
    auto props=current.Props();std::vector<CollisionPlane> planes;
    std::vector<CollisionBox> boxes;std::vector<CollisionTriangle> triangles;
    PT_REQUIRE(current.CopyNativeCollisionForProps({0,1},planes,boxes,triangles));
    MapDocument target;PT_REQUIRE(target.Load(fixture,error));
    PT_REQUIRE(target.SaveLegacyAs(root/"target.xml",error));
    const auto targetId=workspace.Add(std::move(target),current,history);
    PT_REQUIRE(workspace.Size()==2 && !history.CanUndo());
    auto before=current;std::vector<int> inserted;
    PT_REQUIRE(current.AppendLosslessNativeStaticClone(props,planes,boxes,triangles,inserted,error));
    history.PushSnapshot(std::move(before),current);
    PT_REQUIRE(current.Props().size()==4 && current.CollisionPlanes().size()==2);
    PT_REQUIRE(workspace.Activate(sourceId,current,history));
    PT_REQUIRE(current.Props().size()==2 && current.Props()[0].position.x==moved.x);
    std::vector<size_t> changed;PT_REQUIRE(history.Undo(current,changed));
    PT_REQUIRE(current.Props()[0].position.x==original.x);
    PT_REQUIRE(workspace.Activate(targetId,current,history));
    PT_REQUIRE(current.Props().size()==4 && history.CanUndo());
    PT_REQUIRE(current.SaveLegacyAs(root/"renamed.xml",error));
    pugi::xml_document xml;PT_REQUIRE(xml.load_file((root/"renamed.xml").c_str()));
    auto prop=xml.child("map").child("static-geometry").child("prop").next_sibling("prop").next_sibling("prop");
    PT_REQUIRE(std::string(prop.attribute("opaque-attr").value())=="keep-me");
    PT_REQUIRE(std::string(prop.child("future-extension").child("nested").text().as_string())=="opaque-data");
    std::set<std::string> ids;
    for(const auto node:xml.child("map").child("collision-geometry").children())
        if(node.attribute("id"))PT_REQUIRE(ids.insert(node.attribute("id").value()).second);
    PT_REQUIRE(ids.size()==6);
    PT_REQUIRE(history.Undo(current,changed));PT_REQUIRE(current.Props().size()==2);
    PT_REQUIRE(current.Path()==root/"renamed.xml" && current.Dirty());
    PT_REQUIRE(history.Redo(current,changed));PT_REQUIRE(current.Props().size()==4);
    PT_REQUIRE(current.Path()==root/"renamed.xml");
    PT_REQUIRE(workspace.FindPath(root/"renamed.xml",current)==targetId);
    PT_REQUIRE(!workspace.Activate(999999,current,history));
    PT_REQUIRE(current.Props().size()==4 && workspace.ActiveId()==targetId);
    PT_REQUIRE(workspace.Activate(sourceId,current,history));
    workspace.CloseActive(current,history); // clipboard still owns source XML
    PT_REQUIRE(workspace.Size()==1 && workspace.ActiveId()==targetId && current.Props().size()==4);
    PT_REQUIRE(current.AppendLosslessNativeStaticClone(props,planes,boxes,triangles,inserted,error));
    PT_REQUIRE(current.Props().size()==6);
    workspace.CloseActive(current,history);
    PT_REQUIRE(workspace.Size()==1 && current.Props().empty() && current.Path().empty() && !current.Dirty());
    PT_REQUIRE(workspace.Tabs(current)[0].title=="NewProject" && !history.CanUndo());
    // Opening replaces an untouched startup/loaded-empty map, not edited work.
    MapDocument opened;PT_REQUIRE(opened.Load(fixture,error));
    const auto blankId=workspace.ActiveId();
    PT_REQUIRE(workspace.Open(opened,current,history)==blankId && workspace.Size()==1);
    MapDocument empty;empty.CreateBlank("1.0.Light",false);
    workspace.Add(std::move(empty),current,history);
    PT_REQUIRE(current.SaveLegacyAs(root/"empty.xml",error));
    PT_REQUIRE(MapWorkspace::IsUntouchedEmpty(current,history));
    const auto emptyId=workspace.ActiveId();
    workspace.Open(opened,current,history);
    PT_REQUIRE(workspace.Size()==2 && workspace.ActiveId()==emptyId);
    MapDocument beforeDelete=current;
    current.CreateBlank("1.0.Light",false);history.PushSnapshot(beforeDelete,current);
    PT_REQUIRE(!MapWorkspace::IsUntouchedEmpty(current,history));
    workspace.Open(opened,current,history);PT_REQUIRE(workspace.Size()==3);
    // Empty geometry with gameplay is still a meaningful map.
    MapDocument gameplay;gameplay.CreateBlank("1.0.Light",false);
    SpawnMarker spawn;spawn.type="dm";gameplay.AddSpawn(spawn);
    PT_REQUIRE(gameplay.SaveLegacyAs(root/"gameplay.xml",error));
    EditHistory noHistory;PT_REQUIRE(!MapWorkspace::IsUntouchedEmpty(gameplay,noHistory));
    MapWorkspace placeholders;MapDocument blank;blank.CreateBlank("1.0.Light",false);
    MapDocument anotherBlank;anotherBlank.CreateBlank("1.0.Light",false);
    placeholders.Add(std::move(anotherBlank),blank,noHistory);
    PT_REQUIRE(placeholders.Size()==2);
    placeholders.Open(opened,blank,noHistory);PT_REQUIRE(placeholders.Size()==1);
    std::cout<<"PASS: map tabs, isolated undo, cross-map XML/collision clipboard, stable Save As identity and close\n";
}
