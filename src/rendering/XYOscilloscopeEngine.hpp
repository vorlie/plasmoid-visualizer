#pragma once

#include "XYOscilloscopeTypes.hpp"

#include <unordered_map>
#include <vector>

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
        float lastIntensity = 1.0f;
        bool hasLast = false;
        std::uint64_t lastTriggerFrame = 0;
        bool hasTriggerFrame = false;
        XYTraceBatch heldSweep;
        XYMeasurements cachedMeasurements;
        int framesSinceMeasure = 0;
        bool hasMeasured = false;
    };

private:
    struct PreparedSample {
        float x;
        float y;
        float z;
        bool invalid;
    };

    XYTraceBatch process(
        LayerId layerId, const XYLayerSettings& settings,
        const XYInputChunk& input, bool continuous,
        int viewportWidth, int viewportHeight);

    std::unordered_map<LayerId, Runtime> m_runtime;
    std::vector<float> m_conditionedX;
    std::vector<float> m_conditionedY;
    std::vector<PreparedSample> m_prepared;
    std::vector<PreparedSample> m_triggerScratch;
    std::vector<XYTracePoint> m_oversampleScratch;
    std::vector<float> m_crossingsScratch;
};
