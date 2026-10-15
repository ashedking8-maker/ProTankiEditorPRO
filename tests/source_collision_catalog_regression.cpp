#include "SourceCollisionLearning.h"
#include "SourceCollisionStorage.h"
#include "VerifiedMeshSurface.h"
#include "Native3DSWriter.h"
#include "ReleaseTestCheck.h"
#include <iostream>
int main(int argc,char**argv){
    PT_REQUIRE(argc==3);
    using namespace SourceCollisionCatalog;
    const Polygon t{{0,0,0},{2,0,0},{0,2,0}},adjacent{{2,0,0},{2,2,0},{0,2,0}};
    PT_REQUIRE(SourceCollisionLearning::OverlapArea(t,t,{0,0,0},{1,0,0},{0,1,0})>1.99);
    PT_REQUIRE(SourceCollisionLearning::OverlapArea(t,adjacent,{0,0,0},{1,0,0},{0,1,0})<.0001);
    const auto identity=Identity("Custom","Unknown","Any name");
    const auto root=std::filesystem::temp_directory_path()/"source_collision_catalog_regression_data";
    const auto lib=root/"library"/"Custom";std::filesystem::create_directories(lib);
    Native3DSWriter::Model model;model.visualName="CompletelyUnlistedShape";
    model.vertices={{-1,0,-1.5f,0,0},{1,0,-1.5f,0,0},{1,0,1.5f,0,0},{-1,0,1.5f,0,0}};
    model.indices={0,2,1,0,3,2};model.parts={{0,6,"material","texture.jpg"}};
    Native3DSWriter::Bytes bytes;std::string error;PT_REQUIRE(Native3DSWriter::Write(model,bytes,error));
    const auto meshFile=lib/"shape.3ds";
    {std::ofstream f(meshFile,std::ios::binary);f.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));}
    {std::ofstream f(lib/"library.xml");f<<"<library name='Custom'><prop-group name='Unknown'><prop name='Any name'><mesh file='shape.3ds'/></prop></prop-group></library>";}
    MapDocument authored;authored.CreateBlank();Result collision;collision.planes.push_back({{0,0,0},{0,0,0},2,3});
    for(int i=0;i<2;++i){PropInstance prop;prop.library="Custom";prop.group="Unknown";prop.name="Any name";prop.position={float(i*10),0,0};const auto at=authored.AddProp(prop);PT_REQUIRE(authored.AddImportedCollisionForProp(at,collision));}
    const auto original=root/"reference.xml";PT_REQUIRE(authored.SaveLegacyAs(original,error));
    MapDocument source;PT_REQUIRE(source.Load(original,error));AssetRegistry assets;PT_REQUIRE(assets.Scan(root/"library",error));
    {std::lock_guard lock(Mutex());Entries().clear();}
    auto learned=SourceCollisionLearning::Learn(source,assets);PT_REQUIRE(learned.learned==1&&learned.rejected==0);
    PT_REQUIRE(!NativeCollisionImport::Read(meshFile).Valid());
    auto resolved=VerifiedMeshSurface::Read(meshFile,"",identity);PT_REQUIRE(resolved.Valid()&&resolved.planes.size()==1);
    PT_REQUIRE(!SourceCollisionCatalog::Read(meshFile,"",Identity("Decoration","Unknown","Any name")).Valid());
    MapDocument placed;placed.CreateBlank();auto p=source.Props()[0];p.position={500,600,700};p.rotation.z=1.57079632679f;
    const auto at=placed.AddProp(p);PT_REQUIRE(placed.AddImportedCollisionForProp(at,resolved));
    const auto exported=root/"export.xml";PT_REQUIRE(placed.SaveLegacyAs(exported,error));MapDocument reloaded;PT_REQUIRE(reloaded.Load(exported,error));PT_REQUIRE(reloaded.BindImportedCollisionForProp(0,resolved));
    const auto cache=root/"cache.xml";PT_REQUIRE(SourceCollisionStorage::Save(cache,error));
    {std::lock_guard lock(Mutex());Entries().clear();}
    PT_REQUIRE(!SourceCollisionCatalog::Read(meshFile,"",identity).Valid());PT_REQUIRE(SourceCollisionStorage::Load(cache,error));
    PT_REQUIRE(SourceCollisionCatalog::Read(meshFile,"",identity).Valid());
    const auto invalid=root/"bad-cache.xml";{std::ofstream f(invalid);f<<"broken";}
    PT_REQUIRE(!SourceCollisionStorage::Load(invalid,error));PT_REQUIRE(SourceCollisionCatalog::Read(meshFile,"",identity).Valid());
    // A changed mesh is a new signature, never an excuse to reuse stale physics.
    model.vertices[0].x-=.5f;PT_REQUIRE(Native3DSWriter::Write(model,bytes,error));
    const auto modified=lib/"changed.3ds";{std::ofstream f(modified,std::ios::binary);f.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));}
    PT_REQUIRE(!SourceCollisionCatalog::Read(modified,"",identity).Valid());
    Mesh signature;PT_REQUIRE(Capture(meshFile,"",signature));Result different=resolved;different.planes[0].width=4;
    PT_REQUIRE(!Register(signature,different,identity));PT_REQUIRE(!SourceCollisionCatalog::Read(meshFile,"",identity).Valid());
    PT_REQUIRE(SourceCollisionStorage::Save(cache,error));{std::lock_guard lock(Mutex());Entries().clear();}
    PT_REQUIRE(SourceCollisionStorage::Load(cache,error));PT_REQUIRE(!SourceCollisionCatalog::Read(meshFile,"",identity).Valid());
    // Missing source physics or unknown collision metadata must not produce a partial template.
    for(int mode=0;mode<3;++mode){
        pugi::xml_document xml;PT_REQUIRE(xml.load_file(original.c_str()));auto geometry=xml.child("map").child("collision-geometry");
        if(mode==0)geometry.remove_child(geometry.first_child());
        if(mode==1)geometry.first_child().append_attribute("unknown-friction")="0.7";
        if(mode==2)geometry.append_copy(geometry.first_child());
        const auto path=root/("invalid-source-"+std::to_string(mode)+".xml");PT_REQUIRE(xml.save_file(path.c_str()));
        MapDocument bad;PT_REQUIRE(bad.Load(path,error));{std::lock_guard lock(Mutex());Entries().clear();}
        const auto stats=SourceCollisionLearning::Learn(bad,assets);PT_REQUIRE(stats.learned==0&&stats.rejected==1);
    }
    PT_REQUIRE(SourceCollisionStorage::Load(std::filesystem::path(argv[1]),error));
    PT_REQUIRE(Entries().size()==18);
    const auto bundled=SourceCollisionCatalog::Read(std::filesystem::path(argv[2]),"",Identity("Fogtown","b","cti"));
    PT_REQUIRE(bundled.Valid()&&bundled.planes.size()==12&&bundled.triangles.size()==58);
    std::filesystem::remove_all(root);
    std::cout<<"PASS: arbitrary asset names; repeated source proof; placement/export/reload; persistent cache; changed mesh, conflict, missing, unknown and duplicate collision rejection\n";
}
