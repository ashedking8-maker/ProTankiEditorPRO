#pragma once
#include "MapDocument.h"
#include <cstddef>
#include <string>
#include <vector>
#include <optional>

struct PropTransformState {
    DirectX::XMFLOAT3 position{};
    DirectX::XMFLOAT3 rotation{};
};

struct TransformEdit {
    size_t propIndex{};
    PropTransformState before{};
    PropTransformState after{};
    std::string label;
};

// A zone coordinate/action change should not duplicate a 230 MB master XML.
// Structural add/delete still use document snapshots for exact legacy round-trips.
struct ZoneEdit {
    size_t index{};
    SpecialBox before{};
    SpecialBox after{};
};

class EditHistory {
public:
    void Clear();
    void Push(TransformEdit edit);
    void PushBatch(std::vector<TransformEdit> edits);
    // Structural edits are reversible as full document snapshots, including legacy source references.
    void PushSnapshot(MapDocument before, const MapDocument& after);
    void PushZone(size_t index, const SpecialBox& before, const SpecialBox& after);
    bool CanUndo() const { return cursor_ > 0; }
    bool CanRedo() const { return cursor_ < edits_.size(); }
    bool Undo(MapDocument& map, std::vector<size_t>& changedIndices);
    bool Redo(MapDocument& map, std::vector<size_t>& changedIndices);

private:
    static bool Different(const PropTransformState& a, const PropTransformState& b);
    struct Entry {
        std::vector<TransformEdit> transforms;
        std::optional<ZoneEdit> zone;
        std::optional<MapDocument> before;
        std::optional<MapDocument> after;
    };
    std::vector<Entry> edits_;
    size_t cursor_{};
};
