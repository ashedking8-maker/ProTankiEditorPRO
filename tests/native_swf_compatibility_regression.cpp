#include "Native3DSScene.h"
#include "Native3DSWriter.h"
#include "NativeCollisionImport.h"
#include "ReleaseTestCheck.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
namespace W=Native3DSWriter;
namespace S=Native3DSScene;
static bool Eq(float a,float b){return std::fabs(a-b)<.002f;}
static bool Eq(S::V a,S::V b){return Eq(a.x,b.x)&&Eq(a.y,b.y)&&Eq(a.z,b.z);}
static W::Bytes Frame(const std::string& name,int parent,S::V pivot={},S::V pos={},float angle=0){
    W::Bytes node,h,p;
    W::CStr(h,name);W::U16(h,0);W::U16(h,0);W::U16(h,static_cast<std::uint16_t>(parent));
    W::Add(node,W::Chunk(0xb010,h));for(float v:{pivot.x,pivot.y,pivot.z})W::Float(p,v);
    W::Add(node,W::Chunk(0xb013,p));
    W::Add(node,W::Chunk(0xb020,W::KeyTrack({pos.x,pos.y,pos.z})));
    W::Add(node,W::Chunk(0xb021,W::KeyTrack({angle,1,0,0})));
    W::Add(node,W::Chunk(0xb022,W::KeyTrack({2,3,4}))); // ignored by original CollisionRect.parse
    return W::Chunk(0xb002,node);
}
static bool Save(const std::filesystem::path& file,const W::Bytes& b){
    std::ofstream f(file,std::ios::binary);f.write(reinterpret_cast<const char*>(b.data()),static_cast<std::streamsize>(b.size()));return bool(f);
}
static W::Bytes OffsetObject(const std::string& name,S::V origin,
                             const std::vector<std::array<float,3>>& local,
                             const std::vector<std::array<std::uint16_t,3>>& faces){
    W::Bytes mesh,verts,matrix,face,obj;W::U16(verts,static_cast<std::uint16_t>(local.size()));
    for(const auto& v:local){W::Float(verts,v[0]+origin.x);W::Float(verts,v[1]+origin.y);W::Float(verts,v[2]+origin.z);}
    W::Add(mesh,W::Chunk(0x4110,verts));
    for(int i=0;i<12;++i)W::Float(matrix,i==0||i==4||i==8?1.f:(i==9?origin.x:(i==10?origin.y:(i==11?origin.z:0.f))));
    W::Add(mesh,W::Chunk(0x4160,matrix));W::U16(face,static_cast<std::uint16_t>(faces.size()));
    for(const auto& f:faces){for(auto i:f)W::U16(face,i);W::U16(face,0);}
    W::Add(mesh,W::Chunk(0x4120,face));W::CStr(obj,name);W::Add(obj,W::Chunk(0x4100,mesh));return W::Chunk(0x4000,obj);
}
int main(int argc,char** argv){
    PT_REQUIRE(argc==2);const auto root=std::filesystem::path(argv[1]);std::string error;
    const auto temp=std::filesystem::temp_directory_path()/
        ("ptpro-swf-compat-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(temp);
    struct Cleanup{std::filesystem::path p;~Cleanup(){std::error_code ec;std::filesystem::remove_all(p,ec);}}cleanup{temp};
    // Hand-calculated inverse of a translated, scaled, sheared and mirrored basis.
    S::Node n;n.hasMatrix=true;n.matrix={2,0,0,1,3,0,0,0,-4,10,20,30};
    S::Frame f;f.pivot={1,2,3};S::V local;
    PT_REQUIRE(S::LocalVertex(n,&f,{24,38,-2},local)&&Eq(local,{3,4,5}));
    PT_REQUIRE(S::LocalVertex(n,nullptr,{24,38,-2},local)&&Eq(local,{24,38,-2}));
    n.matrix[0]=0;PT_REQUIRE(!S::LocalVertex(n,&f,{24,38,-2},local));
    PT_REQUIRE(Eq(S::AngleAxis(1.57079632679f,{0,0,1}).z,-1.57079632679f));
    // Legacy artist scenes may store a visual mesh around a huge world-space
    // translation and cancel it with 0x4160/keyframe data. porch1.3ds in the
    // original library is such a file. Reject NaN/Inf raw data, but apply the
    // magnitude sanity limit only after normalization into prop-local space.
    {
        constexpr S::V origin{11417447.f,-1146447.25f,-69.72265625f};
        W::Bytes largeMeshes,largeFrames,largeBody;
        const std::vector<std::array<float,3>> visualLocal{{-500,-250,0},{0,-250,0},{0,250,0},{-500,250,0}};
        W::Add(largeMeshes,OffsetObject("visual",origin,visualLocal,{{0,1,2},{0,2,3}}));
        W::Add(largeMeshes,W::Object("PlaneChild",{{0,0,0},{200,0,0},{0,300,0}},{{0,1,2}}));
        W::Add(largeFrames,Frame("visual",-1,{},origin));W::Add(largeFrames,Frame("PlaneChild",0));
        W::Add(largeBody,W::Chunk(0x3d3d,largeMeshes));W::Add(largeBody,W::Chunk(0xb000,largeFrames));
        const auto large=temp/"large-world-origin.3ds";PT_REQUIRE(Save(large,W::Chunk(0x4d4d,largeBody)));
        S::Scene largeScene;S::Selection largeSelection;PT_REQUIRE(S::Read(large,largeScene,error));
        PT_REQUIRE(S::Resolve(largeScene,"visual",largeSelection,error));
        S::V normalized{};const auto* largeFrame=S::SelectedFrame(largeScene,largeSelection);
        PT_REQUIRE(S::LocalVertex(largeScene.nodes[largeSelection.node],largeFrame,
            largeScene.nodes[largeSelection.node].vertices.front(),normalized));
        PT_REQUIRE(Eq(normalized,{-500,-250,0}));
        const auto largeCollision=NativeCollisionImport::Read(large,"visual");
        PT_REQUIRE(largeCollision.Valid()&&largeCollision.planes.size()==1);
        // Relaxing raw world coordinates must not allow a helper whose FINAL
        // prop-local collision position is absurdly large.
        largeFrames.clear();largeBody.clear();W::Add(largeFrames,Frame("visual",-1,{},origin));
        W::Add(largeFrames,Frame("PlaneChild",0,{},S::V{20000000.f,0,0}));
        W::Add(largeBody,W::Chunk(0x3d3d,largeMeshes));W::Add(largeBody,W::Chunk(0xb000,largeFrames));
        const auto unsafe=temp/"large-helper-offset.3ds";PT_REQUIRE(Save(unsafe,W::Chunk(0x4d4d,largeBody)));
        const auto unsafeCollision=NativeCollisionImport::Read(unsafe,"visual");
        PT_REQUIRE(!unsafeCollision.Valid()&&unsafeCollision.error.find("Out-of-range normalized")!=std::string::npos);
    }
    // Two roots, a direct plane, a grandchild and an unrelated root named Box.
    // Only the direct plane belongs to the explicitly selected visual object.
    W::Bytes meshes,frames,body;
    const std::vector<std::array<float,3>> vertices{{1,1,0},{3,1,0},{1,5,0}};
    for(const auto* name:{"visual","PlaneChild","BoxGrandchild","BoxOtherRoot"})
        W::Add(meshes,W::Object(name,vertices,{{0,1,2}}));
    W::Add(frames,Frame("visual",-1));
    W::Add(frames,Frame("PlaneChild",0,{1,1,0},{10,20,30},1.57079632679f));
    W::Add(frames,Frame("BoxGrandchild",1));W::Add(frames,Frame("BoxOtherRoot",-1));
    W::Add(body,W::Chunk(0x3d3d,meshes));W::Add(body,W::Chunk(0xb000,frames));
    const auto path=temp/"anything.3ds";PT_REQUIRE(Save(path,W::Chunk(0x4d4d,body)));
    S::Scene scene;S::Selection selection;
    PT_REQUIRE(S::Read(path,scene,error));PT_REQUIRE(S::Resolve(scene,"",selection,error)&&selection.frame==0&&selection.usedRootFallback);
    PT_REQUIRE(S::Resolve(scene,"visual",selection,error)&&selection.frame==0);
    PT_REQUIRE(!S::Resolve(scene,"missing",selection,error));
    const auto helpers=NativeCollisionImport::Read(path,"visual");
    PT_REQUIRE(helpers.Valid()&&helpers.planes.size()==1&&helpers.boxes.empty());
    const auto plane=helpers.planes.front();
    PT_REQUIRE(Eq(plane.width,2)&&Eq(plane.length,4));
    PT_REQUIRE(Eq(plane.offset,{11,20,28})&&Eq(plane.rotation.x,-1.57079632679f));
    // A cyclic hierarchy is rejected before any exportable helper is returned.
    frames=Frame("visual",0);body.clear();W::Add(body,W::Chunk(0x3d3d,meshes));W::Add(body,W::Chunk(0xb000,frames));
    PT_REQUIRE(Save(path,W::Chunk(0x4d4d,body)));PT_REQUIRE(!S::Read(path,scene,error));
    // File renaming cannot change selection or primitive dimensions.
    for(const auto* name:{"contain","tunnel_1","tunnel_2"}){
        const auto source=root/"esplanade"/(std::string(name)+".3ds");
        const auto renamed=temp/"renamed.3ds";std::filesystem::copy_file(source,renamed,std::filesystem::copy_options::overwrite_existing);
        const auto a=NativeCollisionImport::Read(source),b=NativeCollisionImport::Read(renamed);
        PT_REQUIRE(a.Valid()&&b.Valid()&&a.visualAnchor==b.visualAnchor);
        PT_REQUIRE(a.planes.size()==b.planes.size()&&a.boxes.size()==b.boxes.size()&&a.triangles.size()==b.triangles.size());
        for(size_t i=0;i<a.boxes.size();++i)PT_REQUIRE(Eq(a.boxes[i].size,b.boxes[i].size)&&Eq(a.boxes[i].offset,b.boxes[i].offset));
    }
    const auto scaled=NativeCollisionImport::Read(root/"native_helpers/combuild_comb3.3ds");
    // SWF CollisionBox.parse uses LOCAL bounds, not the helper's scaleY=2.
    PT_REQUIRE(scaled.Valid()&&scaled.boxes.size()==1&&Eq(scaled.boxes[0].size,{500,30,150}));
    PT_REQUIRE(S::Read(root/"native_helpers/land01_delta136_game_verified.3ds",scene,error));
    PT_REQUIRE(scene.migratedFlatGenerated);
    const auto older=NativeCollisionImport::Read(root/"native_helpers/land01_delta136_game_verified.3ds");
    PT_REQUIRE(older.Valid()&&older.triangles.size()==138);
    std::cout<<"SWF compatibility: inverse matrices, pivots, angle-axis, explicit selection, direct children, cycle rejection, filename independence, original box scale and old PTPRO migration PASS\n";
}
