#include "SpectrumRenderer.hpp"
#include "VisualizerGeometry.hpp"

#include <algorithm>
#include <utility>

void SpectrumRenderer::drawSolidRects(
    VisualizerPipeline& pipeline,
    const std::vector<float>& vertices,
    const float color[4]
) const {
    if (vertices.size() < 5) return;
    const GLint firstVertex = pipeline.uploadVertices(vertices);
    pipeline.use();
    pipeline.setColor(color[0], color[1], color[2], color[3]);
    pipeline.setCornerRadius(0.0f);
    pipeline.setShape(static_cast<int>(VisualizerShape::Bars));
    glDrawArrays(GL_TRIANGLES, firstVertex, static_cast<GLsizei>(vertices.size() / 5));
    glUseProgram(0);
}

void SpectrumRenderer::render(
    VisualizerPipeline& pipeline,
    const std::vector<float>& magnitudes,
    const SpectrumStyle& style,
    int viewportWidth,
    int viewportHeight
) {
    if (magnitudes.empty() || viewportWidth <= 0 || viewportHeight <= 0) return;

    m_vertexScratch.clear();
    auto& vertices = m_vertexScratch;
    const size_t numBars = magnitudes.size();
    const float effectiveBarWidth = style.mirrored ? (1.0f / numBars) : (2.0f / numBars);

    auto addBar = [&](float x, float h, float width) {
        auto transformCoord = [&](float localX, float localY, float& outX, float& outY) {
            switch (style.barAnchor) {
                case BarAnchor::Bottom:
                    outX = localX;
                    outY = localY;
                    break;
                case BarAnchor::Top:
                    outX = localX;
                    outY = -localY;
                    break;
                case BarAnchor::Left:
                    outX = localY;
                    outY = -localX;
                    break;
                case BarAnchor::Right:
                    outX = -localY;
                    outY = localX;
                    break;
                case BarAnchor::Center:
                    outX = localX;
                    outY = localY + 1.0f - h * 0.5f;
                    break;
            }
        };

        if (style.shape == VisualizerShape::Bars) {
            const float x0 = x, y0 = -1.0f;
            const float x1 = x + width, y1 = -1.0f;
            const float x2 = x, y2 = -1.0f + h;
            const float x3 = x + width, y3 = -1.0f + h;

            float tx0, ty0, tx1, ty1, tx2, ty2, tx3, ty3;
            transformCoord(x0, y0, tx0, ty0);
            transformCoord(x1, y1, tx1, ty1);
            transformCoord(x2, y2, tx2, ty2);
            transformCoord(x3, y3, tx3, ty3);

            vertices.push_back(tx0); vertices.push_back(ty0); vertices.push_back(0.0f); vertices.push_back(0.0f); vertices.push_back(1.0f);
            vertices.push_back(tx1); vertices.push_back(ty1); vertices.push_back(1.0f); vertices.push_back(0.0f); vertices.push_back(1.0f);
            vertices.push_back(tx2); vertices.push_back(ty2); vertices.push_back(0.0f); vertices.push_back(1.0f); vertices.push_back(1.0f);

            vertices.push_back(tx1); vertices.push_back(ty1); vertices.push_back(1.0f); vertices.push_back(0.0f); vertices.push_back(1.0f);
            vertices.push_back(tx3); vertices.push_back(ty3); vertices.push_back(1.0f); vertices.push_back(1.0f); vertices.push_back(1.0f);
            vertices.push_back(tx2); vertices.push_back(ty2); vertices.push_back(0.0f); vertices.push_back(1.0f); vertices.push_back(1.0f);
        } else if (style.shape == VisualizerShape::Lines) {
            const float lx0 = x + width * 0.5f, ly0 = -1.0f;
            const float lx1 = x + width * 0.5f, ly1 = -1.0f + h;
            float tx0, ty0, tx1, ty1;
            transformCoord(lx0, ly0, tx0, ty0);
            transformCoord(lx1, ly1, tx1, ty1);
            vertices.push_back(tx0); vertices.push_back(ty0); vertices.push_back(0.5f); vertices.push_back(0.0f); vertices.push_back(1.0f);
            vertices.push_back(tx1); vertices.push_back(ty1); vertices.push_back(0.5f); vertices.push_back(1.0f); vertices.push_back(1.0f);
        } else if (style.shape == VisualizerShape::Dots) {
            const float dotSize = width * 0.8f;
            const float centerX = x + width * 0.5f;
            const float centerY = -1.0f + h;
            float tcx, tcy;
            transformCoord(centerX, centerY, tcx, tcy);

            float dx = dotSize * 0.5f;
            float dy = dotSize * 0.5f;
            if (style.barAnchor == BarAnchor::Left || style.barAnchor == BarAnchor::Right) {
                std::swap(dx, dy);
            }

            vertices.push_back(tcx - dx); vertices.push_back(tcy - dy); vertices.push_back(0.0f); vertices.push_back(0.0f); vertices.push_back(1.0f);
            vertices.push_back(tcx + dx); vertices.push_back(tcy - dy); vertices.push_back(1.0f); vertices.push_back(0.0f); vertices.push_back(1.0f);
            vertices.push_back(tcx - dx); vertices.push_back(tcy + dy); vertices.push_back(0.0f); vertices.push_back(1.0f); vertices.push_back(1.0f);

            vertices.push_back(tcx + dx); vertices.push_back(tcy - dy); vertices.push_back(1.0f); vertices.push_back(0.0f); vertices.push_back(1.0f);
            vertices.push_back(tcx + dx); vertices.push_back(tcy + dy); vertices.push_back(1.0f); vertices.push_back(1.0f); vertices.push_back(1.0f);
            vertices.push_back(tcx - dx); vertices.push_back(tcy + dy); vertices.push_back(0.0f); vertices.push_back(1.0f); vertices.push_back(1.0f);
        }
    };

    if (style.shape == VisualizerShape::Waveform) {
        const size_t step = std::max<size_t>(1, numBars / 2048);
        for (size_t i = 0; i < numBars; i += step) {
            const float x = -1.0f + 2.0f * static_cast<float>(i) / static_cast<float>(numBars);
            const float y = magnitudes[i] * style.heightScale;
            vertices.push_back(x); vertices.push_back(y); vertices.push_back(0.0f); vertices.push_back(0.0f); vertices.push_back(1.0f);
        }
    } else if (style.shape == VisualizerShape::Curve) {
        m_lineVertexScratch.clear();
        auto appendCurveRange = [&](float xOrigin, float xStep) {
            constexpr int segmentsPerBar = 8;
            for (size_t i = 0; i + 1 < numBars; ++i) {
                const float p0 = std::min((i > 0 ? magnitudes[i - 1] : magnitudes[i]) * style.heightScale, 2.0f) - 1.0f;
                const float p1 = std::min(magnitudes[i] * style.heightScale, 2.0f) - 1.0f;
                const float p2 = std::min(magnitudes[i + 1] * style.heightScale, 2.0f) - 1.0f;
                const float p3 = std::min((i + 2 < numBars ? magnitudes[i + 2] : magnitudes[i + 1]) * style.heightScale, 2.0f) - 1.0f;

                const float xStart = xOrigin + i * xStep;
                const float xEnd = xOrigin + (i + 1) * xStep;

                for (int s = 0; s < segmentsPerBar; ++s) {
                    const float t1 = static_cast<float>(s) / segmentsPerBar;
                    const float t2 = static_cast<float>(s + 1) / segmentsPerBar;
                    const float h1 = VisualizerGeometry::catmullRom(p0, p1, p2, p3, t1);
                    const float h2 = VisualizerGeometry::catmullRom(p0, p1, p2, p3, t2);
                    const float x1 = xStart + (xEnd - xStart) * t1;
                    const float x2 = xStart + (xEnd - xStart) * t2;

                    if (style.fillOpacity > 0.0001f) {
                        vertices.push_back(x1); vertices.push_back(-1.0f); vertices.push_back(0.0f); vertices.push_back(0.0f); vertices.push_back(1.0f);
                        vertices.push_back(x2); vertices.push_back(-1.0f); vertices.push_back(0.0f); vertices.push_back(0.0f); vertices.push_back(1.0f);
                        vertices.push_back(x1); vertices.push_back(h1);    vertices.push_back(0.0f); vertices.push_back(0.0f); vertices.push_back(1.0f);

                        vertices.push_back(x2); vertices.push_back(-1.0f); vertices.push_back(0.0f); vertices.push_back(0.0f); vertices.push_back(1.0f);
                        vertices.push_back(x2); vertices.push_back(h2);    vertices.push_back(0.0f); vertices.push_back(0.0f); vertices.push_back(1.0f);
                        vertices.push_back(x1); vertices.push_back(h1);    vertices.push_back(0.0f); vertices.push_back(0.0f); vertices.push_back(1.0f);
                    }

                    m_lineVertexScratch.push_back(x1); m_lineVertexScratch.push_back(h1); m_lineVertexScratch.push_back(0.0f); m_lineVertexScratch.push_back(0.0f); m_lineVertexScratch.push_back(1.0f);
                    m_lineVertexScratch.push_back(x2); m_lineVertexScratch.push_back(h2); m_lineVertexScratch.push_back(0.0f); m_lineVertexScratch.push_back(0.0f); m_lineVertexScratch.push_back(1.0f);
                }
            }
        };
        if (style.mirrored) {
            appendCurveRange(0.0f, -effectiveBarWidth);
            appendCurveRange(0.0f, effectiveBarWidth);
        } else {
            appendCurveRange(-1.0f, effectiveBarWidth);
        }
    } else if (style.mirrored) {
        for (size_t i = 0; i < numBars; ++i) {
            const float h = std::min(magnitudes[i] * style.heightScale, 2.0f);
            addBar(-static_cast<float>(i) * effectiveBarWidth - effectiveBarWidth, h, effectiveBarWidth);
            addBar(static_cast<float>(i) * effectiveBarWidth, h, effectiveBarWidth);
        }
    } else {
        for (size_t i = 0; i < numBars; ++i) {
            const float x = -1.0f + i * effectiveBarWidth;
            const float h = std::min(magnitudes[i] * style.heightScale, 2.0f);
            addBar(x, h, effectiveBarWidth);
        }
    }

    pipeline.use();
    pipeline.setCornerRadius(style.cornerRadius);
    pipeline.setShape(static_cast<int>(style.shape));

    const float clampedLineWidth = std::clamp(style.traceWidth, 1.0f, style.maxLineWidth);
    if (style.shape == VisualizerShape::Curve) {
        if (!vertices.empty()) {
            const GLint fillFirst = pipeline.uploadVertices(vertices);
            pipeline.setColor(style.r, style.g, style.b, style.a * style.fillOpacity);
            glDrawArrays(GL_TRIANGLES, fillFirst, static_cast<GLsizei>(vertices.size() / 5));
        }
        if (!m_lineVertexScratch.empty()) {
            const GLint lineFirst = pipeline.uploadVertices(m_lineVertexScratch);
            glLineWidth(clampedLineWidth);
            pipeline.setColor(style.r, style.g, style.b, style.a);
            glDrawArrays(GL_LINES, lineFirst, static_cast<GLsizei>(m_lineVertexScratch.size() / 5));
        }
        return;
    }

    if (vertices.empty()) return;
    const GLint firstVertex = pipeline.uploadVertices(vertices);
    pipeline.setColor(style.r, style.g, style.b, style.a);
    if (style.shape == VisualizerShape::Lines) {
        glLineWidth(clampedLineWidth);
        glDrawArrays(GL_LINES, firstVertex, static_cast<GLsizei>(vertices.size() / 5));
    } else if (style.shape == VisualizerShape::Waveform) {
        glLineWidth(clampedLineWidth);
        glDrawArrays(GL_LINE_STRIP, firstVertex, static_cast<GLsizei>(vertices.size() / 5));
    } else {
        glDrawArrays(GL_TRIANGLES, firstVertex, static_cast<GLsizei>(vertices.size() / 5));
    }
}
