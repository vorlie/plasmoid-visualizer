#include "Visualizer.hpp"
#include <iostream>
#include <algorithm>

const char* vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aLocalPos; // Local pos within the bar [0,1]
out vec2 LocalPos;
void main() {
    gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0);
    LocalPos = aLocalPos;
}
)";

const char* fragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;
in vec2 LocalPos;
uniform vec4 uColor;
uniform float uCornerRadius;
uniform int uShape; // 0: Bars, 1: Lines, 2: Dots

float roundedBoxSDF(vec2 p, vec2 b, float r) {
    vec2 q = abs(p) - b + r;
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}

void main() {
    if (uShape == 0) { // Bars
        if (uCornerRadius > 0.001) {
            // Map LocalPos [0,1] to [-1,1] for SDF
            vec2 p = LocalPos * 2.0 - 1.0;
            float dist = roundedBoxSDF(p, vec2(1.0), uCornerRadius);
            if (dist > 0.0) discard;
        }
    } else if (uShape == 2) { // Dots
        vec2 p = LocalPos * 2.0 - 1.0;
        if (length(p) > 1.0) discard;
    }
    FragColor = uColor;
}
)";

Visualizer::Visualizer() {
    initShaders();
    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
}

Visualizer::~Visualizer() {
    glDeleteVertexArrays(1, &m_vao);
    glDeleteBuffers(1, &m_vbo);
    glDeleteProgram(m_shaderProgram);
}

void Visualizer::initShaders() {
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

void Visualizer::render(const std::vector<float>& magnitudes) {
    if (magnitudes.empty()) return;

    std::vector<float> vertices;
    size_t numBars = magnitudes.size();
    
    // Calculate bar width based on mode
    float effectiveBarWidth = m_mirrored ? (1.0f / numBars) : (2.0f / numBars);

    auto addBar = [&](float x, float h, float width) {
        if (m_shape == VisualizerShape::Bars) {
            // First triangle
            vertices.push_back(x);         vertices.push_back(-1.0f);     vertices.push_back(0.0f); vertices.push_back(0.0f);
            vertices.push_back(x + width); vertices.push_back(-1.0f);     vertices.push_back(1.0f); vertices.push_back(0.0f);
            vertices.push_back(x);         vertices.push_back(-1.0f + h); vertices.push_back(0.0f); vertices.push_back(1.0f);
            // Second triangle
            vertices.push_back(x + width); vertices.push_back(-1.0f);     vertices.push_back(1.0f); vertices.push_back(0.0f);
            vertices.push_back(x + width); vertices.push_back(-1.0f + h); vertices.push_back(1.0f); vertices.push_back(1.0f);
            vertices.push_back(x);         vertices.push_back(-1.0f + h); vertices.push_back(0.0f); vertices.push_back(1.0f);
        } else if (m_shape == VisualizerShape::Lines) {
            vertices.push_back(x + width/2); vertices.push_back(-1.0f);     vertices.push_back(0.5f); vertices.push_back(0.0f);
            vertices.push_back(x + width/2); vertices.push_back(-1.0f + h); vertices.push_back(0.5f); vertices.push_back(1.0f);
        } else if (m_shape == VisualizerShape::Dots) {
            float dotSize = width * 0.8f;
            float centerX = x + width/2;
            float centerY = -1.0f + h;
            // Square for the dot (fragment shader will clip it to circle)
            vertices.push_back(centerX - dotSize/2); vertices.push_back(centerY - dotSize/2); vertices.push_back(0.0f); vertices.push_back(0.0f);
            vertices.push_back(centerX + dotSize/2); vertices.push_back(centerY - dotSize/2); vertices.push_back(1.0f); vertices.push_back(0.0f);
            vertices.push_back(centerX - dotSize/2); vertices.push_back(centerY + dotSize/2); vertices.push_back(0.0f); vertices.push_back(1.0f);
            
            vertices.push_back(centerX + dotSize/2); vertices.push_back(centerY - dotSize/2); vertices.push_back(1.0f); vertices.push_back(0.0f);
            vertices.push_back(centerX + dotSize/2); vertices.push_back(centerY + dotSize/2); vertices.push_back(1.0f); vertices.push_back(1.0f);
            vertices.push_back(centerX - dotSize/2); vertices.push_back(centerY + dotSize/2); vertices.push_back(0.0f); vertices.push_back(1.0f);
        }
    };

    if (m_shape == VisualizerShape::Waveform) {
        // ... (existing waveform logic) ...
        size_t step = std::max((size_t)1, numBars / 2048);
        for (size_t i = 0; i < numBars; i += step) {
             // ...
             float x = -1.0f + 2.0f * (float)i / (float)numBars;
             float y = magnitudes[i] * m_heightScale;
             // ...
             vertices.push_back(x); vertices.push_back(y); vertices.push_back(0.0f); vertices.push_back(0.0f);
        }
    } else if (m_shape == VisualizerShape::OscilloscopeXY) {
        // XY Mode with square aspect ratio and CRT glow
        size_t numSamples = magnitudes.size() / 2;
        size_t step = std::max((size_t)1, numSamples / 2048);

        // Calculate aspect ratio correction for square display
        float aspectRatio = (float)m_viewportWidth / (float)m_viewportHeight;
        float xScale = 1.0f;
        float yScale = 1.0f;
        
        if (aspectRatio > 1.0f) {
            // Width > Height: compress X
            xScale = 1.0f / aspectRatio;
        } else {
            // Height > Width: compress Y
            yScale = aspectRatio;
        }

        for (size_t i = 0; i < numSamples; i += step) {
            float l = magnitudes[i * 2];
            float r = magnitudes[i * 2 + 1];
            
            // Apply gain
            float x = l * m_heightScale;
            float y = r * m_heightScale;

            // Apply aspect correction for square
            x *= xScale;
            y *= yScale;

            // Clamp
            x = std::clamp(x, -1.0f, 1.0f);
            y = std::clamp(y, -1.0f, 1.0f);

            vertices.push_back(x); vertices.push_back(y); vertices.push_back(0.0f); vertices.push_back(0.0f);
        }
    } else if (m_mirrored) {
        for (size_t i = 0; i < numBars; ++i) {
            float h = std::min(magnitudes[i] * m_heightScale, 2.0f);
            // Left side (reversed)
            addBar(-(float)i * effectiveBarWidth - effectiveBarWidth, h, effectiveBarWidth);
            // Right side
            addBar((float)i * effectiveBarWidth, h, effectiveBarWidth);
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

    // Position attribute
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // LocalPos attribute (unused for waveform but kept for compatibility layout)
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glUseProgram(m_shaderProgram);
    glUniform1f(glGetUniformLocation(m_shaderProgram, "uCornerRadius"), m_cornerRadius);
    glUniform1i(glGetUniformLocation(m_shaderProgram, "uShape"), (int)m_shape);

    if (m_shape == VisualizerShape::Lines) {
        glLineWidth(2.0f);
        glDrawArrays(GL_LINES, 0, (GLsizei)(vertices.size() / 4));
    } else if (m_shape == VisualizerShape::Waveform) {
        glLineWidth(2.0f);
        glDrawArrays(GL_LINE_STRIP, 0, (GLsizei)(vertices.size() / 4));
    } else if (m_shape == VisualizerShape::OscilloscopeXY) {
        // CRT Glow Effect: Multi-pass with decreasing thickness and alpha
        GLsizei vertexCount = (GLsizei)(vertices.size() / 4);
        GLint colorLoc = glGetUniformLocation(m_shaderProgram, "uColor");
        
        // Pass 1: Wide glow halo (outer)
        glLineWidth(6.0f);
        glUniform4f(colorLoc, m_r, m_g, m_b, m_a * 0.15f);
        glDrawArrays(GL_LINE_STRIP, 0, vertexCount);
        
        // Pass 2: Medium glow
        glLineWidth(4.0f);
        glUniform4f(colorLoc, m_r, m_g, m_b, m_a * 0.35f);
        glDrawArrays(GL_LINE_STRIP, 0, vertexCount);
        
        // Pass 3: Core line (bright)
        glLineWidth(2.0f);
        glUniform4f(colorLoc, m_r, m_g, m_b, m_a);
        glDrawArrays(GL_LINE_STRIP, 0, vertexCount);
    } else {
        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(vertices.size() / 4));
    }
}
