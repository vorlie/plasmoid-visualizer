#ifndef VISUALIZER_HPP
#define VISUALIZER_HPP

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <vector>

enum class VisualizerShape {
    Bars,
    Lines,
    Dots
};

class Visualizer {
public:
    Visualizer();
    ~Visualizer();

    void render(const std::vector<float>& magnitudes);
    void setHeightScale(float scale) { m_heightScale = scale; }
    void setColor(float r, float g, float b, float a);
    void setMirrored(bool mirrored) { m_mirrored = mirrored; }
    void setShape(VisualizerShape shape) { m_shape = shape; }
    void setCornerRadius(float radius) { m_cornerRadius = radius; }

private:
    GLuint m_shaderProgram;
    GLuint m_vao, m_vbo;
    float m_heightScale = 0.2f;
    bool m_mirrored = false;
    VisualizerShape m_shape = VisualizerShape::Bars;
    float m_cornerRadius = 0.0f;

    void initShaders();
};

#endif // VISUALIZER_HPP
