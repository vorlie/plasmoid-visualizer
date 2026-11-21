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

void VisualizerGeometry::buildTraceRibbon(
    const std::vector<float>& centerline,
    float widthPixels,
    int viewportWidth,
    int viewportHeight,
    std::vector<float>& ribbon
) {
    ribbon.clear();
    const size_t pointCount = centerline.size() / 5;
    if (pointCount < 2 || viewportWidth <= 0 || viewportHeight <= 0) return;
    if (ribbon.capacity() < pointCount * 10) {
        ribbon.reserve(pointCount * 10);
    }

    const float halfWidth = std::max(widthPixels, 1.0f) * 0.5f;
    const float halfVpW = static_cast<float>(viewportWidth) * 0.5f;
    const float halfVpH = static_cast<float>(viewportHeight) * 0.5f;
    const float invHalfVpW = 2.0f / static_cast<float>(viewportWidth);
    const float invHalfVpH = 2.0f / static_cast<float>(viewportHeight);

    auto direction = [&](size_t from, size_t to, float& x, float& y) {
        x = (centerline[to * 5] - centerline[from * 5]) * halfVpW;
        y = (centerline[to * 5 + 1] - centerline[from * 5 + 1]) * halfVpH;
        const float length = std::sqrt(x * x + y * y);
        if (length <= 1e-5f) return false;
        const float invLength = 1.0f / length;
        x *= invLength;
        y *= invLength;
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
            const float invTan = 1.0f / tangentLength;
            tangentX *= invTan;
            tangentY *= invTan;
        } else {
            tangentX = outgoingX;
            tangentY = outgoingY;
        }

        const float normalX = -tangentY;
        const float normalY = tangentX;
        const float outgoingNormalX = -outgoingY;
        const float outgoingNormalY = outgoingX;
        const float alignment = std::abs(normalX * outgoingNormalX + normalY * outgoingNormalY);
        const float miterLength = std::min(
            halfWidth / std::max(alignment, 0.25f),
            halfWidth * 2.0f);

        const float offsetX = normalX * miterLength * invHalfVpW;
        const float offsetY = normalY * miterLength * invHalfVpH;
        const float x = centerline[i * 5];
        const float y = centerline[i * 5 + 1];
        const float intensity = centerline[i * 5 + 4];

        ribbon.push_back(x + offsetX);
        ribbon.push_back(y + offsetY);
        ribbon.push_back(0.0f);
        ribbon.push_back(0.0f);
        ribbon.push_back(intensity);

        ribbon.push_back(x - offsetX);
        ribbon.push_back(y - offsetY);
        ribbon.push_back(0.0f);
        ribbon.push_back(1.0f);
        ribbon.push_back(intensity);
    }
}

std::vector<float> VisualizerGeometry::buildTraceRibbon(
    const std::vector<float>& centerline,
    float widthPixels,
    int viewportWidth,
    int viewportHeight
) {
    std::vector<float> ribbon;
    buildTraceRibbon(centerline, widthPixels, viewportWidth, viewportHeight, ribbon);
    return ribbon;
}
