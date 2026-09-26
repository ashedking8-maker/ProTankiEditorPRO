#pragma once
// CPU-only import shared by the renderer and regression checks.
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

inline Model Load(const std::filesystem::path& file) {
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

    const std::string stem = Lower(file.stem().string());
    auto anchor = std::find_if(nodes.begin(), nodes.end(), [&](const Node& n){return Lower(n.node->mName.C_Str()) == stem;});
    Model result;

    // Legacy prop 3DS files commonly contain one visible mesh plus helper nodes
    // named Box/Plane/Tri/Occl. Those helpers describe collision/occlusion and
    // must NOT be rendered. When the filename and visible-node name differ,
    // prefer the node that actually carries a non-default material/texture.
    if (anchor == nodes.end()) {
        long long bestScore = std::numeric_limits<long long>::min();
        auto best = nodes.begin();
        for (auto it = nodes.begin(); it != nodes.end(); ++it) {
            long long score = 0;
            size_t faceCount = 0;
            bool textured = false;
            bool nonDefaultMaterial = false;
            for (unsigned j=0; j<it->node->mNumMeshes; ++j) {
                const auto* mesh = scene->mMeshes[it->node->mMeshes[j]];
                faceCount += mesh->mNumFaces;
                if (mesh->mMaterialIndex < scene->mNumMaterials) {
                    const auto* material = scene->mMaterials[mesh->mMaterialIndex];
                    aiString tex;
                    if (material->GetTexture(aiTextureType_DIFFUSE,0,&tex) == AI_SUCCESS && tex.length) textured = true;
                    aiString matName;
                    if (material->Get(AI_MATKEY_NAME, matName) == AI_SUCCESS) {
                        const auto n = Lower(matName.C_Str());
                        if (!n.empty() && n != "default") nonDefaultMaterial = true;
                    }
                }
            }
            score += static_cast<long long>(faceCount);
            if (textured) score += 1'000'000;
            if (nonDefaultMaterial) score += 100'000;
            if (HelperLikeName(it->node->mName.C_Str())) score -= 10'000;
            if (score > bestScore) { bestScore = score; best = it; }
        }
        anchor = best;
        result.warnings.push_back("No node matching filename; selected visual mesh node by material/face score");
    }

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
    const bool mirrored = transform.Determinant() < 0;

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
            const auto p = transform * mesh->mVertices[i];
            auto n = normals * (mesh->HasNormals() ? mesh->mNormals[i] : aiVector3D(0,0,1));
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
