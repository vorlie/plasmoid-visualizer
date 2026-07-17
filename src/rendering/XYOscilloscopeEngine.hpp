#pragma once

#include "XYOscilloscopeTypes.hpp"

#include <unordered_map>

class XYOscilloscopeEngine {
public:
    XYTraceBatch processContinuous(
        LayerId layerId, const XYLayerSettings& settings,
        const XYInputChunk& input, int viewportWidth, int viewportHeight);
    XYTraceBatch processTriggered(
        LayerId layerId, const XYLayerSettings& settings,
        const XYInputChunk& input, int viewportWidth, int viewportHeight);
    void reset(LayerId layerId);
    void resetAll();

    struct AxisFilter {
        float dcEstimate = 0.0f;
        float x1 = 0.0f, x2 = 0.0f;
        float y1 = 0.0f, y2 = 0.0f;
    };
    struct Runtime {
        AxisFilter xFilter;
        AxisFilter yFilter;
        std::uint64_t generation = 0;
        float autoGain = 1.0f;
        float lastX = 0.0f;
        float lastY = 0.0f;
        bool hasLast = false;
        XYTraceBatch heldSweep;
    };

private:
    XYTraceBatch process(
        LayerId layerId, const XYLayerSettings& settings,
        const XYInputChunk& input, bool continuous,
        int viewportWidth, int viewportHeight);
    std::unordered_map<LayerId, Runtime> m_runtime;
};
