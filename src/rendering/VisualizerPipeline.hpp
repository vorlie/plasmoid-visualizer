#pragma once

#include "AssetPaths.hpp"
#include "ShaderProgram.hpp"
#include "StreamingVertexBuffer.hpp"

#include <GL/glew.h>
#include <string>

class VisualizerPipeline {
public:
    VisualizerPipeline() = default;
    ~VisualizerPipeline() = default;
    VisualizerPipeline(const VisualizerPipeline&) = delete;
    VisualizerPipeline& operator=(const VisualizerPipeline&) = delete;
    VisualizerPipeline(VisualizerPipeline&&) = delete;
    VisualizerPipeline& operator=(VisualizerPipeline&&) = delete;

    void initialize() {
        m_program.load(
            AssetPaths::shader("visualizer.vert"),
            AssetPaths::shader("visualizer.frag"));
        cacheUniforms();

        m_vertices.initialize(5, 256 * 1024);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), nullptr);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), reinterpret_cast<void*>(2 * sizeof(float)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 5 * sizeof(float), reinterpret_cast<void*>(4 * sizeof(float)));
        glEnableVertexAttribArray(2);
        m_vertices.finishLayout();
    }

    bool reloadShaders(bool force = false, std::string* errorOut = nullptr) {
        if (!m_program.reloadIfChanged(force, errorOut)) return false;
        cacheUniforms();
        return true;
    }

    void use() const noexcept { glUseProgram(m_program); }
    void setColor(float r, float g, float b, float a) const noexcept { glUniform4f(m_colorLoc, r, g, b, a); }
    void setCornerRadius(float radius) const noexcept { glUniform1f(m_cornerRadiusLoc, radius); }
    void setShape(int shape) const noexcept { glUniform1i(m_shapeLoc, shape); }
    GLint uploadVertices(const std::vector<float>& vertices) { return m_vertices.upload(vertices); }

private:
    void cacheUniforms() noexcept {
        m_colorLoc = glGetUniformLocation(m_program, "uColor");
        m_cornerRadiusLoc = glGetUniformLocation(m_program, "uCornerRadius");
        m_shapeLoc = glGetUniformLocation(m_program, "uShape");
    }

    ShaderProgram m_program;
    GLint m_colorLoc = -1;
    GLint m_cornerRadiusLoc = -1;
    GLint m_shapeLoc = -1;
    StreamingVertexBuffer m_vertices;
};
