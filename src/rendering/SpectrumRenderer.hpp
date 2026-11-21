#pragma once

#include "VisualizerPipeline.hpp"
#include "VisualizerTypes.hpp"

#include <vector>

struct SpectrumStyle {
    VisualizerShape shape = VisualizerShape::Bars;
    BarAnchor barAnchor = BarAnchor::Bottom;
    float heightScale = 0.2f;
    bool mirrored = false;
    float cornerRadius = 0.0f;
    float traceWidth = 2.0f;
    float maxLineWidth = 1.0f;
    float fillOpacity = 0.0f;
    float r = 1.0f, g = 1.0f, b = 1.0f, a = 1.0f;
};

class SpectrumRenderer {
public:
    void render(
        VisualizerPipeline& pipeline,
        const std::vector<float>& magnitudes,
        const SpectrumStyle& style,
        int viewportWidth,
        int viewportHeight);

    void drawSolidRects(
        VisualizerPipeline& pipeline,
        const std::vector<float>& vertices,
        const float color[4]) const;

private:
    std::vector<float> m_vertexScratch;
    std::vector<float> m_lineVertexScratch;
};
