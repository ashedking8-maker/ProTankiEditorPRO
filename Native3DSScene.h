#pragma once
// GTanksEditor.swf: Parser3DS buildMesh/buildHierarchy and MeshLoader selection.
// No filename/material scoring. Coordinates stay in native Z-up space here.
#include <DirectXMath.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
namespace Native3DSScene {
using V=DirectX::XMFLOAT3;
inline V Add(V a,V b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
inline V Sub(V a,V b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
inline V Mul(V a,float s){return {a.x*s,a.y*s,a.z*s};}
inline float Dot(V a,V b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline V Cross(V a,V b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
inline bool Numeric(V a){return std::isfinite(a.x)&&std::isfinite(a.y)&&std::isfinite(a.z);}
// Sanity limit for NORMALIZED prop-local values. Raw 3DS world coordinates can
// legitimately be much larger because old assets may carry an artist-scene
// translation that is cancelled by the 0x4160 matrix / keyframe root.
inline bool Finite(V a){return Numeric(a)&&
    std::fabs(a.x)<1.e7f&&std::fabs(a.y)<1.e7f&&std::fabs(a.z)<1.e7f;}
struct Node {
    std::string name;std::vector<V> vertices;
    std::vector<std::array<std::uint16_t,3>> faces;
    std::array<float,12> matrix{};bool hasMatrix{};
};
struct Frame {
    std::string name;int parent{-1};
    V pivot{},position{},rotation{},scale{1,1,1};
};
struct Scene {std::vector<Node> nodes;std::vector<Frame> frames;bool migratedFlatGenerated{};};
struct Selection {size_t node{};int frame{-1};bool usedRootFallback{};};
inline V Rotate(V v,V r){
    const float cx=std::cos(r.x),sx=std::sin(r.x),cy=std::cos(r.y),sy=std::sin(r.y),cz=std::cos(r.z),sz=std::sin(r.z);
    V x{v.x,cx*v.y-sx*v.z,sx*v.y+cx*v.z};V y{cy*x.x+sy*x.z,x.y,-sy*x.x+cy*x.z};
    return {cz*y.x-sz*y.y,sz*y.x+cz*y.y,y.z};
}
// Exact formulas from Parser3DS.getRotationFrom3DSAngleAxis (negative 3DS angle).
inline V AngleAxis(float angle,V axis){
    const float s=std::sin(angle),c=std::cos(angle),t=1-c;
    const float q=axis.x*axis.z*t+axis.y*s;
    constexpr float halfPi=1.5707963267948966f;
    if(q>=1.f)return {0,-halfPi,-2.f*std::atan2(axis.x*std::sin(angle*.5f),std::cos(angle*.5f))};
    if(q<=-1.f)return {0,halfPi,2.f*std::atan2(axis.x*std::sin(angle*.5f),std::cos(angle*.5f))};
    return {-std::atan2(axis.x*s-axis.z*axis.y*t,1-(axis.x*axis.x+axis.y*axis.y)*t),
        -std::asin(q),-std::atan2(axis.z*s-axis.x*axis.y*t,1-(axis.z*axis.z+axis.y*axis.y)*t)};
}
inline bool LocalVertex(const Node& node,const Frame* frame,V world,V& local){
    local=world;
    if(!frame)return Finite(local); // Original no-animation branch leaves 0x4110 coordinates alone.
    if(!node.hasMatrix)return false;
    const auto& m=node.matrix;V x{m[0],m[1],m[2]},y{m[3],m[4],m[5]},z{m[6],m[7],m[8]};
    const double det=static_cast<double>(Dot(x,Cross(y,z)));
    if(!std::isfinite(det)||std::fabs(det)<1.e-9)return false;
    auto p=Sub(world,{m[9],m[10],m[11]});
    local={static_cast<float>(Dot(p,Cross(y,z))/det),static_cast<float>(Dot(p,Cross(z,x))/det),static_cast<float>(Dot(p,Cross(x,y))/det)};
    local=Sub(local,frame->pivot);return Finite(local);
}
inline bool Read(const std::filesystem::path& path,Scene& out,std::string& error){
    out={};std::error_code ec;auto size=std::filesystem::file_size(path,ec);
    if(ec||size<6||size>64ull*1024*1024){error="3DS source is missing or exceeds 64 MB.";return false;}
    std::ifstream input(path,std::ios::binary);std::vector<std::uint8_t> b((std::istreambuf_iterator<char>(input)),{});
    if(b.size()!=size){error="Short 3DS read.";return false;}
    auto u16=[&](size_t p){return std::uint16_t(b[p])|(std::uint16_t(b[p+1])<<8);};
    auto u32=[&](size_t p){return std::uint32_t(u16(p))|(std::uint32_t(u16(p+2))<<16);};
    auto f32=[&](size_t p){auto bits=u32(p);float f{};std::memcpy(&f,&bits,4);return f;};
    auto vec=[&](size_t p){return V{f32(p),f32(p+4),f32(p+8)};};
    if(u16(0)!=0x4d4d||u32(2)!=b.size()){error="Invalid 3DS root chunk.";return false;}
    bool failed=false;
    auto str=[&](size_t& a,size_t e){size_t start=a;while(a<e&&b[a])++a;
        if(a==e||a-start>1024){failed=true;return std::string{};}
        std::string value(reinterpret_cast<const char*>(b.data()+start),a-start);++a;return value;};
    auto walk=[&](auto&& self,size_t a,size_t end,int obj,int anim,int depth)->void{
        if(depth>24){failed=true;return;}
        while(a<end&&!failed){
            if(end-a<6){failed=true;break;}
            auto tag=u16(a);auto len=u32(a+2);if(len<6||len>end-a){failed=true;break;}
            size_t p=a+6,e=a+len;
            if(tag==0x4000){
                auto name=str(p,e);if(out.nodes.size()>=4096){failed=true;break;}
                Node n;n.name=std::move(name);out.nodes.push_back(std::move(n));
                self(self,p,e,static_cast<int>(out.nodes.size()-1),-1,depth+1);
            }else if(tag==0xb002){
                if(out.frames.size()>=4096){failed=true;break;}
                out.frames.push_back({});self(self,p,e,-1,static_cast<int>(out.frames.size()-1),depth+1);
            }else if(tag==0x4d4d||tag==0x3d3d||tag==0x4100||tag==0xb000)self(self,p,e,obj,anim,depth+1);
            else if(obj>=0){
                auto& n=out.nodes[static_cast<size_t>(obj)];
                if(tag==0x4110){
                    if(e-p<2){failed=true;break;}size_t count=u16(p);p+=2;
                    if(count>(e-p)/12||!n.vertices.empty()){failed=true;break;}
                    for(size_t i=0;i<count;++i){auto v=vec(p+12*i);if(!Numeric(v)){failed=true;break;}n.vertices.push_back(v);}
                }else if(tag==0x4120){
                    if(e-p<2){failed=true;break;}size_t count=u16(p);p+=2;
                    if(count>(e-p)/8||!n.faces.empty()){failed=true;break;}
                    for(size_t i=0;i<count;++i)n.faces.push_back({static_cast<std::uint16_t>(u16(p+8*i)),static_cast<std::uint16_t>(u16(p+8*i+2)),static_cast<std::uint16_t>(u16(p+8*i+4))});
                }else if(tag==0x4160){
                    if(e-p<48||n.hasMatrix){failed=true;break;}n.hasMatrix=true;
                    for(size_t i=0;i<12;++i){n.matrix[i]=f32(p+4*i);if(!std::isfinite(n.matrix[i]))failed=true;}
                }
            }else if(anim>=0){
                auto& f=out.frames[static_cast<size_t>(anim)];
                if(tag==0xb010){f.name=str(p,e);if(e-p<6){failed=true;break;}
                    auto parent=u16(p+4);f.parent=parent==65535?-1:parent;
                }else if(tag==0xb011)f.name=str(p,e); // Same overwrite as original parser.
                else if(tag==0xb013){if(e-p<12){failed=true;break;}f.pivot=vec(p);}
                else if(tag==0xb020||tag==0xb021||tag==0xb022){
                    // The source editor consumes its first key at the fixed 20-byte offset.
                    if(e-p<(tag==0xb021?36u:32u)){failed=true;break;}
                    if(tag==0xb020)f.position=vec(p+20);
                    if(tag==0xb021)f.rotation=AngleAxis(f32(p+20),vec(p+24));
                    if(tag==0xb022)f.scale=vec(p+20);
                }
                if(!Finite(f.pivot)||!Numeric(f.position)||!Finite(f.rotation)||!Finite(f.scale))failed=true;
            }
            a=e;
        }
    };
    walk(walk,0,b.size(),-1,-1,0);
    if(failed||out.nodes.empty()){error="Malformed 3DS mesh/keyframe chunks.";return false;}
    for(const auto& n:out.nodes)for(const auto& f:n.faces)for(auto i:f)if(i>=n.vertices.size()){
        error="Invalid 3DS face index.";return false;}
    for(size_t i=0;i<out.frames.size();++i){
        int at=static_cast<int>(i);size_t depth=0;
        while(at>=0){if(static_cast<size_t>(at)>=out.frames.size()||++depth>out.frames.size()){
            error="Invalid/cyclic 3DS parent hierarchy.";return false;}at=out.frames[static_cast<size_t>(at)].parent;}
    }
    // Earlier PTPRO writers emitted ptpro_mesh and identity helpers as siblings.
    // Migrate only that known generated layout; original libraries never use this fallback.
    if(out.frames.size()>1 && out.frames[0].name=="ptpro_mesh"){
        bool generated=out.nodes.size()==out.frames.size();
        for(size_t i=0;i<out.frames.size();++i){const auto& f=out.frames[i];
            bool helper=i==0||f.name.rfind("BoxPT",0)==0||f.name.rfind("tri_PT",0)==0||
                (f.name.rfind("tri_",0)==0 && f.name.size()>4 &&
                 std::all_of(f.name.begin()+4,f.name.end(),[](char c){return c>='0'&&c<='9';}));
            if(!helper||f.parent!=-1||Dot(f.pivot,f.pivot)>1.e-10f||Dot(f.position,f.position)>1.e-10f||
               Dot(f.rotation,f.rotation)>1.e-10f||Dot(Sub(f.scale,{1,1,1}),Sub(f.scale,{1,1,1}))>1.e-10f)generated=false;
        }
        for(const auto& n:out.nodes){
            if(!n.hasMatrix)generated=false;
            for(size_t k=0;k<12;++k)if(std::fabs(n.matrix[k]-(k==0||k==4||k==8?1.f:0.f))>1.e-6f)generated=false;
            if(std::count_if(out.frames.begin(),out.frames.end(),[&](const Frame& f){return f.name==n.name;})!=1)generated=false;
        }
        if(generated){for(size_t i=1;i<out.frames.size();++i)out.frames[i].parent=0;out.migratedFlatGenerated=true;}
    }
    error.clear();return true;
}
inline bool Resolve(const Scene& s,const std::string& objectName,Selection& selection,std::string& error){
    selection={};
    std::vector<int> candidates;
    if(!s.frames.empty()){
        for(size_t i=0;i<s.frames.size();++i)if(objectName.empty()?s.frames[i].parent<0:s.frames[i].name==objectName)candidates.push_back(static_cast<int>(i));
        if(objectName.empty()){
            // AIR Set.peek() has no portable dictionary ordering. For libraries
            // without an explicit object, use the first declared usable mesh root.
            // Do not reject the entire prop just because a sibling root exists.
            candidates.erase(std::remove_if(candidates.begin(),candidates.end(),[&](int i){
                return std::none_of(s.nodes.begin(),s.nodes.end(),[&](const Node& n){return n.name==s.frames[i].name&&!n.faces.empty();});
            }),candidates.end());
            selection.usedRootFallback=candidates.size()>1;
        }
        if(candidates.empty()||(!objectName.empty()&&candidates.size()!=1)){error="3DS mesh object is missing or ambiguous.";return false;}
        selection.frame=candidates.front();
        const auto& name=s.frames[static_cast<size_t>(selection.frame)].name;
        size_t count=0;for(size_t i=0;i<s.nodes.size();++i)if(s.nodes[i].name==name){selection.node=i;++count;}
        if(count!=1){error="Selected 3DS root is not a unique mesh.";return false;}
    }else{
        selection.frame=-1;size_t count=0;
        for(size_t i=0;i<s.nodes.size();++i)if((objectName.empty()&&!s.nodes[i].faces.empty())||s.nodes[i].name==objectName){if(count==0)selection.node=i;++count;}
        selection.usedRootFallback=objectName.empty()&&count>1;
        if(count==0||(!objectName.empty()&&count!=1)){error="3DS mesh object is missing or ambiguous.";return false;}
    }
    if(s.nodes[selection.node].faces.empty()){error="Selected 3DS object has no faces.";return false;}
    error.clear();return true;
}
inline const Frame* SelectedFrame(const Scene& s,const Selection& selected){return selected.frame<0?nullptr:&s.frames[static_cast<size_t>(selected.frame)];}
} // namespace Native3DSScene
