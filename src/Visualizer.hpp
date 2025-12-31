#ifndef VISUALIZER_HPP
#define VISUALIZER_HPP

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <vector>

enum class VisualizerShape {
    Bars,
    Lines,
    Dots,
    Waveform,
    OscilloscopeXY
};

class Visualizer {
public:
    Visualizer();
    ~Visualizer();

    void render(const std::vector<float>& magnitudes);
    void drawFullscreenDimmer(float decayRate);
    void setHeightScale(float scale);
    void setColor(float r, float g, float b, float a);
    void setMirrored(bool mirrored);
    void setShape(VisualizerShape shape);
    void setCornerRadius(float radius);
    void setViewportSize(int width, int height); // For aspect ratio correction
    void setRotation(float angleRadians); // For XY oscilloscope rotation
    void setFlip(bool flipX, bool flipY); // For XY oscilloscope flip

private:
    GLuint m_shaderProgram;
    GLuint m_vao, m_vbo;
    float m_heightScale = 0.2f;
    bool m_mirrored = false;
    VisualizerShape m_shape = VisualizerShape::Bars;
    float m_cornerRadius = 0.0f;
    int m_viewportWidth = 1920;
    int m_viewportHeight = 1080;
    float m_r = 1.0f, m_g = 1.0f, m_b = 1.0f, m_a = 1.0f; // Stored for glow effect
    float m_rotationAngle = 0.0f; // For XY oscilloscope rotation in radians
    bool m_flipX = false;
    bool m_flipY = false;

    void initShaders();
};

#endif // VISUALIZER_HPP
