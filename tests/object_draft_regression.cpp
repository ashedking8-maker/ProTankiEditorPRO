#include "ObjectDraft.h"
#include <cassert>
#include <fstream>
#include <iostream>
int main() {
    namespace fs=std::filesystem;
    const auto root=fs::temp_directory_path()/"ptpro_object_draft_regression";
    std::error_code ec;fs::remove_all(root,ec);fs::create_directories(root);
    const auto file=root/"model.3ds";
    {std::ofstream out(file,std::ios::binary);out<<"placeholder source bytes";}
    ObjectDraft::Document d;d.name="My Example";d.model=file;
    d.boxes.push_back({{-100,-90,0},{100,90,110},ObjectDraft::BoxRole::Trigger});
    d.scale=0.5f;d.meshVertices={{{-3.f,2.f,0.f}},{{0.f,2.f,1.f}},{{3.f,2.f,0.f}},{{4.f,3.f,0.f}}};d.meshIndices={0,1,2,1,2,3};
    const auto originalLibrary=root/"OriginalLibrary";
    fs::create_directories(originalLibrary/"nested");
    assert(ObjectDraft::IsWithin(originalLibrary/"nested",originalLibrary));
    assert(!ObjectDraft::IsWithin(root/"OriginalLibrary_backup",originalLibrary));
    fs::path saved;std::string error;
    assert(!ObjectDraft::SaveNew(d,originalLibrary,saved,error,originalLibrary));
    assert(!ObjectDraft::SaveNew(d,originalLibrary/"nested",saved,error,originalLibrary));
    assert(ObjectDraft::SaveNew(d,root,saved,error));
    assert(fs::exists(saved/"source.3ds"));
    ObjectDraft::Document loaded;
    assert(ObjectDraft::Load(saved,loaded,error));
    assert(loaded.boxes.size()==2 && loaded.boxes[1].role==ObjectDraft::BoxRole::Trigger);
    assert(loaded.boxes[1].max[2]==110.f);
    assert(loaded.scale==0.5f && loaded.meshVertices.size()==4 && loaded.meshVertices[3][0]==4.f);
    assert(loaded.meshIndices.size()==6 && loaded.meshIndices[5]==3);
    assert(loaded.purpose==ObjectDraft::Purpose::SolidDraft);
    // Optional template is a complete, isolated original library source.
    // Importing a template does NOT convert it into native playable GLB data.
    ObjectDraft::Document templated=d;
    templated.name="With Original Template";
    templated.templateLibrary="Concrete Walls";
    templated.templateGroup="default";
    templated.templateName="Wall End 1";
    templated.templatePropXml="<prop name=\"Wall End 1\" custom=\"unknown\"><mesh file=\"wall.3ds\"/></prop>";
    const std::string libraryBytes="<?xml version=\"1.0\"?><library name=\"Concrete Walls\" unknown=\"preserve\">"
        "<prop-group name=\"default\"><prop name=\"Wall End 1\" custom=\"unknown\"/>"
        "</prop-group><unknown-game-data/></library>";
    templated.libraryTemplateXml=std::make_shared<const std::string>(libraryBytes);
    fs::path templateSaved;
    assert(ObjectDraft::SaveNew(templated,root,templateSaved,error,originalLibrary));
    assert(fs::exists(templateSaved/"library-source.xml") &&
        fs::exists(templateSaved/"library-prop-template.xml") &&
        fs::exists(templateSaved/"library-template-origin.txt"));
    ObjectDraft::Document templateReloaded;
    assert(ObjectDraft::Load(templateSaved,templateReloaded,error));
    assert(templateReloaded.libraryTemplateXml && *templateReloaded.libraryTemplateXml==libraryBytes);
    assert(templateReloaded.templatePropXml==templated.templatePropXml);
    assert(templateReloaded.templateLibrary=="Concrete Walls" && templateReloaded.templateName=="Wall End 1");
    assert(fs::exists(templateSaved/"source.3ds"));
    // Incomplete reference files fail without changing a currently loaded draft.
    fs::remove(templateSaved/"library-prop-template.xml");
    ObjectDraft::Document intactTemplate=templateReloaded;
    assert(!ObjectDraft::Load(templateSaved,intactTemplate,error));
    assert(intactTemplate.libraryTemplateXml && *intactTemplate.libraryTemplateXml==libraryBytes);
    // Version 3 was the previous saved-draft format. It must still load with
    // a safe, explicit default rather than being silently rejected by v4.
    {
        const auto manifest=saved/"object-draft.txt";
        std::ifstream in(manifest,std::ios::binary);
        std::string data((std::istreambuf_iterator<char>(in)),std::istreambuf_iterator<char>());
        const auto version=data.find("PROTANKI_OBJECT_DRAFT 4");
        const auto purpose=data.find("purpose solid_draft\n");
        assert(version!=std::string::npos && purpose!=std::string::npos);
        data.replace(version,std::string("PROTANKI_OBJECT_DRAFT 4").size(),"PROTANKI_OBJECT_DRAFT 3");
        data.erase(data.find("purpose solid_draft\n"),std::string("purpose solid_draft\n").size());
        std::ofstream out(manifest,std::ios::binary|std::ios::trunc);out<<data;out.close();
        ObjectDraft::Document old;
        assert(ObjectDraft::Load(saved,old,error) && old.purpose==ObjectDraft::Purpose::SolidDraft);
    }
    // A decoration is an actual box-free authoring draft, not a fake native
    // collision. The purpose must survive serialization without a game claim.
    ObjectDraft::Document decoration=d;
    decoration.name="No Physical Geometry";
    decoration.purpose=ObjectDraft::Purpose::Decorative;
    decoration.boxes.clear();
    fs::path decorativePath;
    assert(ObjectDraft::SaveNew(decoration,root,decorativePath,error));
    ObjectDraft::Document reopened;
    assert(ObjectDraft::Load(decorativePath,reopened,error));
    assert(reopened.purpose==ObjectDraft::Purpose::Decorative && reopened.boxes.empty());
    decoration.purpose=ObjectDraft::Purpose::DriveableDraft;
    assert(!ObjectDraft::SaveNew(decoration,root,decorativePath,error)); // physical intent needs geometry
    decoration.boxes.push_back(ObjectDraft::Box{});
    decoration.name="Driveable Surface";
    assert(ObjectDraft::SaveNew(decoration,root,decorativePath,error));
    assert(ObjectDraft::Load(decorativePath,reopened,error));
    assert(reopened.purpose==ObjectDraft::Purpose::DriveableDraft && reopened.boxes.size()==1);
    assert(!ObjectDraft::SaveNew(d,root,saved,error)); // no overwrite
    assert(fs::exists(root/"My_Example"/"source.3ds"));
    d.boxes[0].max[0]=d.boxes[0].min[0];
    assert(!ObjectDraft::SaveNew(d,root,saved,error));
    d.boxes[0].max[0]=250;d.name="../../escape";
    assert(ObjectDraft::SaveNew(d,root,saved,error));
    assert(saved.parent_path()==root && fs::exists(saved));
    // Loading malformed data must not partially replace the active document.
    {
        std::ofstream out(saved/"object-draft.txt",std::ios::trunc);
        out<<"PROTANKI_OBJECT_DRAFT 2\nname \"broken\"\nboxes 99999\n";
    }
    ObjectDraft::Document intact=d;
    assert(!ObjectDraft::Load(saved,intact,error));
    assert(intact.name==d.name && intact.boxes.size()==d.boxes.size());
    fs::remove_all(root,ec);
    std::cout<<"ObjectDraft: roundtrip, multi-box, no-overwrite, invalid bounds and path containment PASS\n";
}
