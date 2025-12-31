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

void Visualizer::setColor(float r, float g, float b, float a) {
    glUseProgram(m_shaderProgram);
    GLint colorLoc = glGetUniformLocation(m_shaderProgram, "uColor");
    glUniform4f(colorLoc, r, g, b, a);
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
        // Waveform: Draw a continuous line strip centered on Y=0
        // Input 'magnitudes' is expected to be raw audio samples [-1, 1]
        // We might want to downsample if there are too many points
        size_t step = std::max((size_t)1, numBars / 2048); // Limit to ~2000 points
        
        for (size_t i = 0; i < numBars; i += step) {
            float x = -1.0f + 2.0f * (float)i / (float)numBars;
            float y = magnitudes[i] * m_heightScale;
            // Clamp to screen
            if (y > 1.0f) y = 1.0f;
            if (y < -1.0f) y = -1.0f;
            
            // Vertex
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
    } else {
        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(vertices.size() / 4));
    }
}
