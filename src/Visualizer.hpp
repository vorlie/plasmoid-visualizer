#ifndef VISUALIZER_HPP
#define VISUALIZER_HPP

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <vector>

class Visualizer {
public:
    Visualizer();
    ~Visualizer();

    void render(const std::vector<float>& magnitudes);
    void setHeightScale(float scale) { m_heightScale = scale; }
    void setColor(float r, float g, float b, float a);

private:
    GLuint m_shaderProgram;
    GLuint m_vao, m_vbo;
    float m_heightScale = 0.2f;

    void initShaders();
};

#endif // VISUALIZER_HPP
