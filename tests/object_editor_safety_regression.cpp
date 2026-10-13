#include "ObjectDraft.h"
#include "NativeVerifiedSurfaceExport.h"
#include "ReleaseTestCheck.h"
#include <iostream>
int main(int argc,char**argv) {
    PT_REQUIRE(argc==2);
    ObjectDraft::Document previous;previous.purpose=ObjectDraft::Purpose::TriggerDraft;
    previous.tint={.2f,.3f,.4f};previous.boxes[0].role=ObjectDraft::BoxRole::Trigger;
    previous.templateName="Old model";previous.materialOverride.enabled=true;
    previous.meshVertices={{1,2,3}};previous.excludedTemplateFields={"/prop/name"};
    previous=ObjectDraft::FreshSource(std::filesystem::path(argv[1])/"land01.3ds","New model");
    PT_REQUIRE(previous.purpose==ObjectDraft::Purpose::SolidDraft && previous.boxes.size()==1);
    PT_REQUIRE(previous.boxes[0].role==ObjectDraft::BoxRole::Solid && previous.templateName.empty());
    PT_REQUIRE(!previous.materialOverride.enabled && previous.meshVertices.empty() && previous.excludedTemplateFields.empty());
    PT_REQUIRE(previous.tint[0]==1.f && previous.scale==1.f);
    const auto certified=VerifiedMeshSurface::ReadVerified(previous.model);PT_REQUIRE(certified.Valid());
    Native3DSWriter::Model original;original.visualName="ptpro_mesh";
    const auto& ref=VerifiedMeshSurface::References()[0];
    for(auto p:ref.vertices)original.vertices.push_back({p.x,p.z,p.y,0,0});
    for(auto f:ref.faces){original.indices.push_back(f[0]);original.indices.push_back(f[2]);original.indices.push_back(f[1]);}
    original.parts={{0,original.indices.size(),"mat","land1.jpg"}};
    auto edited=original;std::string error;
    PT_REQUIRE(NativeVerifiedSurfaceExport::Build(certified,original,edited,edited.triangles,error));
    PT_REQUIRE(edited.triangles.size()==18&&edited.boxes.empty());
    Native3DSWriter::Bytes bytes;PT_REQUIRE(Native3DSWriter::Write(edited,bytes,error));
    const auto tmp=std::filesystem::temp_directory_path()/"object_surface_preservation_test.3ds";
    {std::ofstream f(tmp,std::ios::binary);f.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));PT_REQUIRE(bool(f));}
    const auto back=NativeCollisionImport::Read(tmp);std::filesystem::remove(tmp);
    PT_REQUIRE(back.Valid() && back.triangles.size()==18 && back.boxes.empty());
    for(size_t i=0;i<18;++i) {
        const auto& a=certified.triangles[i];const auto& b=back.triangles[i];
        const auto wa=VerifiedMeshSurface::World(a.offset,a.rotation,a.v0,a.v1,a.v2);
        const auto wb=VerifiedMeshSurface::World(b.offset,b.rotation,b.v0,b.v1,b.v2);
        PT_REQUIRE(VerifiedMeshSurface::SameCorners(wa,wb)&&VerifiedMeshSurface::SameWinding(wa,wb));
    }
    edited.vertices[0].y+=10;
    PT_REQUIRE(!NativeVerifiedSurfaceExport::Build(certified,original,edited,edited.triangles,error));
    PT_REQUIRE(edited.triangles.empty());
    edited=original;std::swap(edited.indices[1],edited.indices[2]);
    PT_REQUIRE(!NativeVerifiedSurfaceExport::Build(certified,original,edited,edited.triangles,error));
    std::cout<<"PASS: fresh source state; verified surface preserved through 3DS; edited/topology-changed surface refused\n";
}
