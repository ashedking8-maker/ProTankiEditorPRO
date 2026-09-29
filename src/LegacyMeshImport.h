#pragma once
// CPU-only import shared by the renderer and regression checks.
#include "Native3DSScene.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <algorithm>
#include <array>
#include <map>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <fstream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace LegacyMeshImport {
struct Vertex { aiVector3D position, normal; aiVector2D uv; };
struct Part {
    uint32_t firstIndex{}, indexCount{};
    std::filesystem::path diffuse;
    aiColor4D color{1,1,1,1};
    bool oneSidedPairedAtlas{}; // opposite, exactly coincident faces use distinct UV halves
};
struct Model {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<Part> parts;
    std::string anchor;
    size_t ignoredMeshNodes{}; // collision/occlusion/helper nodes not rendered
    std::vector<std::string> warnings;
};
inline std::string Lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){return static_cast<char>(std::tolower(c));});
    return s;
}
inline bool StartsWith(const std::string& value, const char* prefix) {
    const std::string p = prefix;
    return value.size() >= p.size() && std::equal(p.begin(), p.end(), value.begin());
}
inline bool HelperLikeName(const std::string& raw) {
    const std::string n = Lower(raw);
    return StartsWith(n,"box") || StartsWith(n,"plane") || StartsWith(n,"tri") || StartsWith(n,"occl");
}
inline std::filesystem::path TexturePath(const std::filesystem::path& dir, std::string name) {
    std::replace(name.begin(), name.end(), '\\', '/');
    // Old 3DS files can retain an absolute path from the artist's machine.
    const auto candidate = dir / std::filesystem::path(name).filename();
    if (std::filesystem::exists(candidate)) return candidate;
    const auto wanted = Lower(candidate.filename().string());
    for (const auto& e : std::filesystem::directory_iterator(dir))
        if (e.is_regular_file() && Lower(e.path().filename().string()) == wanted) return e.path();
    return candidate; // Let texture loading report the missing file.
}
// Some native 3DS meshes (e.g. Industrial Bridge / brid_1) contain two
// opposite-facing, co-planar triangle sets mapped to DIFFERENT halves of one
// atlas. If both sets are drawn with CULL_NONE and LESS_EQUAL, the later dark
// underside can overwrite the earlier bright upper surface at equal depth.
// Only detect *fully paired* parts; keep unpaired legacy geometry two-sided.
inline bool HasOppositeFaceAtlas(const std::vector<Vertex>& vertices,
                                 const std::vector<uint32_t>& indices,
                                 uint32_t firstIndex, uint32_t indexCount) {
    if (indexCount < 12 || indexCount % 6 != 0 ||
        static_cast<size_t>(firstIndex) + indexCount > indices.size()) return false;
    struct Face { aiVector3D normal; float averageU{}; };
    using FaceKey=std::array<long long,9>;
    std::map<FaceKey,std::vector<Face>> groups;
    const auto quantize=[](float v)->long long {return std::llround(static_cast<double>(v)*1000.0);};
    for(uint32_t i=firstIndex;i<firstIndex+indexCount;i+=3) {
        const auto a=indices[i],b=indices[i+1],c=indices[i+2];
        if(a>=vertices.size()||b>=vertices.size()||c>=vertices.size())return false;
        const auto& va=vertices[a],&vb=vertices[b],&vc=vertices[c];
        std::array<std::array<long long,3>,3> corners{};
        const Vertex* trio[3]={&va,&vb,&vc};
        for(int j=0;j<3;++j)corners[j]={quantize(trio[j]->position.x),
            quantize(trio[j]->position.y),quantize(trio[j]->position.z)};
        std::sort(corners.begin(),corners.end());
        FaceKey key{};
        for(int j=0;j<3;++j)for(int k=0;k<3;++k)key[3*j+k]=corners[j][k];
        const auto u=vb.position-va.position,v=vc.position-va.position;
        aiVector3D normal(u.y*v.z-u.z*v.y,u.z*v.x-u.x*v.z,u.x*v.y-u.y*v.x);
        if(normal.SquareLength()<1e-10f)return false;
        normal.Normalize();
        groups[key].push_back({normal,(va.uv.x+vb.uv.x+vc.uv.x)/3.f});
    }
    if(groups.size()*6 != indexCount)return false; // every triangle exactly paired
    for(const auto& [key,faces]:groups) {
        (void)key;
        if(faces.size()!=2)return false;
        const auto& a=faces[0],&b=faces[1];
        const float dot=a.normal.x*b.normal.x+a.normal.y*b.normal.y+a.normal.z*b.normal.z;
        if(dot>-.99f || std::abs(a.averageU-b.averageU)<.25f)return false;
    }
    return true;
}

inline Model Load(const std::filesystem::path& file,const std::string& objectName={}) {
    Native3DSScene::Scene native;Native3DSScene::Selection selected;std::string nativeError;
    if(!Native3DSScene::Read(file,native,nativeError)||!Native3DSScene::Resolve(native,objectName,selected,nativeError))
        throw std::runtime_error(nativeError);
    // Read via filesystem::path so Windows Unicode paths do not depend on ACP.
    std::ifstream stream(file, std::ios::binary);
    if (!stream) throw std::runtime_error("Cannot open mesh");
    const std::vector<char> bytes((std::istreambuf_iterator<char>(stream)), {});
    Assimp::Importer importer;
    const auto* scene = importer.ReadFileFromMemory(bytes.data(), bytes.size(),
        aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_GenSmoothNormals |
        aiProcess_ImproveCacheLocality | aiProcess_SortByPType | aiProcess_FlipUVs, "3ds");
    if (!scene || !scene->mRootNode || !scene->HasMeshes())
        throw std::runtime_error(importer.GetErrorString());

    struct Node { const aiNode* node; aiMatrix4x4 world; };
    std::vector<Node> nodes;
    std::function<void(const aiNode*, const aiMatrix4x4&)> visit = [&](const aiNode* n, const aiMatrix4x4& parent) {
        const auto world = parent * n->mTransformation;
        if (n->mNumMeshes) nodes.push_back({n, world});
        for (unsigned i=0; i<n->mNumChildren; ++i) visit(n->mChildren[i], world);
    };
    visit(scene->mRootNode, aiMatrix4x4());
    if (nodes.empty()) throw std::runtime_error("No mesh nodes");

    const auto& source=native.nodes[selected.node];
    auto anchor=std::find_if(nodes.begin(),nodes.end(),[&](const Node& n){return source.name==n.node->mName.C_Str();});
    if(anchor==nodes.end())throw std::runtime_error("Assimp did not preserve the selected native mesh node: "+source.name);
    Model result;
    if(selected.usedRootFallback)result.warnings.push_back("Multiple 3DS roots: using first declared mesh "+source.name+"; mesh/@object overrides this fallback");
    if(native.migratedFlatGenerated)result.warnings.push_back("Migrated older PTPRO flat helper hierarchy");
    const auto* frame=Native3DSScene::SelectedFrame(native,selected);
    // Match Assimp's vertex space to source vertices, then use the ORIGINAL
    // Parser3DS local coordinates. Assimp versions differ in pivot baking.
    // Every vertex must match; no inferred bounds-based recentering is allowed.
    struct NativeVertex {aiVector3D unpivoted,local,placed;};
    std::vector<NativeVertex> sourceVertices;sourceVertices.reserve(source.vertices.size());
    using GridKey=std::array<long long,3>;
    std::array<std::map<GridKey,std::vector<size_t>>,3> grids;
    const auto gridKey=[](aiVector3D v){return GridKey{static_cast<long long>(std::floor(v.x/.05)),
        static_cast<long long>(std::floor(v.y/.05)),static_cast<long long>(std::floor(v.z/.05))};};
    for(auto raw:source.vertices){
        Native3DSScene::V local{};
        if(!Native3DSScene::LocalVertex(source,frame,raw,local))throw std::runtime_error("Invalid native visual transform");
        auto unpivoted=frame?Native3DSScene::Add(local,frame->pivot):local;
        auto placed=local;
        if(frame)placed=Native3DSScene::Rotate({local.x*frame->scale.x,local.y*frame->scale.y,local.z*frame->scale.z},frame->rotation);
        if(!Native3DSScene::Finite(placed))throw std::runtime_error("Non-finite native visual vertex");
        sourceVertices.push_back({{unpivoted.x,unpivoted.y,unpivoted.z},{local.x,local.y,local.z},{placed.x,placed.y,placed.z}});
        const auto i=sourceVertices.size()-1;const auto& v=sourceVertices.back();
        grids[0][gridKey(v.unpivoted)].push_back(i);grids[1][gridKey(v.local)].push_back(i);grids[2][gridKey(v.placed)].push_back(i);
    }
    const auto match=[&](aiVector3D p,int mode)->size_t {
        const auto key=gridKey(p);float best=.011f;size_t found=sourceVertices.size();
        for(int x=-2;x<=2;++x)for(int y=-2;y<=2;++y)for(int z=-2;z<=2;++z){
            auto it=grids[static_cast<size_t>(mode)].find({key[0]+x,key[1]+y,key[2]+z});if(it==grids[static_cast<size_t>(mode)].end())continue;
            for(size_t i:it->second){const auto& v=sourceVertices[i];const auto candidate=mode==0?v.unpivoted:mode==1?v.local:v.placed;
                const auto distance=(p-candidate).SquareLength();if(distance<best){best=distance;found=i;}}
        }
        return found;
    };
    int vertexMode=-1;
    for(int mode=0;mode<3&&vertexMode<0;++mode){bool all=true;
        for(unsigned j=0;j<anchor->node->mNumMeshes && all;++j){const auto* mesh=scene->mMeshes[anchor->node->mMeshes[j]];
            for(unsigned i=0;i<mesh->mNumVertices;++i)if(match(mesh->mVertices[i],mode)==sourceVertices.size()){all=false;break;}}
        if(all)vertexMode=mode;
    }
    if(vertexMode<0)throw std::runtime_error("Assimp/native 3DS vertex-space mismatch; import stopped");
    result.anchor = anchor->node->mName.C_Str();
    result.ignoredMeshNodes = nodes.size() > 0 ? nodes.size() - 1 : 0;
    if (std::abs(anchor->world.Determinant()) < 1e-12f) throw std::runtime_error("Singular mesh anchor transform");

    // Assimp's 3DS importer has already expressed the selected mesh in node-local
    // coordinates. The visible anchor is the prop origin; helper mesh nodes are
    // collision/occlusion metadata and are intentionally omitted from rendering.
    // Convert the original GTanks left-handed Z-up local coordinates to the
    // Direct3D left-handed Y-up viewport basis (x,z,y) once.  The former
    // (x,z,-y) conversion mirrored every imported map and made object yaw
    // appear reversed relative to the original editor / ProTLVK.
    const aiMatrix4x4 basis(1,0,0,0, 0,0,1,0, 0,1,0,0, 0,0,0,1);
    const auto transform = basis;
    aiMatrix3x3 normals(transform);
    if (std::abs(normals.Determinant()) < 1e-12f) throw std::runtime_error("Singular visual transform");
    normals.Inverse().Transpose();
    const bool visualMirrored=frame && frame->scale.x*frame->scale.y*frame->scale.z<0;
    const bool mirrored = (transform.Determinant() < 0) != visualMirrored;

    const auto* node = anchor->node;
    for (unsigned j=0; j<node->mNumMeshes; ++j) {
        const auto* mesh = scene->mMeshes[node->mMeshes[j]];
        const auto base = static_cast<uint32_t>(result.vertices.size());
        Part part; part.firstIndex = static_cast<uint32_t>(result.indices.size());
        if (mesh->mMaterialIndex < scene->mNumMaterials) {
            const auto* material = scene->mMaterials[mesh->mMaterialIndex];
            aiString texture;
            if (material->GetTexture(aiTextureType_DIFFUSE,0,&texture) == AI_SUCCESS)
                part.diffuse = TexturePath(file.parent_path(), texture.C_Str());
            material->Get(AI_MATKEY_COLOR_DIFFUSE, part.color);
        }
        for (unsigned i=0; i<mesh->mNumVertices; ++i) {
            const auto nativeIndex=match(mesh->mVertices[i],vertexMode);
            if(nativeIndex==sourceVertices.size())throw std::runtime_error("Native visual vertex match lost");
            const auto p = transform * sourceVertices[nativeIndex].placed;
            auto sourceNormal=mesh->HasNormals()?mesh->mNormals[i]:aiVector3D(0,0,1);
            if(frame && vertexMode!=2){
                if(std::fabs(frame->scale.x)<1.e-8f||std::fabs(frame->scale.y)<1.e-8f||std::fabs(frame->scale.z)<1.e-8f)
                    throw std::runtime_error("Singular native visual scale");
                auto rotated=Native3DSScene::Rotate({sourceNormal.x/frame->scale.x,sourceNormal.y/frame->scale.y,sourceNormal.z/frame->scale.z},frame->rotation);
                sourceNormal={rotated.x,rotated.y,rotated.z};
            }
            auto n = normals * sourceNormal;
            n.NormalizeSafe();
            const auto uv = mesh->HasTextureCoords(0) ? mesh->mTextureCoords[0][i] : aiVector3D();
            if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z)) throw std::runtime_error("Non-finite mesh vertex");
            result.vertices.push_back({p,n,{uv.x,uv.y}});
        }
        for (unsigned i=0; i<mesh->mNumFaces; ++i) {
            const auto& f = mesh->mFaces[i]; if (f.mNumIndices != 3) continue;
            result.indices.push_back(base+f.mIndices[0]);
            result.indices.push_back(base+f.mIndices[mirrored ? 2 : 1]);
            result.indices.push_back(base+f.mIndices[mirrored ? 1 : 2]);
        }
        part.indexCount = static_cast<uint32_t>(result.indices.size())-part.firstIndex;
        part.oneSidedPairedAtlas=HasOppositeFaceAtlas(result.vertices,result.indices,part.firstIndex,part.indexCount);
        if (part.oneSidedPairedAtlas)
            result.warnings.push_back("Opposite-face UV atlas detected; use selective front-face rendering");
        if (part.indexCount) result.parts.push_back(part);
    }

    if (result.indices.empty()) throw std::runtime_error("No drawable visual mesh triangles");
    return result;
}
}
