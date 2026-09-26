#pragma once
#include <cmath>

// The grid is a movement contract, not merely a drawn overlay. Older UI versions
// permitted the visually selected step to be silently disabled by Snap toggle.
namespace GridStep {
inline float Quantize(float value,float step) {
    if (!std::isfinite(value) || !std::isfinite(step) || step<=0.0001f) return value;
    return std::round(value/step)*step;
}
// Anchor-relative quantization preserves the original centre offset of legacy tiles.
inline float QuantizeAroundAnchor(float target,float step,float anchor) {
    return anchor+Quantize(target-anchor,step);
}
inline float KeyboardStep(float grid,bool fine) {
    const float unit=grid>0.0001f?grid:100.0f;
    return fine?unit*0.1f:unit;
}
}
