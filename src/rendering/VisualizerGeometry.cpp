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

    auto direction = [&](size_t from, size_t to, float& x, float& y) {
        x = (centerline[to * 5] - centerline[from * 5]) * viewportWidth * 0.5f;
        y = (centerline[to * 5 + 1] - centerline[from * 5 + 1]) * viewportHeight * 0.5f;
        const float length = std::sqrt(x * x + y * y);
        if (length <= 1e-5f) return false;
        x /= length;
        y /= length;
        return true;
    };

    for (size_t i = 0; i < pointCount; ++i) {
        const size_t previous = i > 0 ? i - 1 : i;
        const size_t next = i + 1 < pointCount ? i + 1 : i;

        float incomingX = 0.0f;
        float incomingY = 0.0f;
        float outgoingX = 0.0f;
        float outgoingY = 0.0f;
        const bool hasIncoming = previous != i && direction(previous, i, incomingX, incomingY);
        const bool hasOutgoing = next != i && direction(i, next, outgoingX, outgoingY);

        if (!hasIncoming && hasOutgoing) {
            incomingX = outgoingX;
            incomingY = outgoingY;
        } else if (hasIncoming && !hasOutgoing) {
            outgoingX = incomingX;
            outgoingY = incomingY;
        } else if (!hasIncoming && !hasOutgoing) {
            incomingX = outgoingX = 1.0f;
            incomingY = outgoingY = 0.0f;
        }

        // Average adjacent segment directions to form a mitered join. Clamping
        // prevents long spikes where a trace folds back on itself.
        float tangentX = incomingX + outgoingX;
        float tangentY = incomingY + outgoingY;
        const float tangentLength = std::sqrt(tangentX * tangentX + tangentY * tangentY);
        if (tangentLength > 1e-5f) {
            tangentX /= tangentLength;
            tangentY /= tangentLength;
        } else {
            tangentX = outgoingX;
            tangentY = outgoingY;
        }

        float normalX = -tangentY;
        float normalY = tangentX;
        const float outgoingNormalX = -outgoingY;
        const float outgoingNormalY = outgoingX;
        const float alignment = std::abs(normalX * outgoingNormalX + normalY * outgoingNormalY);
        const float miterLength = std::min(
            halfWidth / std::max(alignment, 0.25f),
            halfWidth * 2.0f);

        const float offsetX = normalX * miterLength * 2.0f / viewportWidth;
        const float offsetY = normalY * miterLength * 2.0f / viewportHeight;
        const float x = centerline[i * 5];
        const float y = centerline[i * 5 + 1];
        const float intensity = centerline[i * 5 + 4];

        ribbon.insert(ribbon.end(), {x + offsetX, y + offsetY, 0.0f, 0.0f, intensity});
        ribbon.insert(ribbon.end(), {x - offsetX, y - offsetY, 0.0f, 1.0f, intensity});
    }
    return ribbon;
}
