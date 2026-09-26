#pragma once
// Pure CPU 3DS helper-geometry reader for native map collision authoring.
// It reads ORIGINAL vertices/faces, never infers colliders from the visual mesh.
// Unsupported/corrupt helpers fail closed instead of creating plausible but
// unverified invisible walls. GLB authoring drafts do not use this game exporter.
#include "VerifiedCollisionTemplates.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
#include <iterator>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

namespace NativeCollisionImport {
using V = DirectX::XMFLOAT3;
struct Result {
    std::vector<VerifiedCollisionTemplates::Plane> planes;
    std::vector<VerifiedCollisionTemplates::Triangle> triangles;
    struct Box { V offset{}, rotation{}, size{}; };
    std::vector<Box> boxes;
    size_t occlusionHelpers{};
    std::string visualAnchor;
    std::string error;
    bool Valid() const { return error.empty() && (!planes.empty() || !triangles.empty() || !boxes.empty()); }
};
inline V Add(V a,V b) {return {a.x+b.x,a.y+b.y,a.z+b.z};}
inline V Sub(V a,V b) {return {a.x-b.x,a.y-b.y,a.z-b.z};}
inline V Mul(V a,float v) {return {a.x*v,a.y*v,a.z*v};}
inline float Dot(V a,V b) {return a.x*b.x+a.y*b.y+a.z*b.z;}
inline V Cross(V a,V b) {return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
inline float Norm(V a) {return std::sqrt(Dot(a,a));}
inline bool Finite(V a) {return std::isfinite(a.x)&&std::isfinite(a.y)&&std::isfinite(a.z) &&
    std::fabs(a.x)<1.e7f&&std::fabs(a.y)<1.e7f&&std::fabs(a.z)<1.e7f;}
inline V Unit(V a) {return Mul(a,1.f/Norm(a));}
inline bool Prefix(std::string a,const char* b) {
    std::transform(a.begin(),a.end(),a.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
    return a.rfind(b,0)==0;
}
// Columns of a RIGHT-HANDED local-to-world rotation matrix, with world Z up.
// The native format's transform is Rx * Ry * Rz in DirectX row-vector order.
inline V Euler(V x,V y,V n) {
    const float pitch=std::asin(std::clamp(-x.z,-1.f,1.f));
    if (std::fabs(std::cos(pitch))<1.e-5f)
        return {0.f,pitch,std::atan2(-y.x,y.y)};
    return {std::atan2(y.z,n.z),pitch,std::atan2(x.y,x.x)};
}
struct Node {
    std::string name;
    std::vector<V> vertices;
    std::vector<std::array<std::uint16_t,3>> faces;
    std::array<float,12> matrix{};
    bool hasMatrix{};
};
inline Result Read(const std::filesystem::path& file) {
    Result result;
    std::error_code ec;
    const auto fileSize=std::filesystem::file_size(file,ec);
    if(ec||fileSize<6||fileSize>64ull*1024*1024) {result.error="3DS helper source is missing or exceeds 64 MB.";return result;}
    std::ifstream in(file,std::ios::binary);
    if(!in) {result.error="3DS helper source cannot be opened.";return result;}
    std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)),{});
    if(bytes.size()!=fileSize) {result.error="Short 3DS read.";return result;}
    auto u16=[&](size_t p)->std::uint16_t {return std::uint16_t(bytes[p])|(std::uint16_t(bytes[p+1])<<8);};
    auto u32=[&](size_t p)->std::uint32_t {return std::uint32_t(u16(p))|(std::uint32_t(u16(p+2))<<16);};
    auto f32=[&](size_t p)->float {const auto bits=u32(p);float v{};std::memcpy(&v,&bits,sizeof v);return v;};
    if(u16(0)!=0x4d4d||u32(2)!=bytes.size()) {result.error="Invalid 3DS root chunk.";return result;}
    std::vector<Node> nodes;
    bool failed=false;
    // The 3DS mesh and object chunks are nested; vertex/face/matrix chunks
    // always live under their own object. Restrict recursion to known containers.
    auto walk=[&](auto&& self,size_t start,size_t end,int object,int depth)->void {
        if(depth>18) {failed=true;return;}
        while(start<end&&!failed) {
            if(end-start<6) {failed=true;break;}
            const auto tag=u16(start);const auto length=u32(start+2);
            if(length<6||length>end-start) {failed=true;break;}
            size_t a=start+6,b=start+length;
            if(tag==0x4000) {
                const auto zero=std::find(bytes.begin()+static_cast<std::ptrdiff_t>(a),
                    bytes.begin()+static_cast<std::ptrdiff_t>(b),std::uint8_t{0});
                if(zero==bytes.begin()+static_cast<std::ptrdiff_t>(b)||nodes.size()>=256){failed=true;break;}
                const size_t pos=static_cast<size_t>(zero-bytes.begin());
                nodes.push_back({});nodes.back().name=std::string(reinterpret_cast<const char*>(bytes.data()+a),pos-a);
                self(self,pos+1,b,static_cast<int>(nodes.size()-1),depth+1);
            } else if(tag==0x4d4d||tag==0x3d3d||tag==0x4100) self(self,a,b,object,depth+1);
            else if(object>=0) {
                auto& node=nodes[static_cast<size_t>(object)];
                if(tag==0x4110) {
                    if(b-a<2) {failed=true;break;}
                    const size_t count=u16(a);
                    if(count>65535||count>(b-a-2)/12||!node.vertices.empty()) {failed=true;break;}
                    node.vertices.reserve(count);
                    for(size_t i=0;i<count;++i) {
                        const V v{f32(a+2+12*i),f32(a+6+12*i),f32(a+10+12*i)};
                        if(!Finite(v)){failed=true;break;}
                        node.vertices.push_back(v);
                    }
                } else if(tag==0x4120) {
                    if(b-a<2) {failed=true;break;}
                    const size_t count=u16(a);
                    if(count>(b-a-2)/8||!node.faces.empty()) {failed=true;break;}
                    node.faces.reserve(count);
                    for(size_t i=0;i<count;++i) node.faces.push_back({u16(a+2+i*8),u16(a+4+i*8),u16(a+6+i*8)});
                } else if(tag==0x4160) {
                    if(b-a<48||node.hasMatrix){failed=true;break;}
                    for(size_t i=0;i<12;++i)node.matrix[i]=f32(a+4*i);
                    node.hasMatrix=true;
                }
            }
            start=b;
        }
    };
    walk(walk,0,bytes.size(),-1,0);
    if(failed||nodes.empty()) {result.error="Malformed 3DS mesh/helper chunks.";return result;}
    std::string stem=file.stem().string();
    std::transform(stem.begin(),stem.end(),stem.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
    const Node* visual=nullptr;
    size_t best=0;
    for(const auto& node:nodes) {
        if(Prefix(node.name,"tri")||Prefix(node.name,"plane")||Prefix(node.name,"box")||Prefix(node.name,"occl"))continue;
        std::string lower=node.name;
        std::transform(lower.begin(),lower.end(),lower.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
        if(!visual||lower==stem||node.faces.size()>best) {visual=&node;best=node.faces.size();if(lower==stem)break;}
    }
    if(!visual||!visual->hasMatrix) {result.error="3DS visual anchor/pivot is not available.";return result;}
    // Original meshes examined here store their vertices in authoring/world
    // coordinates. Only the translation of the visual anchor is subtracted.
    // Nonidentity 3DS transforms need separate source-format validation.
    const auto identity=[](const Node& n) {
        if(!n.hasMatrix)return false;
        for(int i=0;i<9;++i) if(std::fabs(n.matrix[i]-(i%4==0?1.f:0.f))>1.e-4f)return false;
        return true;
    };
    if(!identity(*visual)) {result.error="Unverified nonidentity 3DS visual pivot; collision export refused.";return result;}
    const V origin{visual->matrix[9],visual->matrix[10],visual->matrix[11]};
    if(!Finite(origin)) {result.error="Non-finite visual pivot.";return result;}
    result.visualAnchor=visual->name;
    for(const auto& n:nodes) {
        if(&n==visual)continue;
        const bool plane=Prefix(n.name,"plane"),tri=Prefix(n.name,"tri"),box=Prefix(n.name,"box");
        if(Prefix(n.name,"occl")){++result.occlusionHelpers;continue;}
        if(!plane&&!tri&&!box)continue;
        if(!identity(n)) {result.error="Nonidentity transform on collision helper: "+n.name;return result;}
        for(auto face:n.faces)for(auto id:face)if(id>=n.vertices.size()) {result.error="Invalid helper face indices.";return result;}
        if(box) {
            // Original CollisionBox.parse takes the axis-aligned source
            // vertex bounds (including duplicated corner vertices), then
            // centers them before applying the placed prop's transform.
            // Validated against GTanks-authored NuBu 3 map XML: Box07/Box08.
            if(n.vertices.size()<8||n.faces.size()<12) {result.error="Incomplete 3DS box helper: "+n.name;return result;}
            V min=n.vertices.front(),max=min;
            for(const V v:n.vertices) {
                min={std::min(min.x,v.x),std::min(min.y,v.y),std::min(min.z,v.z)};
                max={std::max(max.x,v.x),std::max(max.y,v.y),std::max(max.z,v.z)};
            }
            const V dimensions=Sub(max,min);
            if(dimensions.x<.01f||dimensions.y<.01f||dimensions.z<.01f) {
                result.error="Degenerate 3DS box helper: "+n.name;return result;
            }
            // A box helper must actually represent all eight corners of an
            // axis-aligned box; otherwise AABB conversion changes gameplay.
            std::array<bool,8> seen{};
            for(const V v:n.vertices) {
                int code=0;
                const float a[3]={v.x,v.y,v.z},lo[3]={min.x,min.y,min.z},hi[3]={max.x,max.y,max.z};
                for(int k=0;k<3;++k) {
                    if(std::fabs(a[k]-hi[k])<.1f)code|=1<<k;
                    else if(std::fabs(a[k]-lo[k])>=.1f) {
                        result.error="Non-box-shaped helper: "+n.name;return result;
                    }
                }
                seen[static_cast<size_t>(code)]=true;
            }
            if(std::find(seen.begin(),seen.end(),false)!=seen.end()) {
                result.error="Missing box corners: "+n.name;return result;
            }
            Result::Box out;
            out.offset=Sub(Mul(Add(min,max),.5f),origin);
            out.size=dimensions;
            if(!Finite(out.offset)||!Finite(out.size)) {result.error="Invalid box geometry: "+n.name;return result;}
            result.boxes.push_back(out);
        } else if(tri) {
            if(n.vertices.size()!=3||n.faces.size()!=1) {result.error="Unsupported triangle helper: "+n.name;return result;}
            const auto f=n.faces[0];
            const V a=Sub(n.vertices[f[0]],origin),b=Sub(n.vertices[f[1]],origin),c=Sub(n.vertices[f[2]],origin);
            V x=Sub(b,a),cross=Cross(x,Sub(c,a));
            if(Norm(x)<.01f||Norm(cross)<.01f) {result.error="Degenerate triangle helper: "+n.name;return result;}
            x=Unit(x);const V z=Unit(cross),y=Cross(z,x),center=Mul(Add(Add(a,b),c),1.f/3.f);
            auto local=[&](V p){p=Sub(p,center);return V{Dot(p,x),Dot(p,y),0.f};};
            VerifiedCollisionTemplates::Triangle out;
            out.offset=center;out.rotation=Euler(x,y,z);out.v0=local(a);out.v1=local(b);out.v2=local(c);
            if(!Finite(out.rotation)||!Finite(out.offset)) {result.error="Triangle rotation is invalid.";return result;}
            result.triangles.push_back(out);
        } else {
            if(n.vertices.size()!=4||n.faces.size()!=2) {result.error="Unsupported plane helper: "+n.name;return result;}
            const auto& v=n.vertices;
            // Four vertices must form a rectangle in 3D. A diagonal is not an edge.
            int i1=-1,i2=-1;
            for(int i=1;i<4&&i1<0;++i)for(int j=i+1;j<4;++j){
                const V e1=Sub(v[i],v[0]),e2=Sub(v[j],v[0]);
                const float a=Norm(e1),b=Norm(e2);
                if(a<.01f||b<.01f||std::fabs(Dot(e1,e2))>1.e-3f*a*b)continue;
                const int other=6-i-j; // indices are 0+1+2+3 = 6
                if(Norm(Sub(Add(v[0],Add(e1,e2)),v[other]))<.05f) {i1=i;i2=j;break;}
            }
            if(i1<0) {result.error="Nonrectangular 3DS plane helper: "+n.name;return result;}
            V x=Sub(v[i1],v[0]),y=Sub(v[i2],v[0]);
            const float width=Norm(x),length=Norm(y);
            x=Unit(x);y=Unit(y);
            V normal=Unit(Cross(x,y));
            const auto f=n.faces[0];
            const V faceN=Cross(Sub(v[f[1]],v[f[0]]),Sub(v[f[2]],v[f[0]]));
            if(Dot(faceN,normal)<0) {x=Mul(x,-1.f);normal=Mul(normal,-1.f);}
            VerifiedCollisionTemplates::Plane out;
            out.offset=Sub(Mul(Add(Add(v[0],v[1]),Add(v[2],v[3])),.25f),origin);
            out.rotation=Euler(x,y,normal);out.width=width;out.length=length;
            if(!Finite(out.rotation)||!Finite(out.offset)) {result.error="Plane rotation is invalid.";return result;}
            result.planes.push_back(out);
        }
        if(result.planes.size()+result.triangles.size()+result.boxes.size()>2048){result.error="Excessive 3DS collision helper count.";return result;}
    }
    if(result.planes.empty()&&result.triangles.empty()&&result.boxes.empty())result.error="No native plane/box/triangle helpers in this model.";
    if(!result.error.empty()) {result.planes.clear();result.triangles.clear();result.boxes.clear();}
    return result;
}
} // namespace NativeCollisionImport
