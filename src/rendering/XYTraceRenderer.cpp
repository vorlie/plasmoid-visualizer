#include "XYTraceRenderer.hpp"
#include "VisualizerGeometry.hpp"
#include "VisualizerTypes.hpp"

#include <algorithm>

void XYTraceRenderer::renderGrid(
    VisualizerPipeline& pipeline,
    int gridColumns,
    int gridRows,
    int viewportWidth,
    int viewportHeight,
    float r, float g, float b, float a
) {
    if (viewportWidth <= 0 || viewportHeight <= 0) return;

    float xScale = 1.0f;
    float yScale = 1.0f;
    const float aspectRatio = static_cast<float>(viewportWidth) / static_cast<float>(viewportHeight);
    if (aspectRatio > 1.0f) {
        xScale = 1.0f / aspectRatio;
    } else {
        yScale = aspectRatio;
    }

    const int columns = std::max(gridColumns, 1);
    const int rows = std::max(gridRows, 1);
    m_gridVertices.clear();
    const size_t needed = static_cast<size_t>((columns + 1) + (rows + 1)) * 2 * 5;
    if (m_gridVertices.capacity() < needed) m_gridVertices.reserve(needed);

    auto pushLine = [&](float x0, float y0, float x1, float y1) {
        m_gridVertices.push_back(x0); m_gridVertices.push_back(y0); m_gridVertices.push_back(0.5f); m_gridVertices.push_back(0.0f); m_gridVertices.push_back(1.0f);
        m_gridVertices.push_back(x1); m_gridVertices.push_back(y1); m_gridVertices.push_back(0.5f); m_gridVertices.push_back(1.0f); m_gridVertices.push_back(1.0f);
    };

    for (int i = 0; i <= columns; ++i) {
        const float p = -1.0f + 2.0f * static_cast<float>(i) / static_cast<float>(columns);
        pushLine(p * xScale, -1.0f * yScale, p * xScale, 1.0f * yScale);
    }
    for (int i = 0; i <= rows; ++i) {
        const float p = -1.0f + 2.0f * static_cast<float>(i) / static_cast<float>(rows);
        pushLine(-1.0f * xScale, p * yScale, 1.0f * xScale, p * yScale);
    }

    const GLint firstVertex = pipeline.uploadVertices(m_gridVertices);
    pipeline.use();
    pipeline.setColor(r * 0.15f, g * 0.15f, b * 0.15f, a * 0.4f);
    pipeline.setCornerRadius(0.0f);
    pipeline.setShape(static_cast<int>(VisualizerShape::Lines));
    glLineWidth(1.0f);
    glDrawArrays(GL_LINES, firstVertex, static_cast<GLsizei>(m_gridVertices.size() / 5));
}

void XYTraceRenderer::renderTrace(
    VisualizerPipeline& pipeline,
    const XYTraceBatch& trace,
    int viewportWidth,
    int viewportHeight,
    float traceWidth,
    float bloomIntensity,
    float r, float g, float b, float a
) {
    if (trace.points.empty()) return;

    pipeline.use();
    pipeline.setShape(static_cast<int>(VisualizerShape::OscilloscopeXY));
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    const float bloom = std::clamp(bloomIntensity, 0.0f, 5.0f);

    auto drawSegment = [&](const std::vector<float>& centerline) {
        if (centerline.size() < 10) return;
        auto drawPass = [&](float width, float alpha, float whiteMix, float emission) {
            VisualizerGeometry::buildTraceRibbon(
                centerline, width, viewportWidth, viewportHeight, m_ribbonScratch);
            if (m_ribbonScratch.empty()) return;
            const GLint firstVertex = pipeline.uploadVertices(m_ribbonScratch);
            pipeline.setColor(
                (r + (1.0f - r) * whiteMix) * emission,
                (g + (1.0f - g) * whiteMix) * emission,
                (b + (1.0f - b) * whiteMix) * emission,
                a * alpha);
            glDrawArrays(GL_TRIANGLE_STRIP, firstVertex, static_cast<GLsizei>(m_ribbonScratch.size() / 5));
        };
        drawPass(traceWidth + 1.5f, 0.35f, 0.0f, 0.55f + bloom * 0.2f);
        drawPass(traceWidth, 0.95f, 0.25f, 0.8f + bloom * 0.65f);
    };

    const float xAspectCompensation = viewportWidth > 0
        ? static_cast<float>(viewportHeight) / static_cast<float>(viewportWidth)
        : 1.0f;
    m_centerlineScratch.clear();
    for (const auto& point : trace.points) {
        if (point.breakBefore) {
            drawSegment(m_centerlineScratch);
            m_centerlineScratch.clear();
        }
        m_centerlineScratch.push_back(point.x * xAspectCompensation);
        m_centerlineScratch.push_back(point.y);
        m_centerlineScratch.push_back(0.0f);
        m_centerlineScratch.push_back(0.0f);
        m_centerlineScratch.push_back(point.intensity);
    }
    drawSegment(m_centerlineScratch);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}
