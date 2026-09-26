#pragma once
#include <DirectXMath.h>

namespace LegacyTransform {

// Mesh import removes the authoring placement relative to a shared anchor,
// then converts the local assembly to Y-up (x,z,-y). Do not swap mesh axes
// again here. XML map positions and rotations remain legacy Z-up values.
inline DirectX::XMMATRIX LegacyToInternalBasis() {
    using namespace DirectX;
    // Row-vector matrix: (x, y, z) -> (x, z, -y)
    return XMMATRIX(
        1, 0,  0, 0,
        0, 0, -1, 0,
        0, 1,  0, 0,
        0, 0,  0, 1);
}

inline DirectX::XMMATRIX InternalToLegacyBasis() {
    // Orthogonal basis change; inverse == transpose.
    return DirectX::XMMatrixTranspose(LegacyToInternalBasis());
}

inline DirectX::XMFLOAT3 Position(const DirectX::XMFLOAT3& legacy) {
    return {legacy.x, legacy.z, -legacy.y};
}

inline DirectX::XMFLOAT3 ToLegacyPosition(const DirectX::XMFLOAT3& internal) {
    return {internal.x, -internal.z, internal.y};
}

// Use this when geometry vertices are authored in the original XML Z-up basis
// (collision planes, boxes and triangles), rather than in the converted mesh basis.
// For row vectors: legacy local -> legacy rotation/translation -> internal Y-up.
inline DirectX::XMMATRIX WorldFromLegacyLocal(const DirectX::XMFLOAT3& position,
                                               const DirectX::XMFLOAT3& rotation) {
    using namespace DirectX;
    return XMMatrixRotationX(rotation.x) *
           XMMatrixRotationY(rotation.y) *
           XMMatrixRotationZ(rotation.z) *
           XMMatrixTranslation(position.x, position.y, position.z) *
           LegacyToInternalBasis();
}

inline DirectX::XMMATRIX World(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& rotation) {
    using namespace DirectX;
    // Legacy editor transform. Static ProTanki props in the supplied corpus only
    // use rotation.z, but keep x/y support centralized for future compatibility.
    const XMMATRIX legacy =
        XMMatrixRotationX(rotation.x) *
        XMMatrixRotationY(rotation.y) *
        XMMatrixRotationZ(rotation.z) *
        XMMatrixTranslation(position.x, position.y, position.z);

    const XMMATRIX toInternal = LegacyToInternalBasis();
    const XMMATRIX toLegacy = InternalToLegacyBasis();
    return toLegacy * legacy * toInternal;
}

} // namespace LegacyTransform
