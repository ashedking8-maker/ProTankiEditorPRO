#pragma once
// Small deterministic 3DS mesh writer for an ISOLATED new library asset.
// All positions of the visual input use the editor's Y-up (x,z,y) basis;
// collision boxes are already in legacy XML Z-up coordinates. This module
// does not know anything about game trigger semantics.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

namespace Native3DSWriter {
using Bytes=std::vector<std::uint8_t>;
struct Vertex { float x{},y{},z{},u{},v{}; };
struct Part { size_t firstIndex{},indexCount{};std::string material,texture; };
struct MaterialOverride {
    bool enabled{}; // default: preserve the original AFFF material byte-for-byte
    std::array<float,3> ambient{.5f,.5f,.5f},diffuse{1.f,1.f,1.f},specular{0.f,0.f,0.f};
    float shininess{},transparency{}; // normalized 0..1, encoded as 3DS percent
    std::uint16_t shading{3}; // native 3DS flat=1, Gouraud=2, Phong=3
    bool twoSided{};
};
struct Box { std::array<float,3> min{},max{}; };
using Triangle=std::array<std::array<float,3>,3>;
struct Model {
    std::string visualName;
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
    std::vector<Part> parts;
    // 0x4150 is PER FACE, not per vertex; empty means explicit flat masks.
    std::vector<std::uint32_t> smoothingGroups;
    // Complete original 0xAFFF chunks, one per Part, preserve unknown fields.
    std::vector<Bytes> nativeMaterials;
    MaterialOverride materialOverride;
    std::vector<Box> boxes;
    std::vector<Triangle> triangles;
};
inline void U16(Bytes& b,std::uint16_t v){b.push_back(static_cast<std::uint8_t>(v));b.push_back(static_cast<std::uint8_t>(v>>8));}
inline void U32(Bytes& b,std::uint32_t v){for(int i=0;i<4;++i)b.push_back(static_cast<std::uint8_t>(v>>(i*8)));}
inline void Float(Bytes& b,float v){std::uint32_t bits{};std::memcpy(&bits,&v,4);U32(b,bits);}
inline void CStr(Bytes& b,const std::string& v){b.insert(b.end(),v.begin(),v.end());b.push_back(0);}
inline Bytes Chunk(std::uint16_t id,const Bytes& payload){
    Bytes b; b.reserve(payload.size()+6);U16(b,id);U32(b,static_cast<std::uint32_t>(payload.size()+6));
    b.insert(b.end(),payload.begin(),payload.end());return b;
}
inline void Add(Bytes& dst,const Bytes& chunk){dst.insert(dst.end(),chunk.begin(),chunk.end());}
inline bool SafeAscii(const std::string& value,size_t max){
    if(value.empty()||value.size()>max)return false;
    for(const unsigned char c:value)if(c<32||c>126||c=='/'||c=='\\'||c=='"'||c==':'||c=='*'||c=='?'||c=='<'||c=='>'||c=='|')return false;
    return value!="." && value!="..";
}
inline Bytes IdentityMatrix(){Bytes v;for(int i=0;i<12;++i)Float(v,i==0||i==4||i==8?1.f:0.f);return Chunk(0x4160,v);}
inline Bytes Object(const std::string& name,const std::vector<std::array<float,3>>& vertices,
                    const std::vector<std::array<std::uint16_t,3>>& faces,
                    const std::vector<std::array<float,2>>& uv={},
                    const std::vector<Part>& parts={},
                    const std::vector<std::uint32_t>& smoothing={}) {
    Bytes mesh,verts,face,points;
    U16(verts,static_cast<std::uint16_t>(vertices.size()));
    for(const auto& v:vertices)for(float f:v)Float(verts,f);
    Add(mesh,Chunk(0x4110,verts));
    if(!uv.empty()){
        U16(points,static_cast<std::uint16_t>(uv.size()));
        for(const auto& p:uv){Float(points,p[0]);Float(points,p[1]);}
        Add(mesh,Chunk(0x4140,points));
    }
    Add(mesh,IdentityMatrix());
    U16(face,static_cast<std::uint16_t>(faces.size()));
    for(const auto& f:faces){for(auto i:f)U16(face,i);U16(face,0);}
    for(const auto& part:parts){
        Bytes assignment;CStr(assignment,part.material);
        U16(assignment,static_cast<std::uint16_t>(part.indexCount/3));
        for(size_t i=part.firstIndex/3;i<(part.firstIndex+part.indexCount)/3;++i)U16(assignment,static_cast<std::uint16_t>(i));
        Add(face,Chunk(0x4130,assignment));
    }
    if(!smoothing.empty()) {
        Bytes masks;for(const auto group:smoothing)U32(masks,group);
        Add(face,Chunk(0x4150,masks));
    }
    Add(mesh,Chunk(0x4120,face));
    Bytes obj;CStr(obj,name);Add(obj,Chunk(0x4100,mesh));return Chunk(0x4000,obj);
}
inline Bytes Material(const Part& part){
    Bytes body,name,map,tex;
    CStr(name,part.material);Add(body,Chunk(0xA000,name));
    CStr(tex,part.texture);Add(map,Chunk(0xA300,tex));Add(body,Chunk(0xA200,map));
    return Chunk(0xAFFF,body);
}
inline Bytes RGB(const std::array<float,3>& value){
    Bytes rgb;for(float c:value)Float(rgb,c);
    return Chunk(0x0010,rgb);
}
inline Bytes Percentage(float value){
    Bytes percent;U16(percent,static_cast<std::uint16_t>(std::lround(value*100.f)));
    return Chunk(0x0030,percent);
}
inline bool OverrideMaterial(const Bytes& original,const MaterialOverride& options,Bytes& result){
    if(original.size()<6||original[0]!=0xff||original[1]!=0xaf)return false;
    const auto read32=[](const Bytes& b,size_t p){return std::uint32_t(b[p])|
        (std::uint32_t(b[p+1])<<8)|(std::uint32_t(b[p+2])<<16)|(std::uint32_t(b[p+3])<<24);};
    if(read32(original,2)!=original.size())return false;
    Bytes body;size_t p=6;
    while(p<original.size()){
        if(original.size()-p<6)return false;
        const auto size=read32(original,p+2);
        if(size<6||size>original.size()-p)return false;
        const auto id=std::uint16_t(original[p])|(std::uint16_t(original[p+1])<<8);
        // Replace ONLY known shading fields. Other 3DS material properties,
        // texture mapping, vendor extras and original material name survive.
        if(id!=0xA010 && id!=0xA020 && id!=0xA030 && id!=0xA040 &&
           id!=0xA050 && id!=0xA081 && id!=0xA100)
            body.insert(body.end(),original.begin()+static_cast<std::ptrdiff_t>(p),
                        original.begin()+static_cast<std::ptrdiff_t>(p+size));
        p+=size;
    }
    Add(body,Chunk(0xA010,RGB(options.ambient)));
    Add(body,Chunk(0xA020,RGB(options.diffuse)));
    Add(body,Chunk(0xA030,RGB(options.specular)));
    Add(body,Chunk(0xA040,Percentage(options.shininess)));
    Add(body,Chunk(0xA050,Percentage(options.transparency)));
    Bytes mode;U16(mode,options.shading);Add(body,Chunk(0xA100,mode));
    if(options.twoSided)Add(body,Chunk(0xA081,{}));
    result=Chunk(0xAFFF,body);return true;
}
inline Bytes KeyTrack(const std::vector<float>& values){
    Bytes b;U16(b,0);U32(b,0);U32(b,0);U32(b,1);U32(b,0);U16(b,0);
    for(float f:values)Float(b,f);
    return b;
}
inline Bytes KeyNode(const std::string& name,std::uint16_t index){
    Bytes node,id,header,pivot;
    U16(id,index);Add(node,Chunk(0xB030,id));
    CStr(header,name);U16(header,0);U16(header,0);U16(header,0xffff);Add(node,Chunk(0xB010,header));
    for(int i=0;i<3;++i)Float(pivot,0.f);
    Add(node,Chunk(0xB013,pivot));
    Add(node,Chunk(0xB020,KeyTrack({0.f,0.f,0.f})));
    Add(node,Chunk(0xB021,KeyTrack({0.f,0.f,0.f,1.f})));
    Add(node,Chunk(0xB022,KeyTrack({1.f,1.f,1.f})));
    return Chunk(0xB002,node);
}
inline bool Write(const Model& m,Bytes& result,std::string& error){
    if(!SafeAscii(m.visualName,50)||m.vertices.empty()||m.vertices.size()>65535||
       m.indices.empty()||m.indices.size()%3||m.indices.size()/3>65535||m.parts.empty()||
       m.boxes.size()>128||m.triangles.size()>2048||m.boxes.size()+m.triangles.size()>2048){error="3DS mesh exceeds format limits or has no material/geometry.";return false;}
    for(const auto& v:m.vertices) {
        const float f[]={v.x,v.y,v.z,v.u,v.v};
        for(float x:f)if(!std::isfinite(x)||std::abs(x)>1.e7f){error="Non-finite/out-of-range visual vertex or UV.";return false;}
    }
    for(auto i:m.indices)if(i>=m.vertices.size()){error="A visual face refers to an invalid vertex.";return false;}
    if(!m.smoothingGroups.empty()&&m.smoothingGroups.size()!=m.indices.size()/3){
        error="3DS smoothing mask count must equal visual face count.";return false;
    }
    if(!m.nativeMaterials.empty()&&m.nativeMaterials.size()!=m.parts.size()){
        error="Original material count does not match exported parts.";return false;
    }
    if(m.materialOverride.enabled){
        const auto& o=m.materialOverride;
        for(const auto& rgb:{o.ambient,o.diffuse,o.specular})for(float c:rgb)
            if(!std::isfinite(c)||c<0.f||c>1.f){error="Invalid native material color.";return false;}
        if(!std::isfinite(o.shininess)||o.shininess<0.f||o.shininess>1.f||
           !std::isfinite(o.transparency)||o.transparency<0.f||o.transparency>1.f||
           o.shading<1||o.shading>3){error="Invalid native material shading values.";return false;}
        if(m.nativeMaterials.empty()) {error="Material override requires an original source material.";return false;}
    }
    size_t expected=0;
    for(const auto& p:m.parts){
        if(!SafeAscii(p.material,50)||!SafeAscii(p.texture,120)||p.firstIndex!=expected||
           p.indexCount==0||p.indexCount%3){error="Invalid or unsupported 3DS material layout.";return false;}
        expected+=p.indexCount;
    }
    if(expected!=m.indices.size()){error="3DS material groups do not cover all faces.";return false;}
    for(const auto& box:m.boxes)for(int axis=0;axis<3;++axis)if(!std::isfinite(box.min[axis])||
        !std::isfinite(box.max[axis])||box.max[axis]-box.min[axis]<.01f||
        std::abs(box.min[axis])>1.e6f||std::abs(box.max[axis])>1.e6f){error="Invalid native collision box.";return false;}
    for(const auto& t:m.triangles){
        for(const auto& v:t)for(float c:v)if(!std::isfinite(c)||std::abs(c)>1.e6f){error="Invalid terrain collision triangle.";return false;}
        const auto sub=[](const auto& a,const auto& b){return std::array<float,3>{a[0]-b[0],a[1]-b[1],a[2]-b[2]};};
        const auto a=sub(t[1],t[0]),b=sub(t[2],t[0]);
        const float nz=a[0]*b[1]-a[1]*b[0];if(nz<.001f){error="Inverted/degenerate native terrain triangle.";return false;}
    }
    Bytes edit,keys;
    for(size_t i=0;i<m.parts.size();++i){
        Bytes material;
        if(m.nativeMaterials.empty())material=Material(m.parts[i]);
        else if(!m.materialOverride.enabled)material=m.nativeMaterials[i];
        else if(!OverrideMaterial(m.nativeMaterials[i],m.materialOverride,material)){
            error="Cannot safely edit original 3DS material chunk.";return false;
        }
        Add(edit,material);
    }
    std::vector<std::array<float,3>> vertices;std::vector<std::array<float,2>> uv;
    vertices.reserve(m.vertices.size());uv.reserve(m.vertices.size());
    // Swapping Y/Z mirrors handedness; reverse face winding as we undo the
    // editor basis conversion, including for parts exported from a 3DS source.
    for(const auto& v:m.vertices){vertices.push_back({v.x,v.z,v.y});uv.push_back({v.u,1.f-v.v});}
    std::vector<std::array<std::uint16_t,3>> faces;faces.reserve(m.indices.size()/3);
    for(size_t i=0;i<m.indices.size();i+=3)faces.push_back({
        static_cast<std::uint16_t>(m.indices[i]),static_cast<std::uint16_t>(m.indices[i+2]),
        static_cast<std::uint16_t>(m.indices[i+1])});
    Add(edit,Object(m.visualName,vertices,faces,uv,m.parts,m.smoothingGroups));Add(keys,KeyNode(m.visualName,0));
    static constexpr std::array<std::array<std::uint16_t,3>,12> cubeFaces{{
        {{0,2,1}},{{1,2,3}},{{4,5,6}},{{5,7,6}},{{0,1,4}},{{1,5,4}},
        {{2,6,3}},{{3,6,7}},{{0,4,2}},{{2,4,6}},{{1,3,5}},{{3,7,5}}
    }};
    for(size_t i=0;i<m.boxes.size();++i){
        const auto& b=m.boxes[i];std::vector<std::array<float,3>> corners;
        for(int z=0;z<2;++z)for(int y=0;y<2;++y)for(int x=0;x<2;++x)
            corners.push_back({x?b.max[0]:b.min[0],y?b.max[1]:b.min[1],z?b.max[2]:b.min[2]});
        const auto name="BoxPT"+std::to_string(i+1);
        Add(edit,Object(name,corners,std::vector<std::array<std::uint16_t,3>>(cubeFaces.begin(),cubeFaces.end())));
        Add(keys,KeyNode(name,static_cast<std::uint16_t>(i+1)));
    }
    for(size_t i=0;i<m.triangles.size();++i){
        const auto name="tri_PT"+std::to_string(i+1);
        const auto& t=m.triangles[i];
        Add(edit,Object(name,{t[0],t[1],t[2]},{{{0,1,2}}}));
        Add(keys,KeyNode(name,static_cast<std::uint16_t>(m.boxes.size()+i+1)));
    }
    Bytes header;U16(header,5);CStr(header,"ANIM");U32(header,100);Bytes kf;
    Add(kf,Chunk(0xB00A,header));Add(kf,keys);
    Bytes root;Add(root,Chunk(0x3D3D,edit));Add(root,Chunk(0xB000,kf));
    result=Chunk(0x4D4D,root);error.clear();return true;
}
} // namespace Native3DSWriter
