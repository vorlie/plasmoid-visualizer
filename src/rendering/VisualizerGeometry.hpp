#pragma once

#include <vector>

namespace VisualizerGeometry {
float catmullRom(float p0, float p1, float p2, float p3, float t);

std::vector<float> buildTraceRibbon(
    const std::vector<float>& centerline,
    float widthPixels,
    int viewportWidth,
    int viewportHeight);
}
