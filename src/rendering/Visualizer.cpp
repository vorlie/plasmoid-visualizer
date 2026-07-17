#include "Visualizer.hpp"
#include "AssetPaths.hpp"
#include "VisualizerGeometry.hpp"
#include <iostream>
#include <algorithm>
#include <cmath>

Visualizer::Visualizer() {
    initShaders();
    initQuad();
    
    const auto defaultFont = AssetPaths::defaultFont();
    if (!defaultFont.empty()) {
        loadFont(defaultFont.string());
    }
    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);

    glEnable(GL_LINE_SMOOTH);
    glEnable(GL_POLYGON_SMOOTH);
    glEnable(GL_MULTISAMPLE);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);
}

Visualizer::~Visualizer() {
    glDeleteVertexArrays(1, &m_vao);
    glDeleteBuffers(1, &m_vbo);
    glDeleteVertexArrays(1, &m_quadVAO);
    glDeleteBuffers(1, &m_quadVBO);
}

void Visualizer::initShaders() {
    m_shaderProgram.load(
        AssetPaths::shader("visualizer.vert"),
        AssetPaths::shader("visualizer.frag"));
    m_quadShaderProgram.load(
        AssetPaths::shader("quad.vert"),
        AssetPaths::shader("quad.frag"));
}

void Visualizer::setHeightScale(float scale) {
    m_heightScale = scale;
}

void Visualizer::setMirrored(bool mirrored) {
    m_mirrored = mirrored;
}

void Visualizer::setShape(VisualizerShape shape) {
    m_shape = shape;
}

void Visualizer::setColor(float r, float g, float b, float a) {
    m_r = r; m_g = g; m_b = b; m_a = a; // Store for glow effect
    glUseProgram(m_shaderProgram);
    GLint colorLoc = glGetUniformLocation(m_shaderProgram, "uColor");
    glUniform4f(colorLoc, r, g, b, a);
}

void Visualizer::setCornerRadius(float radius) {
    m_cornerRadius = radius;
}

void Visualizer::setViewportSize(int width, int height) {
    m_viewportWidth = width;
    m_viewportHeight = height;
}

void Visualizer::setRotation(float angleRadians) {
    m_rotationAngle = angleRadians;
}

void Visualizer::setFlip(bool flipX, bool flipY) {
    m_flipX = flipX;
    m_flipY = flipY;
}

void Visualizer::setBloomIntensity(float intensity) {
    m_bloomIntensity = intensity;
}

void Visualizer::setupPersistence(int width, int height) {
    setViewportSize(width, height);
    m_persistenceBuffer.resize(width, height);
}

void Visualizer::beginPersistence() {
    if (!m_persistenceBuffer.ready()) return;
    m_persistenceBuffer.bind();
    glViewport(0, 0, m_persistenceBuffer.width(), m_persistenceBuffer.height());
}

void Visualizer::endPersistence() {
    Framebuffer::unbind();
}

void Visualizer::drawPersistenceBuffer() {
    if (!m_persistenceBuffer.ready()) return;
    drawTexture(m_persistenceBuffer.texture());
}

void Visualizer::drawTexture(GLuint texture, float opacity) {
    if (texture == 0) return;
    glUseProgram(m_quadShaderProgram);
    glUniform1i(glGetUniformLocation(m_quadShaderProgram, "uUseTexture"), 1);
    glUniform1i(glGetUniformLocation(m_quadShaderProgram, "uIsFont"), 0);
    glUniform4f(glGetUniformLocation(m_quadShaderProgram, "uColor"), 1.0f, 1.0f, 1.0f, opacity);
    
    // Ensure scale/offset are reset for persistence buffer
    GLint sizeLoc = glGetUniformLocation(m_quadShaderProgram, "uSize");
    GLint offsetLoc = glGetUniformLocation(m_quadShaderProgram, "uOffset");
    if (sizeLoc != -1) glUniform2f(sizeLoc, 1.0f, 1.0f);
    if (offsetLoc != -1) glUniform2f(offsetLoc, 0.0f, 0.0f);
    glUniform1f(glGetUniformLocation(m_quadShaderProgram, "uRotation"), 0.0f);
    glUniform1f(glGetUniformLocation(m_quadShaderProgram, "uCornerRadius"), 0.0f);
    glUniform4f(glGetUniformLocation(m_quadShaderProgram, "uTexRect"), 0.0f, 0.0f, 1.0f, 1.0f);

    glBindVertexArray(m_quadVAO);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glUniform1i(glGetUniformLocation(m_quadShaderProgram, "uTexture"), 0);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glUseProgram(0);
}

bool Visualizer::loadBackground(const std::string& path) {
    return m_backgroundTexture.loadRgba(path, true);
}

void Visualizer::clearBackground() {
    m_backgroundTexture.reset();
}

void Visualizer::drawBackground(float scale, float shakeX, float shakeY, float rotation) {
    if (!m_backgroundTexture.loaded()) return;

    glUseProgram(m_quadShaderProgram);
    glUniform2f(glGetUniformLocation(m_quadShaderProgram, "uSize"), scale, scale);
    glUniform2f(glGetUniformLocation(m_quadShaderProgram, "uOffset"), shakeX, shakeY);
    glUniform1f(glGetUniformLocation(m_quadShaderProgram, "uRotation"), rotation);
    
    glBindVertexArray(m_quadVAO);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_backgroundTexture.id());
    glUniform1i(glGetUniformLocation(m_quadShaderProgram, "uTexture"), 0);
    glUniform1i(glGetUniformLocation(m_quadShaderProgram, "uUseTexture"), 1);
    glUniform4f(glGetUniformLocation(m_quadShaderProgram, "uColor"), 1, 1, 1, 1);

    glDrawArrays(GL_TRIANGLES, 0, 6);

    // Reset for subsequent passes
    glUniform2f(glGetUniformLocation(m_quadShaderProgram, "uSize"), 1.0f, 1.0f);
    glUniform2f(glGetUniformLocation(m_quadShaderProgram, "uOffset"), 0.0f, 0.0f);
    glUniform1f(glGetUniformLocation(m_quadShaderProgram, "uRotation"), 0.0f);

    glUseProgram(0);
}

void Visualizer::drawRoundedRect(float x, float y, float w, float h, float radius, const float color[4]) {
    glUseProgram(m_quadShaderProgram);
    
    glUniform2f(glGetUniformLocation(m_quadShaderProgram, "uSize"), w, h);
    glUniform2f(glGetUniformLocation(m_quadShaderProgram, "uOffset"), x, y);
    glUniform1f(glGetUniformLocation(m_quadShaderProgram, "uRotation"), 0.0f);
    
    glUniform1f(glGetUniformLocation(m_quadShaderProgram, "uCornerRadius"), radius);
    glUniform1f(glGetUniformLocation(m_quadShaderProgram, "uAspect"), w / h);
    glUniform1i(glGetUniformLocation(m_quadShaderProgram, "uUseTexture"), 0);
    glUniform1i(glGetUniformLocation(m_quadShaderProgram, "uIsFont"), 0);
    glUniform4f(glGetUniformLocation(m_quadShaderProgram, "uColor"), color[0], color[1], color[2], color[3]);
    glUniform4f(glGetUniformLocation(m_quadShaderProgram, "uTexRect"), 0, 0, 1, 1);
    
    glBindVertexArray(m_quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    
    glUniform1f(glGetUniformLocation(m_quadShaderProgram, "uCornerRadius"), 0.0f);
    glUseProgram(0);
}

bool Visualizer::loadFont(const std::string& path) {
    return m_fontAtlas.load(path, 48.0f);
}

void Visualizer::drawText(const std::string& text, float x, float y, float scale, const float color[4]) {
    if (!m_fontAtlas.loaded()) return;

    glUseProgram(m_quadShaderProgram);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_fontAtlas.texture());
    glUniform1i(glGetUniformLocation(m_quadShaderProgram, "uTexture"), 0);
    glUniform1i(glGetUniformLocation(m_quadShaderProgram, "uUseTexture"), 1);
    glUniform4f(glGetUniformLocation(m_quadShaderProgram, "uColor"), color[0], color[1], color[2], color[3]);
    glUniform1f(glGetUniformLocation(m_quadShaderProgram, "uCornerRadius"), 0.0f);
    glUniform1f(glGetUniformLocation(m_quadShaderProgram, "uRotation"), 0.0f);

    float curX = x;
    float aspect = (float)m_viewportWidth / (float)m_viewportHeight;

    for (char c : text) {
        if (c < 32 || c >= 128) continue;
        const GlyphInfo& ch = m_fontAtlas.glyph(static_cast<unsigned char>(c));

        float w = (ch.width / (float)m_viewportWidth) * scale;
        float h = (ch.height / (float)m_viewportHeight) * scale;
        float ox = (ch.bearingLeft / (float)m_viewportWidth) * scale;
        float oy = (-ch.bearingTop / (float)m_viewportHeight) * scale;

        // Adjust for quad.frag (which uses TexCoord for SDF/Texture mapping)
        // Actually, for font rendering, we need the frag shader to use the R channel as Alpha.
        // I should update quad.frag to handle alpha-only textures.
        
        glUniform2f(glGetUniformLocation(m_quadShaderProgram, "uSize"), w, h);
        glUniform2f(glGetUniformLocation(m_quadShaderProgram, "uOffset"), curX + ox + w, y - oy - h);
        
        // STB bakes top-down, GL is bottom-up. Flip TW.y? 
        // Let's use uTexRect to flip the mapping: [tx, ty, tw, -th]? No, that might discard.
        // Better: [tx, ty + th, tw, -th]
        float tw = ch.width / 1024.0f;
        float th = ch.height / 1024.0f;
        glUniform4f(glGetUniformLocation(m_quadShaderProgram, "uTexRect"), ch.textureX, ch.textureY + th, tw, -th);
        glUniform1i(glGetUniformLocation(m_quadShaderProgram, "uIsFont"), 1);

        glBindVertexArray(m_quadVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        curX += (ch.advanceX / (float)m_viewportWidth) * scale;
    }

    glUniform4f(glGetUniformLocation(m_quadShaderProgram, "uTexRect"), 0, 0, 1, 1);
    glUniform1i(glGetUniformLocation(m_quadShaderProgram, "uIsFont"), 0);
    glUseProgram(0);
}

void Visualizer::initQuad() {
    float quadVertices[] = {
        // positions   // texCoords
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,

        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f
    };
    glGenVertexArrays(1, &m_quadVAO);
    glGenBuffers(1, &m_quadVBO);
    glBindVertexArray(m_quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
}

void Visualizer::drawFullscreenDimmer(float decayRate) {
    glUseProgram(m_quadShaderProgram);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    GLint colorLoc = glGetUniformLocation(m_quadShaderProgram, "uColor");
    glUniform4f(colorLoc, 0.0f, 0.0f, 0.0f, decayRate);
    glUniform1i(glGetUniformLocation(m_quadShaderProgram, "uUseTexture"), 0); // Tell shader not to use texture

    GLint sizeLoc = glGetUniformLocation(m_quadShaderProgram, "uSize"); // FIXED: from uScale to uSize
    GLint offsetLoc = glGetUniformLocation(m_quadShaderProgram, "uOffset");
    if (sizeLoc != -1) glUniform2f(sizeLoc, 1.0f, 1.0f); // Make sure it covers the screen!
    if (offsetLoc != -1) glUniform2f(offsetLoc, 0.0f, 0.0f);

    glBindVertexArray(m_quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    
    glUniform1i(glGetUniformLocation(m_quadShaderProgram, "uUseTexture"), 1); // Reset
    glUseProgram(0);
}

void Visualizer::renderGrid() {
    if (!m_showGrid) return;

    float xScale = 1.0f;
    float yScale = 1.0f;
    float aspectRatio = (float)m_viewportWidth / (float)m_viewportHeight;
    if (aspectRatio > 1.0f) {
        xScale = 1.0f / aspectRatio;
    } else {
        yScale = aspectRatio;
    }

    glUseProgram(0);
    glLineWidth(1.0f);
    // Very subtle dark version of the color
    glColor4f(m_r * 0.15f, m_g * 0.15f, m_b * 0.15f, m_a * 0.4f);
    glBegin(GL_LINES);
    const int columns = std::max(m_gridColumns, 1);
    const int rows = std::max(m_gridRows, 1);
    for (int i = 0; i <= columns; ++i) {
        const float p = -1.0f + 2.0f * static_cast<float>(i) / columns;
        glVertex2f(p * xScale, -1.0f * yScale);
        glVertex2f(p * xScale, 1.0f * yScale);
    }
    for (int i = 0; i <= rows; ++i) {
        const float p = -1.0f + 2.0f * static_cast<float>(i) / rows;
        glVertex2f(-1.0f * xScale, p * yScale);
        glVertex2f(1.0f * xScale, p * yScale);
    }
    glEnd();
}

void Visualizer::render(const std::vector<float>& magnitudes) {
    if (magnitudes.empty()) return;

    std::vector<float> vertices;
    size_t numBars = magnitudes.size();

    // Aspect ratio correction (Square for XY)
    float xScale = 1.0f;
    float yScale = 1.0f;
    float aspectRatio = (float)m_viewportWidth / (float)m_viewportHeight;
    if (aspectRatio > 1.0f) xScale = 1.0f / aspectRatio;
    else yScale = aspectRatio;
    
    // Calculate bar width based on mode
    float effectiveBarWidth = m_mirrored ? (1.0f / numBars) : (2.0f / numBars);

    auto addBar = [&](float x, float h, float width) {
        // Helper to transform coordinates based on anchor
        auto transformCoord = [&](float localX, float localY, float& outX, float& outY) {
            switch (m_barAnchor) {
                case BarAnchor::Bottom:
                    // Default: bars grow upward from bottom
                    outX = localX;
                    outY = localY;
                    break;
                case BarAnchor::Top:
                    // Flip Y: bars grow down from top
                    outX = localX;
                    outY = -localY;
                    break;
                case BarAnchor::Left:
                    // Rotate 90° CCW: bars grow right from left edge
                    outX = localY;
                    outY = -localX;
                    break;
                case BarAnchor::Right:
                    // Rotate 90° CW: bars grow left from right edge
                    outX = -localY;
                    outY = localX;
                    break;
                case BarAnchor::Center:
                    // Mirror from center
                    outX = localX;
                    outY = localY - 1.0f; // Shift to center (height is already in [-1, 1] range)
                    break;
            }
        };
        
        if (m_shape == VisualizerShape::Bars) {
            // Define bar corners in local space
            float x0 = x, y0 = -1.0f;
            float x1 = x + width, y1 = -1.0f;
            float x2 = x, y2 = -1.0f + h;
            float x3 = x + width, y3 = -1.0f + h;
            
            // Transform to world space
            float tx0, ty0, tx1, ty1, tx2, ty2, tx3, ty3;
            transformCoord(x0, y0, tx0, ty0);
            transformCoord(x1, y1, tx1, ty1);
            transformCoord(x2, y2, tx2, ty2);
            transformCoord(x3, y3, tx3, ty3);
            
            // First triangle
            vertices.push_back(tx0); vertices.push_back(ty0); vertices.push_back(0.0f); vertices.push_back(0.0f); vertices.push_back(1.0f);
            vertices.push_back(tx1); vertices.push_back(ty1); vertices.push_back(1.0f); vertices.push_back(0.0f); vertices.push_back(1.0f);
            vertices.push_back(tx2); vertices.push_back(ty2); vertices.push_back(0.0f); vertices.push_back(1.0f); vertices.push_back(1.0f);
            // Second triangle
            vertices.push_back(tx1); vertices.push_back(ty1); vertices.push_back(1.0f); vertices.push_back(0.0f); vertices.push_back(1.0f);
            vertices.push_back(tx3); vertices.push_back(ty3); vertices.push_back(1.0f); vertices.push_back(1.0f); vertices.push_back(1.0f);
            vertices.push_back(tx2); vertices.push_back(ty2); vertices.push_back(0.0f); vertices.push_back(1.0f); vertices.push_back(1.0f);
        } else if (m_shape == VisualizerShape::Lines) {
            float lx0 = x + width/2, ly0 = -1.0f;
            float lx1 = x + width/2, ly1 = -1.0f + h;
            float tx0, ty0, tx1, ty1;
            transformCoord(lx0, ly0, tx0, ty0);
            transformCoord(lx1, ly1, tx1, ty1);
            vertices.push_back(tx0); vertices.push_back(ty0); vertices.push_back(0.5f); vertices.push_back(0.0f); vertices.push_back(1.0f);
            vertices.push_back(tx1); vertices.push_back(ty1); vertices.push_back(0.5f); vertices.push_back(1.0f); vertices.push_back(1.0f);
        } else if (m_shape == VisualizerShape::Dots) {
            float dotSize = width * 0.8f;
            float centerX = x + width/2;
            float centerY = -1.0f + h;
            
            // Transform center
            float tcx, tcy;
            transformCoord(centerX, centerY, tcx, tcy);
            
            // For dots, we need to rotate the dot quad as well
            // Simplification: use dotSize in both dimensions
            float dx = dotSize/2;
            float dy = dotSize/2;
            
            // Apply rotation for Left/Right anchors
            if (m_barAnchor == BarAnchor::Left || m_barAnchor == BarAnchor::Right) {
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

    const bool isXY = m_shape == VisualizerShape::OscilloscopeXY ||
                      m_shape == VisualizerShape::OscilloscopeXY_Clean;
    const size_t xyPointCount = isXY ? magnitudes.size() / 2 : 0;
    float meanX = 0.0f;
    float meanY = 0.0f;
    float xyGain = 1.0f;
    if (xyPointCount > 0 && m_xyAutoGain) {
        for (size_t i = 0; i < xyPointCount; ++i) {
            meanX += magnitudes[i * 2];
            meanY += magnitudes[i * 2 + 1];
        }
        meanX /= static_cast<float>(xyPointCount);
        meanY /= static_cast<float>(xyPointCount);

        float peak = 0.0f;
        for (size_t i = 0; i < xyPointCount; ++i) {
            peak = std::max(peak, std::abs(magnitudes[i * 2] - meanX));
            peak = std::max(peak, std::abs(magnitudes[i * 2 + 1] - meanY));
        }
        if (peak > 1e-4f) xyGain = std::min(0.9f / peak, 8.0f);
    }

    const float rotationCos = std::cos(m_rotationAngle);
    const float rotationSin = std::sin(m_rotationAngle);
    auto getXY = [&](size_t idx, float& x, float& y) {
        x = (magnitudes[idx * 2] - meanX) * xyGain * xScale * m_scaleX;
        y = (magnitudes[idx * 2 + 1] - meanY) * xyGain * yScale * m_scaleY;

        const float rotatedX = x * rotationCos - y * rotationSin;
        const float rotatedY = x * rotationSin + y * rotationCos;
        x = m_flipX ? -rotatedX : rotatedX;
        y = m_flipY ? -rotatedY : rotatedY;

        // Translation is screen-relative and should not rotate with the trace.
        x += m_offsetX;
        y += m_offsetY;
    };

    if (m_shape == VisualizerShape::Waveform) {
        size_t step = std::max((size_t)1, numBars / 2048);
        for (size_t i = 0; i < numBars; i += step) {
             float x = -1.0f + 2.0f * (float)i / (float)numBars;
             float y = magnitudes[i] * m_heightScale;
             vertices.push_back(x); vertices.push_back(y); vertices.push_back(0.0f); vertices.push_back(0.0f); vertices.push_back(1.0f);
        }
    } else if (m_shape == VisualizerShape::OscilloscopeXY) {
        float prevX = 0, prevY = 0;
        for (size_t i = 0; i < xyPointCount; ++i) {
            float x, y;
            getXY(i, x, y);

            float intensity = 1.0f;
            if (m_velocityModulation > 0.01f && i > 0) {
                const float dist = std::hypot(x - prevX, y - prevY);
                const float dwell = 1.0f / (1.0f + dist * 24.0f);
                intensity = std::max(
                    0.25f,
                    1.0f + (dwell - 1.0f) * m_velocityModulation);
            }

            vertices.push_back(x); vertices.push_back(y); 
            vertices.push_back(0.0f); vertices.push_back(0.0f); 
            vertices.push_back(intensity);

            prevX = x; prevY = y;
        }
    } else if (m_shape == VisualizerShape::OscilloscopeXY_Clean) {
        if (xyPointCount < 2) return;
        float prevX = 0, prevY = 0;
        size_t numPoints = xyPointCount;
        int segmentsPerPoint = 4; // Smoothness factor

        for (size_t i = 0; i < numPoints - 1; ++i) {
             float p0x, p0y, p1x, p1y, p2x, p2y, p3x, p3y;
             getXY(i > 0 ? i - 1 : 0, p0x, p0y);
             getXY(i, p1x, p1y);
             getXY(i + 1, p2x, p2y);
             getXY(i + 2 < numPoints ? i + 2 : i + 1, p3x, p3y);

             for (int s = 0; s < segmentsPerPoint; ++s) {
                 float t = (float)s / (float)segmentsPerPoint;
                 float x = VisualizerGeometry::catmullRom(p0x, p1x, p2x, p3x, t);
                 float y = VisualizerGeometry::catmullRom(p0y, p1y, p2y, p3y, t);
                 
                 float intensity = 1.0f;
                 if (m_velocityModulation > 0.01f && (i > 0 || s > 0)) {
                     const float dist = std::hypot(x - prevX, y - prevY) * segmentsPerPoint;
                     const float dwell = 1.0f / (1.0f + dist * 24.0f);
                     intensity = std::max(
                         0.25f,
                         1.0f + (dwell - 1.0f) * m_velocityModulation);
                 }
                 
                 vertices.push_back(x); vertices.push_back(y); 
                 vertices.push_back(0.0f); vertices.push_back(0.0f); 
                 vertices.push_back(intensity);
                 
                 prevX = x; prevY = y;
             }
        }
    } else if (m_mirrored) {
        for (size_t i = 0; i < numBars; ++i) {
            float h = std::min(magnitudes[i] * m_heightScale, 2.0f);
            // Left side (reversed)
            addBar(-(float)i * effectiveBarWidth - effectiveBarWidth, h, effectiveBarWidth);
            // Right side
            addBar((float)i * effectiveBarWidth, h, effectiveBarWidth);
        }
    } else if (m_shape == VisualizerShape::Curve) {
        // Spline-based smooth curve
        int segmentsPerBar = 8;
        for (size_t i = 0; i < numBars - 1; ++i) {
            float p0 = std::min((i > 0 ? magnitudes[i - 1] : magnitudes[i]) * m_heightScale, 2.0f) - 1.0f;
            float p1 = std::min(magnitudes[i] * m_heightScale, 2.0f) - 1.0f;
            float p2 = std::min(magnitudes[i + 1] * m_heightScale, 2.0f) - 1.0f;
            float p3 = std::min((i + 2 < numBars ? magnitudes[i + 2] : magnitudes[i + 1]) * m_heightScale, 2.0f) - 1.0f;

            float xStart = -1.0f + i * effectiveBarWidth;
            float xEnd = -1.0f + (i + 1) * effectiveBarWidth;

            for (int s = 0; s < segmentsPerBar; ++s) {
                float t1 = (float)s / segmentsPerBar;
                float t2 = (float)(s + 1) / segmentsPerBar;

                float h1 = VisualizerGeometry::catmullRom(p0, p1, p2, p3, t1);
                float h2 = VisualizerGeometry::catmullRom(p0, p1, p2, p3, t2);

                float x1 = xStart + (xEnd - xStart) * t1;
                float x2 = xStart + (xEnd - xStart) * t2;

                // Area fill triangles (lower opacity handled by uColor alpha)
                vertices.push_back(x1); vertices.push_back(-1.0f); vertices.push_back(0.0f); vertices.push_back(0.0f); vertices.push_back(1.0f);
                vertices.push_back(x2); vertices.push_back(-1.0f); vertices.push_back(0.0f); vertices.push_back(0.0f); vertices.push_back(1.0f);
                vertices.push_back(x1); vertices.push_back(h1);    vertices.push_back(0.0f); vertices.push_back(0.0f); vertices.push_back(1.0f);

                vertices.push_back(x2); vertices.push_back(-1.0f); vertices.push_back(0.0f); vertices.push_back(0.0f); vertices.push_back(1.0f);
                vertices.push_back(x2); vertices.push_back(h2);    vertices.push_back(0.0f); vertices.push_back(0.0f); vertices.push_back(1.0f);
                vertices.push_back(x1); vertices.push_back(h1);    vertices.push_back(0.0f); vertices.push_back(0.0f); vertices.push_back(1.0f);
            }
        }
    } else {
        for (size_t i = 0; i < numBars; ++i) {
            float x = -1.0f + i * effectiveBarWidth;
            float h = std::min(magnitudes[i] * m_heightScale, 2.0f);
            addBar(x, h, effectiveBarWidth);
        }
    }

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STREAM_DRAW);

    // stride is now 5 floats
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(4 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glUseProgram(m_shaderProgram);
    glUniform1f(glGetUniformLocation(m_shaderProgram, "uCornerRadius"), m_cornerRadius);
    glUniform1i(glGetUniformLocation(m_shaderProgram, "uShape"), (int)m_shape);

    if (m_shape == VisualizerShape::Lines) {
        glLineWidth(m_traceWidth);
        glDrawArrays(GL_LINES, 0, (GLsizei)(vertices.size() / 5));
    } else if (m_shape == VisualizerShape::Waveform) {
        glLineWidth(m_traceWidth);
        glDrawArrays(GL_LINE_STRIP, 0, (GLsizei)(vertices.size() / 5));
    } else if (m_shape == VisualizerShape::OscilloscopeXY || m_shape == VisualizerShape::OscilloscopeXY_Clean) {
        // Screen-space ribbons provide consistent widths on drivers that only support 1px lines.
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        GLsizei vertexCount = (GLsizei)(vertices.size() / 5);
        GLint colorLoc = glGetUniformLocation(m_shaderProgram, "uColor");

        auto drawTracePass = [&](float width, float alphaMul, float whiteMix, float emission) {
            std::vector<float> ribbon = VisualizerGeometry::buildTraceRibbon(
                vertices, width, m_viewportWidth, m_viewportHeight);
            if (ribbon.empty()) return;
            glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
            glBufferData(GL_ARRAY_BUFFER, ribbon.size() * sizeof(float), ribbon.data(), GL_STREAM_DRAW);
            const float passR = m_r + (1.0f - m_r) * whiteMix;
            const float passG = m_g + (1.0f - m_g) * whiteMix;
            const float passB = m_b + (1.0f - m_b) * whiteMix;
            glUniform4f(
                colorLoc,
                passR * emission,
                passG * emission,
                passB * emission,
                m_a * alphaMul);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, (GLsizei)(ribbon.size() / 5));
        };

        // Draw only the physical emissive beam here. The wide halo is now
        // generated by BloomRenderer from values above the HDR threshold.
        const float bloom = std::clamp(m_bloomIntensity, 0.0f, 5.0f);
        drawTracePass(
            m_traceWidth + 1.5f, 0.35f, 0.0f,
            0.55f + bloom * 0.2f);
        drawTracePass(
            m_traceWidth, 0.95f, 0.25f,
            0.8f + bloom * 0.65f);

        // Restore the centerline for the optional circular beam head.
        if (m_beamHeadSize > 0.01f && vertexCount > 0) {
            glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
            glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STREAM_DRAW);
            glUniform1i(glGetUniformLocation(m_shaderProgram, "uShape"), 10); // Circular Point mode
            
            glPointSize(m_beamHeadSize);
            const float headEmission = 0.8f + bloom * 0.65f;
            glUniform4f(
                colorLoc,
                headEmission,
                headEmission,
                headEmission,
                m_a);
            glDrawArrays(GL_POINTS, vertexCount - 1, 1);

            glUniform1i(glGetUniformLocation(m_shaderProgram, "uShape"), (int)m_shape); // Restore
        }

        // Restore standard blending for other layers
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    } else if (m_shape == VisualizerShape::Curve) {
        GLint colorLoc = glGetUniformLocation(m_shaderProgram, "uColor");
        
        // Pass 1: Draw filled area with lower opacity
        glUniform4f(colorLoc, m_r, m_g, m_b, m_a * m_fillOpacity);
        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(vertices.size() / 5));
        
        // Pass 2: Draw the smooth line on top (full opacity)
        std::vector<float> lineVertices;
        for (size_t i = 0; i < vertices.size(); i += 30) { // 6 vertices per segment, each with 5 floats = 30
            // Point 1 on curve (Vertex 2 in triangle array)
            lineVertices.push_back(vertices[i + 10]); 
            lineVertices.push_back(vertices[i + 11]); 
            lineVertices.push_back(0.0f); 
            lineVertices.push_back(0.0f);
            lineVertices.push_back(1.0f);

            // Point 2 on curve (Vertex 4 in triangle array)
            lineVertices.push_back(vertices[i + 20]); 
            lineVertices.push_back(vertices[i + 21]); 
            lineVertices.push_back(0.0f); 
            lineVertices.push_back(0.0f);
            lineVertices.push_back(1.0f);
        }
        
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        glBufferData(GL_ARRAY_BUFFER, lineVertices.size() * sizeof(float), lineVertices.data(), GL_STREAM_DRAW);
        
        glLineWidth(m_traceWidth);
        glUniform4f(colorLoc, m_r, m_g, m_b, m_a);
        glDrawArrays(GL_LINES, 0, (GLsizei)(lineVertices.size() / 5));
    } else {
        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(vertices.size() / 5));
    }
}

void Visualizer::renderXY(const XYTraceBatch& trace) {
    if (trace.points.empty()) return;

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), reinterpret_cast<void*>(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 5 * sizeof(float), reinterpret_cast<void*>(4 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glUseProgram(m_shaderProgram);
    glUniform1i(glGetUniformLocation(m_shaderProgram, "uShape"), static_cast<int>(VisualizerShape::OscilloscopeXY));
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    const GLint colorLocation = glGetUniformLocation(m_shaderProgram, "uColor");
    const float bloom = std::clamp(m_bloomIntensity, 0.0f, 5.0f);

    auto drawSegment = [&](const std::vector<float>& centerline) {
        if (centerline.size() < 10) return;
        auto drawPass = [&](float width, float alpha, float whiteMix, float emission) {
            auto ribbon = VisualizerGeometry::buildTraceRibbon(
                centerline, width, m_viewportWidth, m_viewportHeight);
            if (ribbon.empty()) return;
            glBufferData(GL_ARRAY_BUFFER, ribbon.size() * sizeof(float), ribbon.data(), GL_STREAM_DRAW);
            glUniform4f(
                colorLocation,
                (m_r + (1.0f - m_r) * whiteMix) * emission,
                (m_g + (1.0f - m_g) * whiteMix) * emission,
                (m_b + (1.0f - m_b) * whiteMix) * emission,
                m_a * alpha);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, static_cast<GLsizei>(ribbon.size() / 5));
        };
        drawPass(m_traceWidth + 1.5f, 0.35f, 0.0f, 0.55f + bloom * 0.2f);
        drawPass(m_traceWidth, 0.95f, 0.25f, 0.8f + bloom * 0.65f);
    };

    // XY coordinates are expressed in scope units, not stretched NDC units.
    // Compensate X for the framebuffer aspect so circles remain circles.
    const float xAspectCompensation = m_viewportWidth > 0
        ? static_cast<float>(m_viewportHeight) / static_cast<float>(m_viewportWidth)
        : 1.0f;
    std::vector<float> centerline;
    for (const auto& point : trace.points) {
        if (point.breakBefore) {
            drawSegment(centerline);
            centerline.clear();
        }
        centerline.insert(centerline.end(), {point.x * xAspectCompensation, point.y, 0.0f, 0.0f, point.intensity});
    }
    drawSegment(centerline);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}
