#pragma once
// Small deterministic 3DS mesh writer for an ISOLATED new library asset.
// All positions of the visual input use the editor's Y-up (x,z,y) basis;
// collision boxes are already in legacy XML Z-up coordinates. This module
// does not know anything about game trigger semantics or model materials.
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
struct Box { std::array<float,3> min{},max{}; };
struct Model {
    std::string visualName;
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
    std::vector<Part> parts;
    std::vector<Box> boxes;
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
                    const std::vector<Part>& parts={}) {
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
    Add(mesh,Chunk(0x4120,face));
    Bytes obj;CStr(obj,name);Add(obj,Chunk(0x4100,mesh));return Chunk(0x4000,obj);
}
inline Bytes Material(const Part& part){
    Bytes body,name,map,tex;
    CStr(name,part.material);Add(body,Chunk(0xA000,name));
    CStr(tex,part.texture);Add(map,Chunk(0xA300,tex));Add(body,Chunk(0xA200,map));
    return Chunk(0xAFFF,body);
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
       m.boxes.size()>128){error="3DS mesh exceeds format limits or has no material/geometry.";return false;}
    for(const auto& v:m.vertices) {
        const float f[]={v.x,v.y,v.z,v.u,v.v};
        for(float x:f)if(!std::isfinite(x)||std::abs(x)>1.e7f){error="Non-finite/out-of-range visual vertex or UV.";return false;}
    }
    for(auto i:m.indices)if(i>=m.vertices.size()){error="A visual face refers to an invalid vertex.";return false;}
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
    Bytes edit,keys;
    for(const auto& p:m.parts)Add(edit,Material(p));
    std::vector<std::array<float,3>> vertices;std::vector<std::array<float,2>> uv;
    vertices.reserve(m.vertices.size());uv.reserve(m.vertices.size());
    // Swapping Y/Z mirrors handedness; reverse face winding as we undo the
    // editor basis conversion, including for parts exported from a 3DS source.
    for(const auto& v:m.vertices){vertices.push_back({v.x,v.z,v.y});uv.push_back({v.u,1.f-v.v});}
    std::vector<std::array<std::uint16_t,3>> faces;faces.reserve(m.indices.size()/3);
    for(size_t i=0;i<m.indices.size();i+=3)faces.push_back({
        static_cast<std::uint16_t>(m.indices[i]),static_cast<std::uint16_t>(m.indices[i+2]),
        static_cast<std::uint16_t>(m.indices[i+1])});
    Add(edit,Object(m.visualName,vertices,faces,uv,m.parts));Add(keys,KeyNode(m.visualName,0));
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
    Bytes header;U16(header,5);CStr(header,"ANIM");U32(header,100);Bytes kf;
    Add(kf,Chunk(0xB00A,header));Add(kf,keys);
    Bytes root;Add(root,Chunk(0x3D3D,edit));Add(root,Chunk(0xB000,kf));
    result=Chunk(0x4D4D,root);error.clear();return true;
}
} // namespace Native3DSWriter
