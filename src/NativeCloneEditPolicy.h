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
// An explicitly unresolved owner can only be deleted as part of a complete
// clone group when the ambiguous primitives are tagged as belonging to that
// group. Merely being the last item in an owner-only group proves nothing
// about untracked source collision elsewhere in the document.
inline constexpr bool MayEraseUnresolved(bool allRemainingClonePropsSelected,
                                          bool batchContainsOpaqueCollision) noexcept {
    return allRemainingClonePropsSelected && batchContainsOpaqueCollision;
}
}
