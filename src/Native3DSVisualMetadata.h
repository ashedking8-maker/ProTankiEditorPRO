#pragma once
// Strict, bounded reader for original 3DS visual face smoothing and complete
// material chunks. This metadata is separate from Assimp's generated normals.
// Never infer face order: associate faces by ORIGINAL geometry instead.
#include "Native3DSWriter.h"
#include "Native3DSScene.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <vector>

namespace Native3DSVisualMetadata {
using Bytes=Native3DSWriter::Bytes;
struct Material {
    std::string name,texture;
    Bytes raw;
    std::array<float,3> ambient{.5f,.5f,.5f},diffuse{1.f,1.f,1.f},specular{0.f,0.f,0.f};
    float shininess{},transparency{};
    std::uint16_t shading{3};bool twoSided{};
};
struct Face {std::array<std::uint16_t,3> indices{};std::uint32_t smoothing{};std::string material;};
struct Visual {
    std::string name;
    std::vector<std::array<float,3>> vertices;
    std::vector<std::array<float,3>> localVertices; // Same normalized space as LegacyMeshImport.
    std::vector<Face> faces;
    std::array<float,12> matrix{};
    bool hasMatrix{},hasSmoothing{};
    std::vector<Material> materials;
};
inline std::uint16_t U16(const Bytes& b,size_t at){return std::uint16_t(b[at])|(std::uint16_t(b[at+1])<<8);}
inline std::uint32_t U32(const Bytes& b,size_t at){return std::uint32_t(U16(b,at))|(std::uint32_t(U16(b,at+2))<<16);}
inline float F32(const Bytes& b,size_t at){auto bits=U32(b,at);float result{};std::memcpy(&result,&bits,4);return result;}
inline bool Chunks(const Bytes& b,size_t start,size_t end,const auto& fn){
    while(start<end){
        if(end-start<6)return false;
        const auto size=U32(b,start+2);if(size<6||size>end-start)return false;
        if(!fn(U16(b,start),start+6,start+size,start))return false;
        start+=size;
    }
    return true;
}
inline bool CString(const Bytes& b,size_t a,size_t end,std::string& value,size_t& next){
    if(a>=end)return false;
    auto z=std::find(b.begin()+static_cast<std::ptrdiff_t>(a),b.begin()+static_cast<std::ptrdiff_t>(end),0);
    if(z==b.begin()+static_cast<std::ptrdiff_t>(end))return false;
    next=static_cast<size_t>(z-b.begin())+1;
    value.assign(reinterpret_cast<const char*>(b.data()+a),next-a-1);
    return value.size()<=120;
}
inline bool Color(const Bytes& b,size_t start,size_t end,std::array<float,3>& rgb){
    return Chunks(b,start,end,[&](auto id,size_t a,size_t z,size_t){
        if((id==0x0011||id==0x0012)&&z-a>=3){for(int i=0;i<3;++i)rgb[i]=b[a+i]/255.f;}
        if((id==0x0010||id==0x0013)&&z-a>=12){for(int i=0;i<3;++i)rgb[i]=F32(b,a+i*4);}
        return true;
    });
}
inline bool Percent(const Bytes& b,size_t start,size_t end,float& percentage){
    return Chunks(b,start,end,[&](auto id,size_t a,size_t z,size_t){
        if(id==0x0030 && z-a>=2)percentage=U16(b,a)/100.f;
        if(id==0x0031 && z-a>=4)percentage=F32(b,a);
        return true;
    });
}
inline bool ParseMaterial(const Bytes& b,size_t start,size_t end,size_t rawStart,Material& m){
    m.raw=Bytes(b.begin()+static_cast<std::ptrdiff_t>(rawStart),b.begin()+static_cast<std::ptrdiff_t>(end));
    return Chunks(b,start,end,[&](auto id,size_t a,size_t z,size_t){
        size_t next{};
        switch(id){
        case 0xA000:return CString(b,a,z,m.name,next);
        case 0xA010:return Color(b,a,z,m.ambient);
        case 0xA020:return Color(b,a,z,m.diffuse);
        case 0xA030:return Color(b,a,z,m.specular);
        case 0xA040:return Percent(b,a,z,m.shininess);
        case 0xA050:return Percent(b,a,z,m.transparency);
        case 0xA081:m.twoSided=true;return true;
        case 0xA100:if(z-a>=2)m.shading=U16(b,a);return true;
        case 0xA200:return Chunks(b,a,z,[&](auto child,size_t p,size_t end,size_t){
            return child!=0xA300||CString(b,p,end,m.texture,next);
        });
        default:return true;
        }
    }) && !m.name.empty();
}
inline bool Read(const std::filesystem::path& path,const std::string& anchor,Visual& result,std::string& error){
    result={};std::error_code ec;const auto size=std::filesystem::file_size(path,ec);
    if(ec||size<6||size>64ull*1024ull*1024ull){error="Invalid/oversized native visual 3DS source.";return false;}
    std::ifstream file(path,std::ios::binary);if(!file){error="Could not open native visual source.";return false;}
    Bytes bytes((std::istreambuf_iterator<char>(file)),{});
    if(bytes.size()!=size||U16(bytes,0)!=0x4d4d||U32(bytes,2)!=size){error="Malformed native 3DS container.";return false;}
    bool found=false;
    const auto faces=[&](size_t a,size_t z)->bool{
        if(z-a<2)return false;
        const size_t count=U16(bytes,a);a+=2;
        if(count>(z-a)/8)return false;
        result.faces.resize(count);
        for(size_t i=0;i<count;++i){auto& f=result.faces[i];
            for(size_t k=0;k<3;++k)f.indices[k]=U16(bytes,a+8*i+2*k);
        }
        a+=count*8;
        return Chunks(bytes,a,z,[&](auto id,size_t p,size_t e,size_t){
            if(id==0x4150){
                if(result.hasSmoothing||e-p!=count*4)return false;
                result.hasSmoothing=true;
                for(size_t i=0;i<count;++i)result.faces[i].smoothing=U32(bytes,p+i*4);
            }else if(id==0x4130){
                std::string material;size_t next{};
                if(!CString(bytes,p,e,material,next)||e-next<2)return false;
                const size_t n=U16(bytes,next);next+=2;
                if(n>(e-next)/2)return false;
                for(size_t i=0;i<n;++i){const auto index=U16(bytes,next+2*i);
                    if(index>=count || (!result.faces[index].material.empty() && result.faces[index].material!=material))return false;
                    result.faces[index].material=material;
                }
            }
            return true;
        });
    };
    const auto mesh=[&](size_t a,size_t z)->bool{
        return Chunks(bytes,a,z,[&](auto id,size_t p,size_t e,size_t){
            if(id==0x4110){
                if(e-p<2)return false;
                const size_t n=U16(bytes,p);p+=2;if(n>(e-p)/12 || !result.vertices.empty())return false;
                result.vertices.reserve(n);
                for(size_t i=0;i<n;++i)result.vertices.push_back({F32(bytes,p+12*i),F32(bytes,p+12*i+4),F32(bytes,p+12*i+8)});
            }else if(id==0x4120){if(!result.faces.empty()||!faces(p,e))return false;}
            else if(id==0x4160){
                if(e-p<48||result.hasMatrix)return false;
                result.hasMatrix=true;
                for(size_t i=0;i<12;++i)result.matrix[i]=F32(bytes,p+4*i);
            }
            return true;
        });
    };
    const auto edit=[&](size_t a,size_t z)->bool{
        return Chunks(bytes,a,z,[&](auto id,size_t p,size_t e,size_t at){
            if(id==0xAFFF){Material m;if(!ParseMaterial(bytes,p,e,at,m))return false;result.materials.push_back(std::move(m));}
            if(id==0x4000){
                std::string name;size_t after{};
                if(!CString(bytes,p,e,name,after))return false;
                if(name==anchor){if(found)return false;found=true;result.name=name;
                    if(!Chunks(bytes,after,e,[&](auto child,size_t s,size_t end,size_t){
                        return child!=0x4100||mesh(s,end);
                    }))return false;
                }
            }
            return true;
        });
    };
    bool ok=Chunks(bytes,6,bytes.size(),[&](auto id,size_t a,size_t z,size_t){
        if(id!=0x3d3d)return true;
        return edit(a,z);
    });
    if(!ok||!found||!result.hasMatrix||result.vertices.empty()||result.faces.empty()){
        error="Cannot read original visual smoothing/material chunks.";return false;
    }
    for(const auto& face:result.faces){
        for(const auto index:face.indices)if(index>=result.vertices.size()){
            error="Original visual face has invalid indices.";return false;
        }
        if(face.material.empty()){error="Original visual face lacks its material association.";return false;}
        if(std::none_of(result.materials.begin(),result.materials.end(),[&](const Material& m){return m.name==face.material;})){
            error="Source face references an unknown material.";return false;
        }
    }
    Native3DSScene::Scene scene;Native3DSScene::Selection selection;
    if(!Native3DSScene::Read(path,scene,error)||!Native3DSScene::Resolve(scene,anchor,selection,error))return false;
    const auto& node=scene.nodes[selection.node];
    const auto* frame=Native3DSScene::SelectedFrame(scene,selection);
    for(auto raw:node.vertices){
        Native3DSScene::V local{};
        if(!Native3DSScene::LocalVertex(node,frame,raw,local)){error="Invalid original visual transform.";return false;}
        if(frame)local=Native3DSScene::Rotate({local.x*frame->scale.x,local.y*frame->scale.y,local.z*frame->scale.z},frame->rotation);
        if(!Native3DSScene::Finite(local)){error="Invalid transformed visual vertex.";return false;}
        result.localVertices.push_back({local.x,local.y,local.z});
    }
    error.clear();return true;
}
// Quantized original LOCAL geometry is matched to Assimp's imported vertices.
// Sorting face corners avoids relying on Assimp's face/material order.
using Point=std::array<long long,3>;
using Key=std::array<Point,3>;
inline Key FaceKey(const std::array<std::array<float,3>,3>& corners){
    Key key{};for(size_t i=0;i<3;++i)for(size_t c=0;c<3;++c)
        key[i][c]=std::llround(static_cast<double>(corners[i][c])*100.);
    std::sort(key.begin(),key.end());return key;
}
inline bool Match(const Visual& raw,const Native3DSWriter::Model& imported,
                  std::vector<std::uint32_t>& smoothing,std::vector<std::string>& material,std::string& error){
    smoothing.clear();material.clear();
    if(imported.indices.size()%3||raw.faces.size()!=imported.indices.size()/3){
        error="Original visual triangle topology changed; material/smoothing correspondence is unsafe.";return false;
    }
    if(raw.localVertices.size()!=raw.vertices.size()){error="Missing normalized visual coordinates.";return false;}
    // Match in the same inverse-matrix/pivot/rotation/scale space as the viewport.
    std::map<Key,std::vector<size_t>> keys;
    for(size_t i=0;i<raw.faces.size();++i){
        const auto& f=raw.faces[i];std::array<std::array<float,3>,3> p{};
        for(size_t k=0;k<3;++k)for(size_t c=0;c<3;++c)
            p[k][c]=raw.localVertices[f.indices[k]][c];
        const auto key=FaceKey(p);
        const auto& existing=keys[key];
        for(const auto duplicate:existing)if(raw.faces[duplicate].smoothing!=f.smoothing ||
             raw.faces[duplicate].material!=f.material){
            error="Duplicate source geometry has conflicting material/smoothing; cannot map faces safely.";return false;
        }
        keys[key].push_back(i);
    }
    for(size_t i=0;i<imported.indices.size();i+=3){
        std::array<std::array<float,3>,3> p{};
        for(size_t k=0;k<3;++k){
            const auto j=imported.indices[i+k];if(j>=imported.vertices.size()){
                error="Invalid Assimp face index.";return false;
            }
            const auto& v=imported.vertices[j];p[k]={v.x,v.z,v.y};
        }
        const auto it=keys.find(FaceKey(p));
        if(it==keys.end()||it->second.empty()){
            error="Cannot safely match original 3DS smoothing to imported face geometry.";return false;
        }
        const auto index=it->second.back();it->second.pop_back();
        smoothing.push_back(raw.faces[index].smoothing);
        material.push_back(raw.faces[index].material);
    }
    error.clear();return true;
}
} // namespace Native3DSVisualMetadata
