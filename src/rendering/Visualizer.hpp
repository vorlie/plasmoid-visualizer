#ifndef VISUALIZER_HPP
#define VISUALIZER_HPP

#include "Framebuffer.hpp"
#include "QuadRenderer.hpp"
#include "SpectrumRenderer.hpp"
#include "TextRenderer.hpp"
#include "VisualizerPipeline.hpp"
#include "VisualizerTypes.hpp"
#include "XYOscilloscopeTypes.hpp"
#include "XYTraceRenderer.hpp"

#include <string>
#include <vector>

class Visualizer {
public:
    Visualizer();
    ~Visualizer() = default;
    Visualizer(const Visualizer&) = delete;
    Visualizer& operator=(const Visualizer&) = delete;
    Visualizer(Visualizer&&) = delete;
    Visualizer& operator=(Visualizer&&) = delete;

    void render(const std::vector<float>& magnitudes);
    void renderXY(const XYTraceBatch& trace);
    void renderGrid();
    void drawFullscreenDimmer(float decayRate);
    bool reloadShaders(bool force = false, std::string* errorOut = nullptr);

    void setHeightScale(float scale) { m_style.heightScale = scale; }
    void setColor(float r, float g, float b, float a);
    void setMirrored(bool mirrored) { m_style.mirrored = mirrored; }
    void setShape(VisualizerShape shape) { m_style.shape = shape; }
    void setCornerRadius(float radius) { m_style.cornerRadius = radius; }
    void setViewportSize(int width, int height);
    [[nodiscard]] int viewportWidth() const noexcept { return m_viewportWidth; }
    [[nodiscard]] int viewportHeight() const noexcept { return m_viewportHeight; }
    void setBloomIntensity(float intensity) { m_bloomIntensity = intensity; }
    void setGridEnabled(bool enabled) { m_showGrid = enabled; }
    void setGridDivisions(int columns, int rows) { m_gridColumns = columns; m_gridRows = rows; }
    void setTraceWidth(float width) { m_style.traceWidth = width; }
    void setFillOpacity(float opacity) { m_style.fillOpacity = opacity; }
    void setBarAnchor(BarAnchor anchor) { m_style.barAnchor = anchor; }

    // FBO Persistence
    void setupPersistence(int width, int height);
    void clearPersistence();
    void beginPersistence();
    void endPersistence();
    void drawPersistenceBuffer();
    void drawTexture(GLuint texture, float opacity = 1.0f);
    void drawTextureRegion(
        GLuint texture, float centerX, float centerY, float halfWidth, float halfHeight,
        float textureX, float textureY, float textureWidth, float textureHeight,
        float opacity = 1.0f);
    [[nodiscard]] GLuint persistenceTexture() const noexcept {
        return m_persistenceBuffer.texture();
    }

    // Background Image, Rectangles & Typography
    bool loadBackground(const std::string& path);
    void drawBackground(float scale, float shakeX, float shakeY, float rotation);
    void clearBackground();
    void drawRoundedRect(float x, float y, float w, float h, float radius, const float color[4]);
    void drawSolidRects(const std::vector<float>& vertices, const float color[4]);
    void drawText(
        const std::string& text, float x, float y, float scale,
        const float color[4], VisualizerFont font = VisualizerFont::Primary);
    [[nodiscard]] float measureTextWidth(
        const std::string& text, float scale,
        VisualizerFont font = VisualizerFont::Primary);
    bool loadFont(const std::string& path);
    bool loadLyricsFont(const std::string& path);

private:
    VisualizerPipeline m_pipeline;
    QuadRenderer m_quadRenderer;
    TextRenderer m_textRenderer;
    SpectrumRenderer m_spectrumRenderer;
    XYTraceRenderer m_xyTraceRenderer;
    Framebuffer m_persistenceBuffer;

    SpectrumStyle m_style;
    int m_viewportWidth = 1920;
    int m_viewportHeight = 1080;
    float m_bloomIntensity = 1.0f;
    bool m_showGrid = true;
    int m_gridColumns = 10;
    int m_gridRows = 8;
};

#endif // VISUALIZER_HPP
