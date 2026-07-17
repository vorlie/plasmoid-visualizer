#include "VisualizerGeometry.hpp"

#include <cmath>
#include <iostream>
#include <vector>

namespace {
bool approximatelyEqual(float left, float right, float epsilon = 1e-5f) {
    return std::abs(left - right) <= epsilon;
}
}

int main() {
    const float p1 = VisualizerGeometry::catmullRom(0.0f, 1.0f, 2.0f, 3.0f, 0.0f);
    const float p2 = VisualizerGeometry::catmullRom(0.0f, 1.0f, 2.0f, 3.0f, 1.0f);
    if (!approximatelyEqual(p1, 1.0f) || !approximatelyEqual(p2, 2.0f)) {
        std::cerr << "Catmull-Rom endpoints are incorrect\n";
        return 1;
    }

    const std::vector<float> centerline = {
        -0.5f, 0.0f, 0.0f, 0.0f, 1.0f,
         0.5f, 0.0f, 0.0f, 0.0f, 0.5f
    };
    const auto ribbon = VisualizerGeometry::buildTraceRibbon(centerline, 4.0f, 100, 100);
    if (ribbon.size() != 20) {
        std::cerr << "Trace ribbon has an unexpected vertex count\n";
        return 1;
    }
    if (!approximatelyEqual(ribbon[1], 0.04f) || !approximatelyEqual(ribbon[6], -0.04f)) {
        std::cerr << "Trace ribbon width is incorrect\n";
        return 1;
    }

    return 0;
}
