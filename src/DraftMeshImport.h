#pragma once
// GLB is used only for authoring previews. Never silently reinterpret this as
// game-compatible 3DS/collision geometry.
#include "LegacyMeshImport.h"
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <stdexcept>
#include <vector>

namespace DraftMeshImport {
inline LegacyMeshImport::Model Load(const std::filesystem::path& file) {
    std::ifstream input(file,std::ios::binary);
    if (!input) throw std::runtime_error("GLB preview file cannot be opened");
    const std::vector<char> bytes((std::istreambuf_iterator<char>(input)),{});
    if (bytes.size()<20 || bytes.size()>512ull*1024*1024) throw std::runtime_error("Invalid GLB preview size");
    Assimp::Importer importer;
    const aiScene* scene=importer.ReadFileFromMemory(bytes.data(),bytes.size(),
        aiProcess_Triangulate|aiProcess_JoinIdenticalVertices|aiProcess_GenSmoothNormals|
        aiProcess_SortByPType,"glb");
    if (!scene || !scene->mRootNode || !scene->HasMeshes())
        throw std::runtime_error(std::string("GLB geometry import: ")+importer.GetErrorString());
    LegacyMeshImport::Model model;
    model.anchor="GLB scene (all mesh nodes)";
    // glTF is Y-up in meters; the draft viewport works in legacy-size units.
    // 500 units/meter is a DISPLAY convention only, not a game export scale.
    constexpr float previewUnits=500.f;
    std::function<void(const aiNode*,const aiMatrix4x4&)> walk=[&](const aiNode* node,const aiMatrix4x4& parent) {
        const aiMatrix4x4 world=parent*node->mTransformation;
        aiMatrix3x3 normalMatrix(world);
        if (std::abs(normalMatrix.Determinant())>1e-10f) normalMatrix.Inverse().Transpose();
        for (unsigned m=0;m<node->mNumMeshes;++m) {
            const aiMesh* mesh=scene->mMeshes[node->mMeshes[m]];
            if (!mesh || !mesh->HasPositions() || mesh->mPrimitiveTypes==aiPrimitiveType_LINE || mesh->mPrimitiveTypes==aiPrimitiveType_POINT) continue;
            if (model.vertices.size()+mesh->mNumVertices>2'000'000 || model.indices.size()+size_t(mesh->mNumFaces)*3>6'000'000)
                throw std::runtime_error("GLB preview exceeds geometry safety limits");
            const auto base=static_cast<std::uint32_t>(model.vertices.size());
            LegacyMeshImport::Part part;
            part.firstIndex=static_cast<std::uint32_t>(model.indices.size());
            if (mesh->mMaterialIndex<scene->mNumMaterials)
                scene->mMaterials[mesh->mMaterialIndex]->Get(AI_MATKEY_COLOR_DIFFUSE,part.color);
            for (unsigned i=0;i<mesh->mNumVertices;++i) {
                const auto p=world*mesh->mVertices[i];
                auto n=normalMatrix*(mesh->HasNormals()?mesh->mNormals[i]:aiVector3D(0,1,0));
                n.NormalizeSafe();
                const auto uv=mesh->HasTextureCoords(0)?mesh->mTextureCoords[0][i]:aiVector3D();
                if (!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z))
                    throw std::runtime_error("GLB contains non-finite geometry");
                model.vertices.push_back({{p.x*previewUnits,p.y*previewUnits,p.z*previewUnits},n,{uv.x,uv.y}});
            }
            for (unsigned i=0;i<mesh->mNumFaces;++i) {
                const aiFace& f=mesh->mFaces[i];
                if(f.mNumIndices!=3)continue;
                for(unsigned j=0;j<3;++j)model.indices.push_back(base+f.mIndices[j]);
            }
            part.indexCount=static_cast<std::uint32_t>(model.indices.size())-part.firstIndex;
            if(part.indexCount)model.parts.push_back(part);
        }
        for (unsigned i=0;i<node->mNumChildren;++i)walk(node->mChildren[i],world);
    };
    walk(scene->mRootNode,aiMatrix4x4());
    if(model.indices.empty())throw std::runtime_error("GLB contains no drawable triangles");
    model.warnings.push_back("GLB preview renders material colors; embedded textures/native 3DS export are not implemented.");
    return model;
}
} // namespace DraftMeshImport
