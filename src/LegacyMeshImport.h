#pragma once
// CPU-only import shared by the renderer and regression checks.
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <algorithm>
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
    // Convert Z-up legacy local coordinates to our render basis (x,z,-y) once.
    const aiMatrix4x4 basis(1,0,0,0, 0,0,1,0, 0,-1,0,0, 0,0,0,1);
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
        if (part.indexCount) result.parts.push_back(part);
    }

    if (result.indices.empty()) throw std::runtime_error("No drawable visual mesh triangles");
    return result;
}
}
