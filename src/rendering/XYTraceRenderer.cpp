#include "XYTraceRenderer.hpp"
#include "AssetPaths.hpp"
#include "VisualizerTypes.hpp"

#include <algorithm>
#include <cmath>

void XYTraceRenderer::cacheUniforms() noexcept {
    m_colorLoc = glGetUniformLocation(m_beamProgram, "uColor");
    m_bloomLoc = glGetUniformLocation(m_beamProgram, "uBloom");
}

void XYTraceRenderer::initialize() {
    m_beamProgram.load(
        AssetPaths::shader("xy_beam.vert"),
        AssetPaths::shader("xy_beam.frag"));
    cacheUniforms();

    // 8 floats per vertex: aPos (2), aLocal (2), aParams (4)
    m_beamVertices.initialize(8, 512 * 1024);
    constexpr GLsizei stride = 8 * sizeof(float);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(4 * sizeof(float)));
    glEnableVertexAttribArray(2);
    m_beamVertices.finishLayout();
}

bool XYTraceRenderer::reloadShaders(bool force, std::string* errorOut) {
    if (!m_beamProgram.reloadIfChanged(force, errorOut)) return false;
    cacheUniforms();
    return true;
}

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
    m_accentGridVertices.clear();

    auto pushLine = [](std::vector<float>& dst, float x0, float y0, float x1, float y1) {
        dst.push_back(x0); dst.push_back(y0); dst.push_back(0.5f); dst.push_back(0.0f); dst.push_back(1.0f);
        dst.push_back(x1); dst.push_back(y1); dst.push_back(0.5f); dst.push_back(1.0f); dst.push_back(1.0f);
    };

    // Interior major grid divisions (subtle)
    for (int i = 1; i < columns; ++i) {
        const float p = -1.0f + 2.0f * static_cast<float>(i) / static_cast<float>(columns);
        if (std::abs(p) < 1e-4f) continue; // Center axis drawn in emphasized pass
        pushLine(m_gridVertices, p * xScale, -yScale, p * xScale, yScale);
    }
    for (int i = 1; i < rows; ++i) {
        const float p = -1.0f + 2.0f * static_cast<float>(i) / static_cast<float>(rows);
        if (std::abs(p) < 1e-4f) continue;
        pushLine(m_gridVertices, -xScale, p * yScale, xScale, p * yScale);
    }

    // Outer Tektronix bezel border + center crosshairs
    pushLine(m_accentGridVertices, -xScale, -yScale,  xScale, -yScale);
    pushLine(m_accentGridVertices,  xScale, -yScale,  xScale,  yScale);
    pushLine(m_accentGridVertices,  xScale,  yScale, -xScale,  yScale);
    pushLine(m_accentGridVertices, -xScale,  yScale, -xScale, -yScale);
    pushLine(m_accentGridVertices, -xScale,  0.0f,    xScale,  0.0f);
    pushLine(m_accentGridVertices,  0.0f,   -yScale,  0.0f,    yScale);

    // Classic 0.2-division minor tick marks along both center X and Y axes
    constexpr int ticksPerDivision = 5;
    const float tickHalf = 0.018f;
    const int totalXTicks = columns * ticksPerDivision;
    for (int i = 1; i < totalXTicks; ++i) {
        const float p = -1.0f + 2.0f * static_cast<float>(i) / static_cast<float>(totalXTicks);
        const float halfLen = (i % ticksPerDivision == 0) ? (tickHalf * 1.45f) : tickHalf;
        pushLine(m_accentGridVertices, p * xScale, -halfLen * yScale, p * xScale, halfLen * yScale);
    }
    const int totalYTicks = rows * ticksPerDivision;
    for (int i = 1; i < totalYTicks; ++i) {
        const float p = -1.0f + 2.0f * static_cast<float>(i) / static_cast<float>(totalYTicks);
        const float halfLen = (i % ticksPerDivision == 0) ? (tickHalf * 1.45f) : tickHalf;
        pushLine(m_accentGridVertices, -halfLen * xScale, p * yScale, halfLen * xScale, p * yScale);
    }

    pipeline.use();
    pipeline.setCornerRadius(0.0f);
    pipeline.setShape(static_cast<int>(VisualizerShape::Lines));
    glLineWidth(1.0f);

    if (!m_gridVertices.empty()) {
        const GLint firstVertex = pipeline.uploadVertices(m_gridVertices);
        pipeline.setColor(r * 0.45f, g * 0.45f, b * 0.45f, a * 0.65f);
        glDrawArrays(GL_LINES, firstVertex, static_cast<GLsizei>(m_gridVertices.size() / 5));
    }
    if (!m_accentGridVertices.empty()) {
        const GLint firstVertex = pipeline.uploadVertices(m_accentGridVertices);
        pipeline.setColor(r * 0.72f, g * 0.72f, b * 0.72f, std::min(a * 1.15f, 1.0f));
        glDrawArrays(GL_LINES, firstVertex, static_cast<GLsizei>(m_accentGridVertices.size() / 5));
    }
}

void XYTraceRenderer::renderTrace(
    const XYTraceBatch& trace,
    int viewportWidth,
    int viewportHeight,
    float traceWidth,
    float bloomIntensity,
    float beamHeadSize,
    float r, float g, float b, float a
) {
    if (trace.points.empty() || viewportWidth <= 0 || viewportHeight <= 0) return;

    float xScale = 1.0f;
    float yScale = 1.0f;
    const float aspectRatio = static_cast<float>(viewportWidth) / static_cast<float>(viewportHeight);
    if (aspectRatio > 1.0f) {
        xScale = 1.0f / aspectRatio;
    } else {
        yScale = aspectRatio;
    }

    const float halfW = static_cast<float>(viewportWidth) * 0.5f;
    const float halfH = static_cast<float>(viewportHeight) * 0.5f;
    const float invHalfW = 1.0f / halfW;
    const float invHalfH = 1.0f / halfH;
    const float sigma = std::max(traceWidth * 0.38f, 0.45f);
    const float bloom = std::clamp(bloomIntensity, 0.0f, 5.0f);

    m_capsuleScratch.clear();
    const size_t estimatedQuads = trace.points.size() + 2;
    if (m_capsuleScratch.capacity() < estimatedQuads * 48) {
        m_capsuleScratch.reserve(estimatedQuads * 48);
    }

    auto emitCapsule = [&](
        float x0Ndc, float y0Ndc, float i0,
        float x1Ndc, float y1Ndc, float i1,
        float segSigma
    ) {
        const float x0Px = x0Ndc * halfW;
        const float y0Px = y0Ndc * halfH;
        const float x1Px = x1Ndc * halfW;
        const float y1Px = y1Ndc * halfH;
        const float dx = x1Px - x0Px;
        const float dy = y1Px - y0Px;
        float len = std::hypot(dx, dy);

        float tx = 1.0f, ty = 0.0f;
        if (len > 1e-4f) {
            const float invLen = 1.0f / len;
            tx = dx * invLen;
            ty = dy * invLen;
        } else {
            len = 0.0f;
        }
        const float nx = -ty;
        const float ny = tx;
        const float pad = segSigma * 5.5f;

        const float ax = (x0Px - tx * pad - nx * pad) * invHalfW;
        const float ay = (y0Px - ty * pad - ny * pad) * invHalfH;
        const float bx = (x0Px + tx * (len + pad) - nx * pad) * invHalfW;
        const float by = (y0Px + ty * (len + pad) - ny * pad) * invHalfH;
        const float cx = (x0Px + tx * (len + pad) + nx * pad) * invHalfW;
        const float cy = (y0Px + ty * (len + pad) + ny * pad) * invHalfH;
        const float ex = (x0Px - tx * pad + nx * pad) * invHalfW;
        const float ey = (y0Px - ty * pad + ny * pad) * invHalfH;

        auto pushVert = [&](float vx, float vy, float lx, float ly) {
            m_capsuleScratch.push_back(vx);
            m_capsuleScratch.push_back(vy);
            m_capsuleScratch.push_back(lx);
            m_capsuleScratch.push_back(ly);
            m_capsuleScratch.push_back(len);
            m_capsuleScratch.push_back(segSigma);
            m_capsuleScratch.push_back(i0);
            m_capsuleScratch.push_back(i1);
        };

        pushVert(ax, ay, -pad, -pad);
        pushVert(bx, by, len + pad, -pad);
        pushVert(cx, cy, len + pad, pad);

        pushVert(ax, ay, -pad, -pad);
        pushVert(cx, cy, len + pad, pad);
        pushVert(ex, ey, -pad, pad);
    };

    const auto& pts = trace.points;
    size_t runStart = 0;
    while (runStart < pts.size()) {
        size_t runEnd = runStart + 1;
        while (runEnd < pts.size() && !pts[runEnd].breakBefore) {
            ++runEnd;
        }

        if (runEnd - runStart == 1) {
            // Stationary single-point beam dwell -> circular Gaussian CRT spot.
            const auto& p = pts[runStart];
            emitCapsule(
                p.x * xScale, p.y * yScale, p.intensity,
                p.x * xScale, p.y * yScale, p.intensity,
                sigma);
        } else {
            for (size_t k = runStart; k + 1 < runEnd; ++k) {
                const auto& p0 = pts[k];
                const auto& p1 = pts[k + 1];
                emitCapsule(
                    p0.x * xScale, p0.y * yScale, p0.intensity,
                    p1.x * xScale, p1.y * yScale, p1.intensity,
                    sigma);
            }
        }
        runStart = runEnd;
    }

    // Optional electron-beam head dot at the latest active beam coordinate.
    if (beamHeadSize > 0.01f) {
        const auto& head = pts.back();
        const float headSigma = std::max(beamHeadSize * 0.35f, sigma * 1.15f);
        const float headIntensity = std::max(head.intensity, 0.35f) * 1.45f;
        emitCapsule(
            head.x * xScale, head.y * yScale, headIntensity,
            head.x * xScale, head.y * yScale, headIntensity,
            headSigma);
    }

    if (m_capsuleScratch.empty()) return;

    const GLint firstVertex = m_beamVertices.upload(m_capsuleScratch);
    glUseProgram(m_beamProgram);
    glUniform4f(m_colorLoc, r, g, b, a);
    glUniform1f(m_bloomLoc, bloom);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    glDrawArrays(GL_TRIANGLES, firstVertex, static_cast<GLsizei>(m_capsuleScratch.size() / 8));
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(0);
    glBindVertexArray(0);
}
