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
    OscilloscopeXY,
    Curve
};

class Visualizer {
public:
    Visualizer();
    ~Visualizer();

    void render(const std::vector<float>& magnitudes);
    void renderGrid();
    void drawFullscreenDimmer(float decayRate);
    void setHeightScale(float scale);
    void setColor(float r, float g, float b, float a);
    void setMirrored(bool mirrored);
    void setShape(VisualizerShape shape);
    void setCornerRadius(float radius);
    void setViewportSize(int width, int height); // For aspect ratio correction
    void setRotation(float angleRadians); // For XY oscilloscope rotation
    void setFlip(bool flipX, bool flipY); // For XY oscilloscope flip
    void setBloomIntensity(float intensity); // For XY oscilloscope bloom
    void setGridEnabled(bool enabled) { m_showGrid = enabled; }
    void setTraceWidth(float width) { m_traceWidth = width; }
    void setFillOpacity(float opacity) { m_fillOpacity = opacity; }
    void setBeamHeadSize(float size) { m_beamHeadSize = size; }
    void setVelocityModulation(float amount) { m_velocityModulation = amount; }

    // FBO Persistence
    void setupPersistence(int width, int height);
    void beginPersistence();
    void endPersistence();
    void drawPersistenceBuffer();

private:
    GLuint m_shaderProgram;
    GLuint m_quadShaderProgram;
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
    float m_bloomIntensity = 1.0f;
    bool m_showGrid = true;
    float m_traceWidth = 2.0f;
    float m_fillOpacity = 0.0f; // 0.0 = no fill, 1.0 = solid fill
    float m_beamHeadSize = 0.0f; // 0.0 = disabled
    float m_velocityModulation = 0.0f; // 0.0 = disabled

    // FBO Persistence
    GLuint m_fbo = 0;
    GLuint m_fboTexture = 0;
    int m_fboWidth = 0;
    int m_fboHeight = 0;
    GLuint m_quadVAO = 0, m_quadVBO = 0;

    void initShaders();
    void initQuad();
};

#endif // VISUALIZER_HPP
