#ifndef VISUALIZER_HPP
#define VISUALIZER_HPP

#include "VisualizerTypes.hpp"
#include "ShaderProgram.hpp"
#include "Framebuffer.hpp"
#include "Texture2D.hpp"
#include "FontAtlas.hpp"
#include "XYOscilloscopeTypes.hpp"
#include <GL/glew.h>
#include <vector>
#include <string>

class Visualizer {
public:
    Visualizer();
    ~Visualizer();

    void render(const std::vector<float>& magnitudes);
    void renderXY(const XYTraceBatch& trace);
    void renderGrid();
    void drawFullscreenDimmer(float decayRate);
    void setHeightScale(float scale);
    void setColor(float r, float g, float b, float a);
    void setMirrored(bool mirrored);
    void setShape(VisualizerShape shape);
    void setCornerRadius(float radius);
    void setViewportSize(int width, int height); // For aspect ratio correction
    [[nodiscard]] int viewportWidth() const noexcept { return m_viewportWidth; }
    [[nodiscard]] int viewportHeight() const noexcept { return m_viewportHeight; }
    void setRotation(float angleRadians); // For XY oscilloscope rotation
    void setFlip(bool flipX, bool flipY); // For XY oscilloscope flip
    void setBloomIntensity(float intensity); // For XY oscilloscope bloom
    void setGridEnabled(bool enabled) { m_showGrid = enabled; }
    void setGridDivisions(int columns, int rows) { m_gridColumns = columns; m_gridRows = rows; }
    void setTraceWidth(float width) { m_traceWidth = width; }
    void setFillOpacity(float opacity) { m_fillOpacity = opacity; }
    void setBeamHeadSize(float size) { m_beamHeadSize = size; }
    void setVelocityModulation(float amount) { m_velocityModulation = amount; }
    void setXYAutoGain(bool enabled) { m_xyAutoGain = enabled; }
    void setBarAnchor(BarAnchor anchor) { m_barAnchor = anchor; }
    void setOffset(float x, float y) { m_offsetX = x; m_offsetY = y; }
    void setScale(float x, float y) { m_scaleX = x; m_scaleY = y; }
    void setPersistenceEnabled(bool enabled) { m_persistenceEnabled = enabled; }

    // FBO Persistence
    void setupPersistence(int width, int height);
    void beginPersistence();
    void endPersistence();
    void drawPersistenceBuffer();
    void drawTexture(GLuint texture, float opacity = 1.0f);
    [[nodiscard]] GLuint persistenceTexture() const noexcept {
        return m_persistenceBuffer.texture();
    }
    
    // Background Image & Zen-Kun Effects
    bool loadBackground(const std::string& path);
    void drawBackground(float scale, float shakeX, float shakeY, float rotation);
    void clearBackground();
    void drawRoundedRect(float x, float y, float w, float h, float radius, const float color[4]);
    void drawText(const std::string& text, float x, float y, float scale, const float color[4]);
    bool loadFont(const std::string& path);

private:
    ShaderProgram m_shaderProgram;
    ShaderProgram m_quadShaderProgram;
    GLuint m_vao = 0, m_vbo = 0;
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
    int m_gridColumns = 10;
    int m_gridRows = 8;
    float m_traceWidth = 2.0f;
    float m_fillOpacity = 0.0f; // 0.0 = no fill, 1.0 = solid fill
    float m_beamHeadSize = 0.0f; // 0.0 = disabled
    float m_velocityModulation = 0.0f; // 0.0 = disabled
    bool m_xyAutoGain = false;
    BarAnchor m_barAnchor = BarAnchor::Bottom;
    float m_offsetX = 0.0f;
    float m_offsetY = 0.0f;
    float m_scaleX = 1.0f;
    float m_scaleY = 1.0f;
    bool m_persistenceEnabled = true;

    Framebuffer m_persistenceBuffer;
    GLuint m_quadVAO = 0, m_quadVBO = 0;

    Texture2D m_backgroundTexture;

    FontAtlas m_fontAtlas;

    void initShaders();
    void initQuad();
};

#endif // VISUALIZER_HPP
