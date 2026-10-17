#pragma once
// Deliberately independent from rendering, XML and DirectX. A lossless native
// clone often contains both verified per-prop collider ownership and opaque
// primitives copied without interpretation. Never erase/move an opaque node
// merely because its clone batch also contains an edited, VERIFIED prop.
namespace NativeCloneEditPolicy {
inline constexpr bool MayEdit(bool unresolvedOwnership, bool hasExactOwnedCollision,
                              bool batchContainsOpaqueCollision) noexcept {
    return !unresolvedOwnership && (!batchContainsOpaqueCollision || hasExactOwnedCollision);
}
inline constexpr bool MayEraseOpaque(bool allRemainingClonePropsSelected) noexcept {
    return allRemainingClonePropsSelected;
}
}
