#include "EditHistory.h"
#include <cmath>
#include <algorithm>

namespace {
bool diff3(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b) {
    constexpr float e = 0.00001f;
    return std::fabs(a.x-b.x)>e || std::fabs(a.y-b.y)>e || std::fabs(a.z-b.z)>e;
}
}

void EditHistory::Clear() { edits_.clear(); cursor_ = 0; }

bool EditHistory::Different(const PropTransformState& a, const PropTransformState& b) {
    return diff3(a.position,b.position) || diff3(a.rotation,b.rotation);
}

void EditHistory::Push(TransformEdit edit) {
    std::vector<TransformEdit> one; one.push_back(std::move(edit)); PushBatch(std::move(one));
}

void EditHistory::PushBatch(std::vector<TransformEdit> edits) {
    edits.erase(std::remove_if(edits.begin(), edits.end(), [&](const TransformEdit& e){ return !Different(e.before,e.after); }), edits.end());
    if (edits.empty()) return;
    if (cursor_ < edits_.size()) edits_.erase(edits_.begin()+static_cast<std::ptrdiff_t>(cursor_), edits_.end());
    Entry entry; entry.transforms=std::move(edits);
    edits_.push_back(std::move(entry)); cursor_=edits_.size();
}

void EditHistory::PushSnapshot(MapDocument before, const MapDocument& after) {
    if (cursor_ < edits_.size()) edits_.erase(edits_.begin()+static_cast<std::ptrdiff_t>(cursor_), edits_.end());
    Entry entry; entry.before=std::move(before); entry.after=after;
    edits_.push_back(std::move(entry)); cursor_=edits_.size();
}

void EditHistory::PushZone(size_t index, const SpecialBox& before, const SpecialBox& after) {
    if (!diff3(before.min,after.min) && !diff3(before.max,after.max) &&
        before.action==after.action && before.free==after.free) return;
    if (cursor_ < edits_.size()) edits_.erase(edits_.begin()+static_cast<std::ptrdiff_t>(cursor_), edits_.end());
    Entry entry; entry.zone=ZoneEdit{index,before,after};
    edits_.push_back(std::move(entry));cursor_=edits_.size();
}

bool EditHistory::Undo(MapDocument& map, std::vector<size_t>& indices) {
    if (!CanUndo()) return false;
    indices.clear();
    const Entry& entry=edits_[cursor_-1];
    if (entry.before) { map=*entry.before; --cursor_; return true; }
    if (entry.zone) {
        if (!map.SetSpecialBox(entry.zone->index,entry.zone->before)) return false;
        --cursor_;return true;
    }
    for (const auto& edit:entry.transforms) {
        if (!map.SetPropTransform(edit.propIndex,edit.before.position,edit.before.rotation)) return false;
        indices.push_back(edit.propIndex);
    }
    --cursor_;
    return true;
}

bool EditHistory::Redo(MapDocument& map, std::vector<size_t>& indices) {
    if (!CanRedo()) return false;
    indices.clear();
    const Entry& entry=edits_[cursor_];
    if (entry.after) { map=*entry.after; ++cursor_; return true; }
    if (entry.zone) {
        if (!map.SetSpecialBox(entry.zone->index,entry.zone->after)) return false;
        ++cursor_;return true;
    }
    for (const auto& edit:entry.transforms) {
        if (!map.SetPropTransform(edit.propIndex,edit.after.position,edit.after.rotation)) return false;
        indices.push_back(edit.propIndex);
    }
    ++cursor_;
    return true;
}
