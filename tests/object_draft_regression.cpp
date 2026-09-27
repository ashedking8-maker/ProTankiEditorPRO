#include "ObjectDraft.h"
#include "ReleaseTestCheck.h"
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
    d.scale=0.5f;
    d.smoothingMode=3;d.smoothingGroup=7;
    d.materialOverride.enabled=true;d.materialOverride.shading=2;
    d.materialOverride.diffuse={.25f,.5f,.75f};d.materialOverride.shininess=.42f;
    d.meshVertices={{{-3.f,2.f,0.f}},{{0.f,2.f,1.f}},{{3.f,2.f,0.f}},{{4.f,3.f,0.f}}};d.meshIndices={0,1,2,1,2,3};
    const auto originalLibrary=root/"OriginalLibrary";
    fs::create_directories(originalLibrary/"nested");
    PT_REQUIRE(ObjectDraft::IsWithin(originalLibrary/"nested",originalLibrary));
    PT_REQUIRE(!ObjectDraft::IsWithin(root/"OriginalLibrary_backup",originalLibrary));
    fs::path saved;std::string error;
    PT_REQUIRE(!ObjectDraft::SaveNew(d,originalLibrary,saved,error,originalLibrary));
    PT_REQUIRE(!ObjectDraft::SaveNew(d,originalLibrary/"nested",saved,error,originalLibrary));
    PT_REQUIRE(ObjectDraft::SaveNew(d,root,saved,error));
    PT_REQUIRE(fs::exists(saved/"source.3ds"));
    ObjectDraft::Document loaded;
    PT_REQUIRE(ObjectDraft::Load(saved,loaded,error));
    PT_REQUIRE(loaded.boxes.size()==2 && loaded.boxes[1].role==ObjectDraft::BoxRole::Trigger);
    PT_REQUIRE(loaded.boxes[1].max[2]==110.f);
    PT_REQUIRE(loaded.scale==0.5f && loaded.meshVertices.size()==4 && loaded.meshVertices[3][0]==4.f);
    PT_REQUIRE(loaded.meshIndices.size()==6 && loaded.meshIndices[5]==3);
    PT_REQUIRE(loaded.purpose==ObjectDraft::Purpose::SolidDraft);
    PT_REQUIRE(loaded.smoothingMode==3 && loaded.smoothingGroup==7 &&
        loaded.materialOverride.enabled && loaded.materialOverride.shading==2 &&
        loaded.materialOverride.diffuse[2]==.75f && loaded.materialOverride.shininess==.42f);
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
    templated.excludedTemplateFields={"/prop/@custom"};
    fs::path templateSaved;
    PT_REQUIRE(ObjectDraft::SaveNew(templated,root,templateSaved,error,originalLibrary));
    PT_REQUIRE(fs::exists(templateSaved/"library-source.xml") &&
        fs::exists(templateSaved/"library-prop-template.xml") &&
        fs::exists(templateSaved/"library-template-origin.txt"));
    ObjectDraft::Document templateReloaded;
    PT_REQUIRE(ObjectDraft::Load(templateSaved,templateReloaded,error));
    PT_REQUIRE(templateReloaded.libraryTemplateXml && *templateReloaded.libraryTemplateXml==libraryBytes);
    PT_REQUIRE(templateReloaded.templatePropXml==templated.templatePropXml);
    PT_REQUIRE(fs::exists(templateSaved/"library-prop-selection.txt"));
    PT_REQUIRE(templateReloaded.excludedTemplateFields==templated.excludedTemplateFields);
    PT_REQUIRE(templateReloaded.templateLibrary=="Concrete Walls" && templateReloaded.templateName=="Wall End 1");
    // Per-field draft selection never strips the raw source snapshots.
    PT_REQUIRE(templateReloaded.templatePropXml.find("custom=\"unknown\"")!=std::string::npos);
    PT_REQUIRE(templateReloaded.libraryTemplateXml->find("<unknown-game-data/>")!=std::string::npos);
    PT_REQUIRE(fs::exists(templateSaved/"source.3ds"));
    // Incomplete reference files fail without changing a currently loaded draft.
    fs::remove(templateSaved/"library-prop-template.xml");
    ObjectDraft::Document intactTemplate=templateReloaded;
    PT_REQUIRE(!ObjectDraft::Load(templateSaved,intactTemplate,error));
    PT_REQUIRE(intactTemplate.libraryTemplateXml && *intactTemplate.libraryTemplateXml==libraryBytes);
    // Version 4 and 3 manifests still load with safe defaults for new visual fields.
    {
        const auto manifest=saved/"object-draft.txt";
        std::ifstream in(manifest,std::ios::binary);
        std::string data((std::istreambuf_iterator<char>(in)),std::istreambuf_iterator<char>());
        const auto version=data.find("PROTANKI_OBJECT_DRAFT 5");
        const auto smoothing=data.find("smoothing 3 7\n");
        const auto material=data.find("material_override ");
        PT_REQUIRE(version!=std::string::npos && smoothing!=std::string::npos && material!=std::string::npos);
        data.replace(version,std::string("PROTANKI_OBJECT_DRAFT 5").size(),"PROTANKI_OBJECT_DRAFT 4");
        const auto materialEnd=data.find('\n',data.find("material_override "));
        data.erase(data.find("material_override "),materialEnd-data.find("material_override ")+1);
        data.erase(data.find("smoothing 3 7\n"),std::string("smoothing 3 7\n").size());
        {std::ofstream out(manifest,std::ios::binary|std::ios::trunc);out<<data;}
        ObjectDraft::Document v4;
        PT_REQUIRE(ObjectDraft::Load(saved,v4,error) && v4.purpose==ObjectDraft::Purpose::SolidDraft &&
            v4.smoothingMode==0 && !v4.materialOverride.enabled);
        data.replace(data.find("PROTANKI_OBJECT_DRAFT 4"),std::string("PROTANKI_OBJECT_DRAFT 4").size(),"PROTANKI_OBJECT_DRAFT 3");
        data.erase(data.find("purpose solid_draft\n"),std::string("purpose solid_draft\n").size());
        {std::ofstream out(manifest,std::ios::binary|std::ios::trunc);out<<data;}
        ObjectDraft::Document v3;
        PT_REQUIRE(ObjectDraft::Load(saved,v3,error) && v3.purpose==ObjectDraft::Purpose::SolidDraft &&
            v3.smoothingMode==0 && !v3.materialOverride.enabled);
    }
    // A decoration is an actual box-free authoring draft, not a fake native
    // collision. The purpose must survive serialization without a game claim.
    ObjectDraft::Document decoration=d;
    decoration.name="No Physical Geometry";
    decoration.purpose=ObjectDraft::Purpose::Decorative;
    decoration.boxes.clear();
    fs::path decorativePath;
    PT_REQUIRE(ObjectDraft::SaveNew(decoration,root,decorativePath,error));
    ObjectDraft::Document reopened;
    PT_REQUIRE(ObjectDraft::Load(decorativePath,reopened,error));
    PT_REQUIRE(reopened.purpose==ObjectDraft::Purpose::Decorative && reopened.boxes.empty());
    decoration.purpose=ObjectDraft::Purpose::DriveableDraft;
    PT_REQUIRE(!ObjectDraft::SaveNew(decoration,root,decorativePath,error)); // physical intent needs geometry
    decoration.boxes.push_back(ObjectDraft::Box{});
    decoration.name="Driveable Surface";
    PT_REQUIRE(ObjectDraft::SaveNew(decoration,root,decorativePath,error));
    PT_REQUIRE(ObjectDraft::Load(decorativePath,reopened,error));
    PT_REQUIRE(reopened.purpose==ObjectDraft::Purpose::DriveableDraft && reopened.boxes.size()==1);
    PT_REQUIRE(!ObjectDraft::SaveNew(d,root,saved,error)); // no overwrite
    PT_REQUIRE(fs::exists(root/"My_Example"/"source.3ds"));
    d.boxes[0].max[0]=d.boxes[0].min[0];
    PT_REQUIRE(!ObjectDraft::SaveNew(d,root,saved,error));
    d.boxes[0].max[0]=250;d.name="../../escape";
    PT_REQUIRE(ObjectDraft::SaveNew(d,root,saved,error));
    PT_REQUIRE(saved.parent_path()==root && fs::exists(saved));
    // Loading malformed data must not partially replace the active document.
    {
        std::ofstream out(saved/"object-draft.txt",std::ios::trunc);
        out<<"PROTANKI_OBJECT_DRAFT 2\nname \"broken\"\nboxes 99999\n";
    }
    ObjectDraft::Document intact=d;
    PT_REQUIRE(!ObjectDraft::Load(saved,intact,error));
    PT_REQUIRE(intact.name==d.name && intact.boxes.size()==d.boxes.size());
    fs::remove_all(root,ec);
    std::cout<<"ObjectDraft: roundtrip, multi-box, no-overwrite, invalid bounds and path containment PASS\n";
}
