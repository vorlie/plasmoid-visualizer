#include "VisualizerGeometry.hpp"

#include <algorithm>
#include <cmath>

float VisualizerGeometry::catmullRom(float p0, float p1, float p2, float p3, float t) {
    return 0.5f * (
        (2.0f * p1) +
        (-p0 + p2) * t +
        (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t * t +
        (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t * t * t
    );
}

std::vector<float> VisualizerGeometry::buildTraceRibbon(
    const std::vector<float>& centerline,
    float widthPixels,
    int viewportWidth,
    int viewportHeight
) {
    const size_t pointCount = centerline.size() / 5;
    std::vector<float> ribbon;
    if (pointCount < 2 || viewportWidth <= 0 || viewportHeight <= 0) return ribbon;
    ribbon.reserve(pointCount * 10);

    const float halfWidth = std::max(widthPixels, 1.0f) * 0.5f;
    for (size_t i = 0; i < pointCount; ++i) {
        const size_t previous = i > 0 ? i - 1 : i;
        const size_t next = i + 1 < pointCount ? i + 1 : i;

        float dx = (centerline[next * 5] - centerline[previous * 5]) * viewportWidth * 0.5f;
        float dy = (centerline[next * 5 + 1] - centerline[previous * 5 + 1]) * viewportHeight * 0.5f;
        const float length = std::sqrt(dx * dx + dy * dy);
        if (length > 1e-5f) {
            dx /= length;
            dy /= length;
        } else {
            dx = 1.0f;
            dy = 0.0f;
        }

        const float offsetX = -dy * halfWidth * 2.0f / viewportWidth;
        const float offsetY = dx * halfWidth * 2.0f / viewportHeight;
        const float x = centerline[i * 5];
        const float y = centerline[i * 5 + 1];
        const float intensity = centerline[i * 5 + 4];

        ribbon.insert(ribbon.end(), {x + offsetX, y + offsetY, 0.0f, 0.0f, intensity});
        ribbon.insert(ribbon.end(), {x - offsetX, y - offsetY, 0.0f, 1.0f, intensity});
    }
    return ribbon;
}
