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
inline constexpr const char* NoNativeHelpersError="No native plane/box/triangle helpers in this model.";
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
    // The native 3DS vertex lists are stored in authoring/world coordinates.
    // 0x4160 gives an oriented object basis. Both are needed: the visual pivot
    // defines the map prop's LOCAL frame; each box helper defines the axes in
    // which the source CollisionBox derives its dimensions. Subtracting only
    // the pivot translation worked for identity matrices but silently lost
    // the geometry of rotated original helpers.
    struct Frame { V x,y,z,origin; };
    const auto frame=[](const Node& n,Frame& out)->bool {
        if(!n.hasMatrix)return false;
        out={{n.matrix[0],n.matrix[1],n.matrix[2]},
             {n.matrix[3],n.matrix[4],n.matrix[5]},
             {n.matrix[6],n.matrix[7],n.matrix[8]},
             {n.matrix[9],n.matrix[10],n.matrix[11]}};
        if(!Finite(out.x)||!Finite(out.y)||!Finite(out.z)||!Finite(out.origin))return false;
        // Explicitly refuse scale, shear, singular and mirrored matrices.
        // The corresponding source-game decomposition is not yet validated.
        constexpr float tolerance=0.003f;
        return std::fabs(Norm(out.x)-1.f)<tolerance &&
            std::fabs(Norm(out.y)-1.f)<tolerance &&
            std::fabs(Norm(out.z)-1.f)<tolerance &&
            std::fabs(Dot(out.x,out.y))<tolerance &&
            std::fabs(Dot(out.x,out.z))<tolerance &&
            std::fabs(Dot(out.y,out.z))<tolerance &&
            Dot(Cross(out.x,out.y),out.z)>1.f-tolerance;
    };
    Frame visualFrame{};
    if(!frame(*visual,visualFrame)) {
        result.error="Unverified scale/shear/mirror in 3DS visual pivot; collision export refused.";
        return result;
    }
    const V origin{visual->matrix[9],visual->matrix[10],visual->matrix[11]};
    if(!Finite(origin)) {result.error="Non-finite visual pivot.";return result;}
    const auto toLocal=[&](V world) {
        const V p=Sub(world,origin);
        return V{Dot(p,visualFrame.x),Dot(p,visualFrame.y),Dot(p,visualFrame.z)};
    };
    const auto dirToLocal=[&](V world) {
        return V{Dot(world,visualFrame.x),Dot(world,visualFrame.y),Dot(world,visualFrame.z)};
    };
    result.visualAnchor=visual->name;
    for(const auto& n:nodes) {
        if(&n==visual)continue;
        const bool plane=Prefix(n.name,"plane"),tri=Prefix(n.name,"tri"),box=Prefix(n.name,"box");
        if(Prefix(n.name,"occl")){++result.occlusionHelpers;continue;}
        if(!plane&&!tri&&!box)continue;
        Frame helper{};
        if(!frame(n,helper)) {
            // Plane and triangle vertices in the 3DS 0x4110 chunk are already
            // stored in authoring/world coordinates. Their native collision
            // surface is reconstructed from those vertices below, not from
            // the 0x4160 helper matrix. Original Fabr Tower planes carry a
            // 1.64 Z scale in that matrix despite forming valid rectangles.
            // Do NOT relax this for boxes: their matrix defines box axes and
            // dimensions, and a non-rigid decomposition needs its own proof.
            if(box || !n.hasMatrix || !std::all_of(n.matrix.begin(),n.matrix.end(),
                [](float v){return std::isfinite(v) && std::fabs(v)<1.e7f;})) {
                result.error="Unverified scale/shear/mirror on 3DS collision helper: "+n.name;
                return result;
            }
        }
        for(auto face:n.faces)for(auto id:face)if(id>=n.vertices.size()) {result.error="Invalid helper face indices.";return result;}
        if(box) {
            // Original CollisionBox.parse takes the axis-aligned source
            // vertex bounds (including duplicated corner vertices), then
            // centers them before applying the placed prop's transform.
            // Validated against GTanks-authored NuBu 3 map XML: Box07/Box08.
            if(n.vertices.size()<8||n.faces.size()<12) {result.error="Incomplete 3DS box helper: "+n.name;return result;}
            // The bounding box is axis-aligned in the HELPER frame, not in
            // 3DS world axes. Billboard Box113: world X=100,Y=45 but its
            // original XML size is X=45,Y=100 and yaw=-pi/2.
            const std::array<V,3> axes{helper.x,helper.y,helper.z};
            std::array<float,3> lo{},hi{};
            for(int k=0;k<3;++k)lo[static_cast<size_t>(k)]=hi[static_cast<size_t>(k)]=Dot(n.vertices.front(),axes[static_cast<size_t>(k)]);
            for(const V v:n.vertices)for(int k=0;k<3;++k) {
                const size_t at=static_cast<size_t>(k);
                const float projected=Dot(v,axes[at]);
                lo[at]=std::min(lo[at],projected);hi[at]=std::max(hi[at],projected);
            }
            const V dimensions{hi[0]-lo[0],hi[1]-lo[1],hi[2]-lo[2]};
            if(dimensions.x<.01f||dimensions.y<.01f||dimensions.z<.01f) {
                result.error="Degenerate 3DS box helper: "+n.name;return result;
            }
            // A box helper must actually represent all eight corners of an
            // axis-aligned box; otherwise AABB conversion changes gameplay.
            std::array<bool,8> seen{};
            for(const V v:n.vertices) {
                int code=0;
                const float a[3]={Dot(v,helper.x),Dot(v,helper.y),Dot(v,helper.z)};
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
            const V center=Add(Add(Mul(helper.x,(lo[0]+hi[0])*.5f),
                                   Mul(helper.y,(lo[1]+hi[1])*.5f)),
                                   Mul(helper.z,(lo[2]+hi[2])*.5f));
            out.offset=toLocal(center);
            out.size=dimensions;
            const V bx=dirToLocal(helper.x),by=dirToLocal(helper.y),bz=dirToLocal(helper.z);
            out.rotation=Euler(bx,by,bz);
            if(!Finite(out.offset)||!Finite(out.size)||!Finite(out.rotation)) {
                result.error="Invalid box geometry/rotation: "+n.name;return result;
            }
            result.boxes.push_back(out);
        } else if(tri) {
            if(n.vertices.size()!=3||n.faces.size()!=1) {result.error="Unsupported triangle helper: "+n.name;return result;}
            const auto f=n.faces[0];
            const V a=toLocal(n.vertices[f[0]]),b=toLocal(n.vertices[f[1]]),c=toLocal(n.vertices[f[2]]);
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
            std::array<V,4> v{};
            for(size_t j=0;j<4;++j)v[j]=toLocal(n.vertices[j]);
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
            out.offset=Mul(Add(Add(v[0],v[1]),Add(v[2],v[3])),.25f);
            out.rotation=Euler(x,y,normal);out.width=width;out.length=length;
            if(!Finite(out.rotation)||!Finite(out.offset)) {result.error="Plane rotation is invalid.";return result;}
            result.planes.push_back(out);
        }
        if(result.planes.size()+result.triangles.size()+result.boxes.size()>2048){result.error="Excessive 3DS collision helper count.";return result;}
    }
    if(result.planes.empty()&&result.triangles.empty()&&result.boxes.empty())result.error=NoNativeHelpersError;
    if(!result.error.empty()) {result.planes.clear();result.triangles.clear();result.boxes.clear();}
    return result;
}
} // namespace NativeCollisionImport
