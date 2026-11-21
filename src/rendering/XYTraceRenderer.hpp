#pragma once

#include "ShaderProgram.hpp"
#include "StreamingVertexBuffer.hpp"
#include "VisualizerPipeline.hpp"
#include "XYOscilloscopeTypes.hpp"

#include <GL/glew.h>
#include <string>
#include <vector>

class XYTraceRenderer {
public:
    XYTraceRenderer() = default;
    ~XYTraceRenderer() = default;
    XYTraceRenderer(const XYTraceRenderer&) = delete;
    XYTraceRenderer& operator=(const XYTraceRenderer&) = delete;
    XYTraceRenderer(XYTraceRenderer&&) = delete;
    XYTraceRenderer& operator=(XYTraceRenderer&&) = delete;

    void initialize();
    bool reloadShaders(bool force = false, std::string* errorOut = nullptr);

    void renderGrid(
        VisualizerPipeline& pipeline,
        int columns,
        int rows,
        int viewportWidth,
        int viewportHeight,
        float r, float g, float b, float a);

    void renderTrace(
        const XYTraceBatch& trace,
        int viewportWidth,
        int viewportHeight,
        float traceWidth,
        float bloomIntensity,
        float beamHeadSize,
        float r, float g, float b, float a);

private:
    void cacheUniforms() noexcept;

    ShaderProgram m_beamProgram;
    GLint m_colorLoc = -1;
    GLint m_bloomLoc = -1;
    StreamingVertexBuffer m_beamVertices;

    std::vector<float> m_gridVertices;
    std::vector<float> m_accentGridVertices;
    std::vector<float> m_capsuleScratch;
};
