#pragma once

// Editor-only diagnostic preview of the ACTUAL <collision-geometry> XML.
// Does not infer prop ownership, change physics, or write to the map.
// It deliberately renders collision rather than pretending painted props prove passability.
#include "MapDocument.h"
#include "LegacyTransform.h"
#include <DirectXMath.h>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

namespace CollisionPreview {

struct Vertex {
    DirectX::XMFLOAT3 position{};
    DirectX::XMFLOAT4 color{};
};
struct Mesh {
    std::vector<Vertex> faces;     // Triangle list; two-sided rasterization in SceneRenderer.
    std::vector<Vertex> edges;     // Line list; outlines from actual primitives.
    size_t planes{};
    size_t boxes{};
    size_t triangles{};
    size_t omitted{};
};

inline bool Finite(const DirectX::XMFLOAT3& p) {
    return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
}
inline DirectX::XMFLOAT3 Transform(const DirectX::XMFLOAT3& p, const DirectX::XMMATRIX& world) {
    DirectX::XMFLOAT3 result{};
    DirectX::XMStoreFloat3(&result, DirectX::XMVector3TransformCoord(DirectX::XMLoadFloat3(&p),world));
    return result;
}
inline DirectX::XMFLOAT4 SurfaceColor(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b,
                                     const DirectX::XMFLOAT3& c) {
    using namespace DirectX;
    const XMVECTOR u = XMLoadFloat3(&b) - XMLoadFloat3(&a);
    const XMVECTOR v = XMLoadFloat3(&c) - XMLoadFloat3(&a);
    const XMVECTOR n = XMVector3Cross(u,v);
    const float len2 = XMVectorGetX(XMVector3LengthSq(n));
    // Green: approximately horizontal collider; red: steep/vertical collider.
    // This classification is based on face orientation, NOT on gameplay flags.
    if (len2 > 0.000001f && std::fabs(XMVectorGetY(n)) / std::sqrt(len2) >= 0.70f)
        return {0.12f, 0.82f, 0.18f, 1.0f};
    return {0.95f, 0.10f, 0.20f, 1.0f};
}

inline Mesh Build(const MapDocument& map) {
    using namespace DirectX;
    Mesh result;
    constexpr size_t maxFaceVertices = 2'000'000;
    constexpr size_t maxEdgeVertices = 2'000'000;
    const XMFLOAT4 border{0.92f,0.96f,1.0f,0.75f};
    auto segment=[&](const XMFLOAT3& a,const XMFLOAT3& b) {
        if(result.edges.size()+2 <= maxEdgeVertices) {
            result.edges.push_back({a,border});result.edges.push_back({b,border});
        }
    };
    auto face=[&](const XMFLOAT3& a,const XMFLOAT3& b,const XMFLOAT3& c,
                  const XMFLOAT4& color) {
        result.faces.push_back({a,color});result.faces.push_back({b,color});result.faces.push_back({c,color});
    };
    auto valid=[&](const std::array<XMFLOAT3,8>& points,size_t count) {
        for(size_t i=0;i<count;++i) if(!Finite(points[i])) return false;
        return true;
    };
    auto quad=[&](const XMFLOAT3& a,const XMFLOAT3& b,const XMFLOAT3& c,const XMFLOAT3& d,
                  const XMFLOAT4& color) {
        face(a,b,c,color);face(a,c,d,color);
    };
    for(const auto& plane:map.CollisionPlanes()) {
        if(!Finite(plane.position) || !Finite(plane.rotation) ||
           !std::isfinite(plane.width) || !std::isfinite(plane.length) ||
           plane.width<=0 || plane.length<=0 || result.faces.size()+6>maxFaceVertices ||
           result.edges.size()+8>maxEdgeVertices) {++result.omitted;continue;}
        const float w=plane.width*0.5f, h=plane.length*0.5f;
        const auto world=LegacyTransform::WorldFromLegacyLocal(plane.position,plane.rotation);
        const std::array<XMFLOAT3,8> p{
            Transform({-w,-h,0},world),Transform({w,-h,0},world),
            Transform({w,h,0},world),Transform({-w,h,0},world)
        };
        if(!valid(p,4)) {++result.omitted;continue;}
        const auto color=SurfaceColor(p[0],p[1],p[2]);
        quad(p[0],p[1],p[2],p[3],color);
        for(int i=0;i<4;++i)segment(p[i],p[(i+1)%4]);
        ++result.planes;
    }
    for(const auto& box:map.CollisionBoxes()) {
        if(!Finite(box.position) || !Finite(box.rotation) || !Finite(box.size) ||
           box.size.x<=0 || box.size.y<=0 || box.size.z<=0 ||
           result.faces.size()+36>maxFaceVertices || result.edges.size()+24>maxEdgeVertices) {
            ++result.omitted;continue;
        }
        const XMFLOAT3 h{box.size.x*0.5f,box.size.y*0.5f,box.size.z*0.5f};
        const auto world=LegacyTransform::WorldFromLegacyLocal(box.position,box.rotation);
        const std::array<XMFLOAT3,8> p{
            Transform({-h.x,-h.y,-h.z},world),Transform({h.x,-h.y,-h.z},world),
            Transform({h.x,h.y,-h.z},world),Transform({-h.x,h.y,-h.z},world),
            Transform({-h.x,-h.y,h.z},world),Transform({h.x,-h.y,h.z},world),
            Transform({h.x,h.y,h.z},world),Transform({-h.x,h.y,h.z},world)
        };
        if(!valid(p,8)) {++result.omitted;continue;}
        const XMFLOAT4 red{0.92f,0.10f,0.18f,1.0f};
        constexpr int sides[6][4]={{0,1,2,3},{4,5,6,7},{0,4,7,3},
                                    {1,5,6,2},{3,2,6,7},{0,1,5,4}};
        for(const auto& s:sides)quad(p[s[0]],p[s[1]],p[s[2]],p[s[3]],red);
        constexpr int links[12][2]={{0,1},{1,2},{2,3},{3,0},{4,5},{5,6},
                                      {6,7},{7,4},{0,4},{1,5},{2,6},{3,7}};
        for(const auto& e:links)segment(p[e[0]],p[e[1]]);
        ++result.boxes;
    }
    for(const auto& triangle:map.CollisionTriangles()) {
        if(!Finite(triangle.position)||!Finite(triangle.rotation)||!Finite(triangle.v0)||
           !Finite(triangle.v1)||!Finite(triangle.v2)||result.faces.size()+3>maxFaceVertices||
           result.edges.size()+6>maxEdgeVertices) {++result.omitted;continue;}
        const auto world=LegacyTransform::WorldFromLegacyLocal(triangle.position,triangle.rotation);
        const auto a=Transform(triangle.v0,world), b=Transform(triangle.v1,world),
                   c=Transform(triangle.v2,world);
        if(!Finite(a)||!Finite(b)||!Finite(c)) {++result.omitted;continue;}
        face(a,b,c,SurfaceColor(a,b,c));segment(a,b);segment(b,c);segment(c,a);
        ++result.triangles;
    }
    return result;
}

} // namespace CollisionPreview
