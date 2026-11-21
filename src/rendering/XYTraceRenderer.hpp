#pragma once

#include "VisualizerPipeline.hpp"
#include "XYOscilloscopeTypes.hpp"

#include <vector>

class XYTraceRenderer {
public:
    void renderGrid(
        VisualizerPipeline& pipeline,
        int columns,
        int rows,
        int viewportWidth,
        int viewportHeight,
        float r, float g, float b, float a);

    void renderTrace(
        VisualizerPipeline& pipeline,
        const XYTraceBatch& trace,
        int viewportWidth,
        int viewportHeight,
        float traceWidth,
        float bloomIntensity,
        float r, float g, float b, float a);

private:
    std::vector<float> m_gridVertices;
    std::vector<float> m_centerlineScratch;
    std::vector<float> m_ribbonScratch;
};
