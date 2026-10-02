#pragma once
// Shared, side-effect-free AX history view: draw and Tab+wheel must use exactly
// the same positions in the original recent history (not row indices).
// Deliberately no Win32/ImGui dependencies: native regression can exercise it.
#include <algorithm>
#include <cstddef>
#include <vector>

namespace AxRecentFilter {
inline float FeedbackAlpha(double elapsed){return elapsed<0?0.f:elapsed<=1.0?1.f:static_cast<float>(std::clamp((1.6-elapsed)/.6,0.0,1.0));}
template<class Recent> bool ManuallyAdded(const Recent& r){
    if constexpr(requires { r.manuallyAdded; })return r.manuallyAdded;
    else return false;
}
template <class Recent, class Asset, class Prop>
std::vector<std::size_t> VisiblePositions(const std::vector<Recent>& recent,
                                          const std::vector<Asset>& assets,
                                          const std::vector<Prop>& placed,
                                          bool onlyUsed) {
    std::vector<std::size_t> positions;
    positions.reserve(recent.size());
    for (std::size_t i=0; i<recent.size(); ++i) {
        const std::size_t id=recent[i].index;
        if (id>=assets.size()) continue;
        if (onlyUsed && !ManuallyAdded(recent[i])) {
            const auto& asset=assets[id];
            if (std::none_of(placed.begin(),placed.end(),[&](const Prop& prop) {
                return prop.library==asset.library && prop.group==asset.group && prop.name==asset.name;
            })) continue;
        }
        positions.push_back(i);
    }
    return positions;
}

inline int ClampRow(int row, std::size_t count) {
    if (!count) return 0;
    return std::clamp(row,0,static_cast<int>(count)-1);
}

inline int NextRow(int row, std::size_t count, float wheel) {
    if (!count || wheel==0.0f) return ClampRow(row,count);
    const int current=ClampRow(row,count);
    const int n=static_cast<int>(count);
    return (current+(wheel<0.0f?1:n-1))%n;
}
} // namespace AxRecentFilter
