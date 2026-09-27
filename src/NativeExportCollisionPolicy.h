#pragma once
// Conservative explicit export routing. Original plane/mixed helpers are not
// convertible to new axis-aligned SOLID draft boxes without game evidence.
#include <cstddef>
#include <string>
namespace NativeExportCollisionPolicy {
inline bool Validate(bool decorative,bool originalValid,size_t planes,size_t boxes,
                     size_t triangles,bool helperless,size_t authoredBoxes,std::string& error) {
    if(decorative)return true; // intentional visual-only object
    if(!originalValid && !helperless){error="Original collision helpers are unsupported.";return false;}
    if(triangles && (planes || boxes)) {
        error="Mixed triangle/plane/box collision cannot be converted safely.";return false;
    }
    if(planes) {
        error="Original plane collision needs a separately verified native exporter; export refused.";return false;
    }
    if(triangles)return true; // NativeTerrainDelta validates topology and source frames
    if(!authoredBoxes) {
        error="Physical native export needs explicitly authored SOLID boxes.";return false;
    }
    return true; // helperless/box-only template: explicitly reauthored boxes
}
}
