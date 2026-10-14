#pragma once
// GLB CPU import for authoring previews and explicit legacy conversion.
// Geometry is not a collision template; the exporter requires authored boxes.
#include "LegacyMeshImport.h"
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/GltfMaterial.h>
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
        aiProcess_SortByPType|aiProcess_FlipUVs,"glb");
    if (!scene || !scene->mRootNode || !scene->HasMeshes())
        throw std::runtime_error(std::string("GLB geometry import: ")+importer.GetErrorString());
    LegacyMeshImport::Model model;
    model.anchor="GLB scene (all mesh nodes)";
    if(scene->HasAnimations())model.nativeExportProblems.push_back("Animated GLB requires a baked static model for legacy export.");
    std::map<const aiTexture*,std::shared_ptr<const std::vector<std::uint8_t>>> images;
    // glTF is Y-up in meters; the draft viewport works in legacy-size units.
    // 500 units/meter is a DISPLAY convention only, not a game export scale.
    constexpr float previewUnits=500.f;
    std::function<void(const aiNode*,const aiMatrix4x4&)> walk=[&](const aiNode* node,const aiMatrix4x4& parent) {
        const aiMatrix4x4 world=parent*node->mTransformation;
        aiMatrix3x3 normalMatrix(world);
        const bool mirrored=normalMatrix.Determinant()<0;
        if (std::abs(normalMatrix.Determinant())>1e-10f) normalMatrix.Inverse().Transpose();
        for (unsigned m=0;m<node->mNumMeshes;++m) {
            const aiMesh* mesh=scene->mMeshes[node->mMeshes[m]];
            if (!mesh || !mesh->HasPositions() || mesh->mPrimitiveTypes==aiPrimitiveType_LINE || mesh->mPrimitiveTypes==aiPrimitiveType_POINT) continue;
            if (model.vertices.size()+mesh->mNumVertices>2'000'000 || model.indices.size()+size_t(mesh->mNumFaces)*3>6'000'000)
                throw std::runtime_error("GLB preview exceeds geometry safety limits");
            const auto base=static_cast<std::uint32_t>(model.vertices.size());
            LegacyMeshImport::Part part;
            if(mesh->HasBones()||mesh->mNumAnimMeshes || mesh->HasVertexColors(0))
                model.nativeExportProblems.push_back("Bake skinning, morphs and vertex colors into a static textured mesh before legacy export.");
            part.firstIndex=static_cast<std::uint32_t>(model.indices.size());
            unsigned uvChannel=0;
            if (mesh->mMaterialIndex<scene->mNumMaterials) {
                const auto* material=scene->mMaterials[mesh->mMaterialIndex];
                int twoSided=0;material->Get(AI_MATKEY_TWOSIDED,twoSided);part.doubleSided=twoSided!=0;
                if(material->Get(AI_MATKEY_BASE_COLOR,part.color)!=AI_SUCCESS)
                    material->Get(AI_MATKEY_COLOR_DIFFUSE,part.color);
                for(const auto c:{part.color.r,part.color.g,part.color.b,part.color.a})
                    if(!std::isfinite(c)||c<0||c>1)throw std::runtime_error("GLB material has invalid base color");
                aiString alphaMode;
                if(material->Get(AI_MATKEY_GLTF_ALPHAMODE,alphaMode)==AI_SUCCESS)
                    part.opaque=std::string(alphaMode.C_Str())=="OPAQUE";
                aiString texture;
                auto type=aiTextureType_BASE_COLOR;
                if(material->GetTexture(type,0,&texture,nullptr,&uvChannel)!=AI_SUCCESS) {
                    type=aiTextureType_DIFFUSE;
                    material->GetTexture(type,0,&texture,nullptr,&uvChannel);
                }
                if(texture.length) {
                    int wrapU=aiTextureMapMode_Wrap,wrapV=aiTextureMapMode_Wrap;
                    material->Get(AI_MATKEY_MAPPINGMODE_U(type,0),wrapU);
                    material->Get(AI_MATKEY_MAPPINGMODE_V(type,0),wrapV);
                    if(wrapU!=aiTextureMapMode_Wrap || wrapV!=aiTextureMapMode_Wrap)
                        throw std::runtime_error("GLB texture uses clamp/mirror sampling; bake a repeating atlas for this legacy viewport");
                    if(!mesh->HasTextureCoords(uvChannel))throw std::runtime_error("GLB base color references missing UV coordinates");
                    aiUVTransform transform;
                    if(material->Get(AI_MATKEY_UVTRANSFORM(type,0),transform)==AI_SUCCESS &&
                       (transform.mTranslation.x!=0 || transform.mTranslation.y!=0 ||
                        transform.mScaling.x!=1 || transform.mScaling.y!=1 || transform.mRotation!=0))
                        throw std::runtime_error("GLB texture transform is unsupported; bake the UV transform first");
                    const auto* embedded=scene->GetEmbeddedTexture(texture.C_Str());
                    if(!embedded || embedded->mHeight!=0 || !embedded->mWidth || embedded->mWidth>64u*1024u*1024u)
                        throw std::runtime_error("GLB requires an embedded PNG/JPEG base-color image (up to 64 MiB)");
                    auto& bytes=images[embedded];
                    if(!bytes) {
                        const auto* begin=reinterpret_cast<const std::uint8_t*>(embedded->pcData);
                        bytes=std::make_shared<const std::vector<std::uint8_t>>(begin,begin+embedded->mWidth);
                    }
                    const auto& b=*bytes;
                    const bool png=b.size()>=8 && b[0]==137 && b[1]==80 && b[2]==78 && b[3]==71;
                    const bool jpeg=b.size()>=3 && b[0]==255 && b[1]==216 && b[2]==255;
                    if(!png&&!jpeg)throw std::runtime_error("GLB base-color image must be PNG or JPEG");
                    part.embeddedDiffuse=bytes;
                }
            }
            for (unsigned i=0;i<mesh->mNumVertices;++i) {
                const auto p=world*mesh->mVertices[i];
                auto n=normalMatrix*(mesh->HasNormals()?mesh->mNormals[i]:aiVector3D(0,1,0));
                n.NormalizeSafe();
                const auto uv=mesh->HasTextureCoords(uvChannel)?mesh->mTextureCoords[uvChannel][i]:aiVector3D();
                if (!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z))
                    throw std::runtime_error("GLB contains non-finite geometry");
                model.vertices.push_back({{p.x*previewUnits,p.y*previewUnits,p.z*previewUnits},n,{uv.x,uv.y}});
            }
            for (unsigned i=0;i<mesh->mNumFaces;++i) {
                const aiFace& f=mesh->mFaces[i];
                if(f.mNumIndices!=3)continue;
                for(unsigned j=0;j<3;++j)model.indices.push_back(base+f.mIndices[mirrored&&j?3-j:j]);
            }
            part.indexCount=static_cast<std::uint32_t>(model.indices.size())-part.firstIndex;
            if(part.indexCount)model.parts.push_back(part);
        }
        for (unsigned i=0;i<node->mNumChildren;++i)walk(node->mChildren[i],world);
    };
    walk(scene->mRootNode,aiMatrix4x4());
    if(model.indices.empty())throw std::runtime_error("GLB contains no drawable triangles");
    model.warnings.push_back("GLB preview uses base color; normal maps, metallic/roughness and animation are not rendered by the legacy viewport.");
    return model;
}
} // namespace DraftMeshImport
