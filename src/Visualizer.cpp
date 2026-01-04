#include "Visualizer.hpp"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>

static std::string loadShaderSource(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) {
        std::cerr << "Failed to open shader file: " << path << std::endl;
        return std::string();
    }
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

Visualizer::Visualizer() {
    initShaders();
    initQuad();
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
    glDeleteProgram(m_shaderProgram);
}

void Visualizer::initShaders() {
    // Load shader sources from files
    std::string vertSrc = loadShaderSource("shaders/visualizer.vert");
    std::string fragSrc = loadShaderSource("shaders/visualizer.frag");

    const char* vertexShaderSource = vertSrc.c_str();
    const char* fragmentShaderSource = fragSrc.c_str();

    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);

    m_shaderProgram = glCreateProgram();
    glAttachShader(m_shaderProgram, vertexShader);
    glAttachShader(m_shaderProgram, fragmentShader);
    glLinkProgram(m_shaderProgram);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // Quad Shader
    std::string qvsSrc = loadShaderSource("shaders/quad.vert");
    std::string qfsSrc = loadShaderSource("shaders/quad.frag");
    const char* qvs = qvsSrc.c_str();
    const char* qfs = qfsSrc.c_str();

    GLuint qvsId = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(qvsId, 1, &qvs, NULL);
    glCompileShader(qvsId);
    GLuint qfsId = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(qfsId, 1, &qfs, NULL);
    glCompileShader(qfsId);
    m_quadShaderProgram = glCreateProgram();
    glAttachShader(m_quadShaderProgram, qvsId);
    glAttachShader(m_quadShaderProgram, qfsId);
    glLinkProgram(m_quadShaderProgram);
    glDeleteShader(qvsId);
    glDeleteShader(qfsId);
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
    if (m_fboWidth == width && m_fboHeight == height) return;
    
    if (m_fbo) {
        glDeleteFramebuffers(1, &m_fbo);
        glDeleteTextures(1, &m_fboTexture);
    }
    
    m_fboWidth = width;
    m_fboHeight = height;
    
    glGenFramebuffers(1, &m_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    
    glGenTextures(1, &m_fboTexture);
    glBindTexture(GL_TEXTURE_2D, m_fboTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_fboTexture, 0);
    
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cerr << "ERROR::FRAMEBUFFER:: Framebuffer is not complete!" << std::endl;
        
    // Clear the new FBO to black initially
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Visualizer::beginPersistence() {
    if (!m_fbo) return;
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glViewport(0, 0, m_fboWidth, m_fboHeight);
}

void Visualizer::endPersistence() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Visualizer::drawPersistenceBuffer() {
    if (!m_fbo) return;
    glUseProgram(m_quadShaderProgram);
    glUniform1i(glGetUniformLocation(m_quadShaderProgram, "uUseTexture"), 1); 
    glBindVertexArray(m_quadVAO);
    glBindTexture(GL_TEXTURE_2D, m_fboTexture);
    glDrawArrays(GL_TRIANGLES, 0, 6);
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
    for (int i = -5; i <= 5; ++i) {
        float p = i * 0.2f;
        // Vertical
        glVertex2f(p * xScale, -1.0f * yScale);
        glVertex2f(p * xScale, 1.0f * yScale);
        // Horizontal
        glVertex2f(-1.0f * xScale, p * yScale);
        glVertex2f(1.0f * xScale, p * yScale);
    }
    glEnd();
}

static float catmullRom(float p0, float p1, float p2, float p3, float t) {
    return 0.5f * (
        (2.0f * p1) +
        (-p0 + p2) * t +
        (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t * t +
        (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t * t * t
    );
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
        if (m_shape == VisualizerShape::Bars) {
            // First triangle
            vertices.push_back(x);         vertices.push_back(-1.0f);     vertices.push_back(0.0f); vertices.push_back(0.0f); vertices.push_back(1.0f);
            vertices.push_back(x + width); vertices.push_back(-1.0f);     vertices.push_back(1.0f); vertices.push_back(0.0f); vertices.push_back(1.0f);
            vertices.push_back(x);         vertices.push_back(-1.0f + h); vertices.push_back(0.0f); vertices.push_back(1.0f); vertices.push_back(1.0f);
            // Second triangle
            vertices.push_back(x + width); vertices.push_back(-1.0f);     vertices.push_back(1.0f); vertices.push_back(0.0f); vertices.push_back(1.0f);
            vertices.push_back(x + width); vertices.push_back(-1.0f + h); vertices.push_back(1.0f); vertices.push_back(1.0f); vertices.push_back(1.0f);
            vertices.push_back(x);         vertices.push_back(-1.0f + h); vertices.push_back(0.0f); vertices.push_back(1.0f); vertices.push_back(1.0f);
        } else if (m_shape == VisualizerShape::Lines) {
            vertices.push_back(x + width/2); vertices.push_back(-1.0f);     vertices.push_back(0.5f); vertices.push_back(0.0f); vertices.push_back(1.0f);
            vertices.push_back(x + width/2); vertices.push_back(-1.0f + h); vertices.push_back(0.5f); vertices.push_back(1.0f); vertices.push_back(1.0f);
        } else if (m_shape == VisualizerShape::Dots) {
            float dotSize = width * 0.8f;
            float centerX = x + width/2;
            float centerY = -1.0f + h;
            // Square for the dot (fragment shader will clip it to circle)
            vertices.push_back(centerX - dotSize/2); vertices.push_back(centerY - dotSize/2); vertices.push_back(0.0f); vertices.push_back(0.0f); vertices.push_back(1.0f);
            vertices.push_back(centerX + dotSize/2); vertices.push_back(centerY - dotSize/2); vertices.push_back(1.0f); vertices.push_back(0.0f); vertices.push_back(1.0f);
            vertices.push_back(centerX - dotSize/2); vertices.push_back(centerY + dotSize/2); vertices.push_back(0.0f); vertices.push_back(1.0f); vertices.push_back(1.0f);
            
            vertices.push_back(centerX + dotSize/2); vertices.push_back(centerY - dotSize/2); vertices.push_back(1.0f); vertices.push_back(0.0f); vertices.push_back(1.0f);
            vertices.push_back(centerX + dotSize/2); vertices.push_back(centerY + dotSize/2); vertices.push_back(1.0f); vertices.push_back(1.0f); vertices.push_back(1.0f);
            vertices.push_back(centerX - dotSize/2); vertices.push_back(centerY + dotSize/2); vertices.push_back(0.0f); vertices.push_back(1.0f); vertices.push_back(1.0f);
        }
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
        for (size_t i = 0; i < magnitudes.size() / 2; ++i) {
            float x = magnitudes[i * 2] * xScale;
            float y = magnitudes[i * 2 + 1] * yScale;

            // Apply rotation
            float rx = x * cos(m_rotationAngle) - y * sin(m_rotationAngle);
            float ry = x * sin(m_rotationAngle) + y * cos(m_rotationAngle);
            x = rx; y = ry;

            if (m_flipX) x = -x;
            if (m_flipY) y = -y;

            x = std::clamp(x, -1.0f, 1.0f);
            y = std::clamp(y, -1.0f, 1.0f);

            float intensity = 1.0f;
            if (m_velocityModulation > 0.01f && i > 0) {
                float dist = sqrt(pow(x - prevX, 2) + pow(y - prevY, 2));
                intensity = 1.0f / (1.0f + dist * m_velocityModulation * 50.0f);
            }

            vertices.push_back(x); vertices.push_back(y); 
            vertices.push_back(0.0f); vertices.push_back(0.0f); 
            vertices.push_back(intensity);

            prevX = x; prevY = y;
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

                float h1 = catmullRom(p0, p1, p2, p3, t1);
                float h2 = catmullRom(p0, p1, p2, p3, t2);

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
    } else if (m_shape == VisualizerShape::OscilloscopeXY) {
        // CRT Glow Effect: Multi-pass with additive blending
        glUseProgram(m_shaderProgram);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE); // Additive blending for "glow" overlap
        
        glLineWidth(m_traceWidth);
        GLsizei vertexCount = (GLsizei)(vertices.size() / 5);
        GLint colorLoc = glGetUniformLocation(m_shaderProgram, "uColor");
        
        // Helper lambda to draw a glow pass and round caps at vertices to avoid corner cuts
        auto drawGlowPass = [&](float lineW, float alphaMul) {
            glLineWidth(lineW);
            glUniform4f(colorLoc, m_r, m_g, m_b, m_a * alphaMul * m_bloomIntensity);
            glDrawArrays(GL_LINE_STRIP, 0, vertexCount);

            // Draw round caps at each vertex to smooth joins where thick lines can produce miter/bevel artifacts
            GLint shapeLoc = glGetUniformLocation(m_shaderProgram, "uShape");
            glUniform1i(shapeLoc, 10);
            glPointSize(lineW);
            glDrawArrays(GL_POINTS, 0, vertexCount);
            // Restore shape uniform
            glUniform1i(shapeLoc, (int)m_shape);
        };
        
        // Pass 1: Massive outer halo (very faint)
        drawGlowPass(20.0f, 0.05f);

        // Pass 2: Wide glow halo
        drawGlowPass(10.0f, 0.15f);
        
        // Pass 3: Medium glow
        drawGlowPass(4.0f, 0.4f);
        
        // Pass 4: Core line (bright)
        glLineWidth(2.0f);
        glUniform4f(colorLoc, m_r, m_g, m_b, m_a * m_bloomIntensity);
        glDrawArrays(GL_LINE_STRIP, 0, vertexCount);
        glPointSize(2.0f);
        glDrawArrays(GL_POINTS, 0, vertexCount);

        // Pass 5: Beam Head (Single point at the very end of the trace)
        if (m_beamHeadSize > 0.01f && vertexCount > 0) {
            glUniform1i(glGetUniformLocation(m_shaderProgram, "uShape"), 10); // Circular Point mode
            
            glPointSize(m_beamHeadSize);
            glUniform4f(colorLoc, 1.0f, 1.0f, 1.0f, m_a * m_bloomIntensity); // White core for head
            glDrawArrays(GL_POINTS, vertexCount - 1, 1);
            
            // Faint glowing halo around head
            glPointSize(m_beamHeadSize * 3.5f);
            glUniform4f(colorLoc, m_r, m_g, m_b, m_a * 0.6f * m_bloomIntensity);
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
