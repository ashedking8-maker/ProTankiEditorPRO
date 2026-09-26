#include "NativeTerrainDelta.h"
#include "NativeTaraWriter.h"
#include "ReleaseTestCheck.h"
#include <filesystem>
#include <fstream>
#include <iostream>

int main(int argc,char** argv){
    PT_REQUIRE(argc==2);
    const auto root=std::filesystem::path(argv[1]);std::string error;
    const auto src=root/"land01_original.3ds",reference=root/"land01_delta136_game_verified.3ds";
    const auto original=NativeCollisionImport::Read(src);
    PT_REQUIRE(original.Valid()&&original.triangles.size()==136&&original.boxes.empty());
    const auto nodes=NativeCollisionImport::ReadNodes(reference,error);
    PT_REQUIRE(error.empty());
    const NativeCollisionImport::Node* v=nullptr;
    for(const auto& n:nodes)if(n.name=="ptpro_mesh")v=&n;
    PT_REQUIRE(v&&v->vertices.size()==71&&v->faces.size()==120);
    Native3DSWriter::Model model;model.visualName="ptpro_mesh";
    for(const auto& p:v->vertices)model.vertices.push_back({p.x,p.z,p.y,0.f,0.f});
    for(const auto& f:v->faces){model.indices.push_back(f[0]);model.indices.push_back(f[2]);model.indices.push_back(f[1]);}
    model.parts={{0,model.indices.size(),"ptpro_mat_0","land21.jpg"}};
    PT_REQUIRE(NativeTerrainDelta::Build(src,model,model.triangles,error));
    PT_REQUIRE(error.empty()&&model.triangles.size()==138);
    // The published, user-tested single-peak fixture is the authoritative
    // geometry. Order and vertex coordinates must match to small float noise.
    std::vector<Native3DSWriter::Triangle> expected;
    for(const auto& n:nodes)if(NativeCollisionImport::Prefix(n.name,"tri")){
        PT_REQUIRE(n.vertices.size()==3&&n.faces.size()==1);
        const auto f=n.faces.front();
        expected.push_back({{{n.vertices[f[0]].x,n.vertices[f[0]].y,n.vertices[f[0]].z},
                             {n.vertices[f[1]].x,n.vertices[f[1]].y,n.vertices[f[1]].z},
                             {n.vertices[f[2]].x,n.vertices[f[2]].y,n.vertices[f[2]].z}}});
    }
    PT_REQUIRE(expected.size()==138);
    for(size_t i=0;i<expected.size();++i)for(int j=0;j<3;++j)for(int k=0;k<3;++k)
        PT_REQUIRE(std::abs(model.triangles[i][j][k]-expected[i][j][k])<.1f);
    Native3DSWriter::Bytes serialized;PT_REQUIRE(Native3DSWriter::Write(model,serialized,error));
    const auto tmp=std::filesystem::temp_directory_path()/"ptpro_0525_delta136_test.3ds";
    {std::ofstream file(tmp,std::ios::binary|std::ios::trunc);file.write(reinterpret_cast<const char*>(serialized.data()),static_cast<std::streamsize>(serialized.size()));PT_REQUIRE(bool(file));}
    const auto back=NativeCollisionImport::Read(tmp);std::error_code ec;std::filesystem::remove(tmp,ec);
    PT_REQUIRE(back.Valid()&&back.triangles.size()==138&&back.boxes.empty()&&back.planes.empty());
    auto unsupported=model;unsupported.vertices[0].y+=100.f;
    PT_REQUIRE(!NativeTerrainDelta::Build(src,unsupported,unsupported.triangles,error));
    // TARA header and data exact big endian, independent of Python reference.
    std::vector<NativeTaraWriter::Entry> entries={{"library.xml",{'x','m','l'}},{"ptpro_mesh.3ds",{1,2,3,4}}};
    NativeTaraWriter::Bytes bytes;PT_REQUIRE(NativeTaraWriter::Encode(entries,bytes,error));
    PT_REQUIRE(bytes.size()==4+2+11+4+2+14+4+3+4&&bytes[0]==0&&bytes[3]==2);
    std::vector<NativeTaraWriter::Entry> decoded;PT_REQUIRE(NativeTaraWriter::Decode(bytes,decoded,error)&&entries==decoded);
    bytes.push_back(42);PT_REQUIRE(!NativeTaraWriter::Decode(bytes,decoded,error));
    std::cout<<"PASS: game-verified 136->138 terrain helper match, 3DS readback, fail-closed multi-edit, TARA roundtrip\n";
}
