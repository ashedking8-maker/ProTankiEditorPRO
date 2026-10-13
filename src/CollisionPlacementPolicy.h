#pragma once
// Lack of helpers proves neither decoration nor safe tank traversal.
// This policy deliberately does not inspect asset names, height or flatness.
namespace CollisionPlacementPolicy {
inline bool MayPlaceWithoutCollision(bool spriteOnly, bool explicitVisualOnly,
                                     int nativeWithCollision) {
    if (nativeWithCollision == 1) return false;
    return spriteOnly || explicitVisualOnly;
}
} // namespace CollisionPlacementPolicy
