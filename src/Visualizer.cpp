#include "Visualizer.hpp"
#include <iostream>

const char* vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec2 aPos;
void main() {
    gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0);
}
)";

const char* fragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;
uniform vec4 uColor;
void main() {
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

void Visualizer::render(const std::vector<float>& magnitudes) {
    if (magnitudes.empty()) return;

    std::vector<float> vertices;
    float width = 2.0f / magnitudes.size();
    for (size_t i = 0; i < magnitudes.size(); ++i) {
        float x = -1.0f + i * width;
        float h = std::min(magnitudes[i] * m_heightScale, 2.0f);

        // First triangle
        vertices.push_back(x);         vertices.push_back(-1.0f);
        vertices.push_back(x + width); vertices.push_back(-1.0f);
        vertices.push_back(x);         vertices.push_back(-1.0f + h);
        
        // Second triangle
        vertices.push_back(x + width); vertices.push_back(-1.0f);
        vertices.push_back(x + width); vertices.push_back(-1.0f + h);
        vertices.push_back(x);         vertices.push_back(-1.0f + h);
    }

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STREAM_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glUseProgram(m_shaderProgram);
    glDrawArrays(GL_TRIANGLES, 0, vertices.size() / 2);
}
void Visualizer::setColor(float r, float g, float b, float a) {
    glUseProgram(m_shaderProgram);
    GLint colorLoc = glGetUniformLocation(m_shaderProgram, "uColor");
    glUniform4f(colorLoc, r, g, b, a);
}
