#include "Visualizer.hpp"

Visualizer::Visualizer() {
    m_pipeline.initialize();
    m_quadRenderer.initialize();
    m_textRenderer.initialize();

    glEnable(GL_LINE_SMOOTH);
    glEnable(GL_MULTISAMPLE);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);

    GLfloat lineWidthRange[2] = { 1.0f, 1.0f };
    glGetFloatv(GL_SMOOTH_LINE_WIDTH_RANGE, lineWidthRange);
    if (lineWidthRange[1] >= 1.0f) {
        m_style.maxLineWidth = lineWidthRange[1];
    }
}

bool Visualizer::reloadShaders(bool force, std::string* errorOut) {
    std::string pipelineErr;
    std::string quadErr;
    const bool pipelineReloaded = m_pipeline.reloadShaders(force, &pipelineErr);
    const bool quadReloaded = m_quadRenderer.reloadShaders(force, &quadErr);
    if (errorOut && (!pipelineErr.empty() || !quadErr.empty())) {
        *errorOut = !pipelineErr.empty() ? pipelineErr : quadErr;
    }
    return pipelineReloaded || quadReloaded;
}

void Visualizer::setColor(float r, float g, float b, float a) {
    m_style.r = r;
    m_style.g = g;
    m_style.b = b;
    m_style.a = a;
    m_pipeline.use();
    m_pipeline.setColor(r, g, b, a);
}

void Visualizer::setViewportSize(int width, int height) {
    m_viewportWidth = width;
    m_viewportHeight = height;
}

void Visualizer::setupPersistence(int width, int height) {
    setViewportSize(width, height);
    m_persistenceBuffer.resize(width, height);
}

void Visualizer::clearPersistence() {
    if (!m_persistenceBuffer.ready()) return;
    m_persistenceBuffer.bind();
    glViewport(0, 0, m_persistenceBuffer.width(), m_persistenceBuffer.height());
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    Framebuffer::unbind();
}

void Visualizer::beginPersistence() {
    if (!m_persistenceBuffer.ready()) return;
    m_persistenceBuffer.bind();
    glViewport(0, 0, m_persistenceBuffer.width(), m_persistenceBuffer.height());
}

void Visualizer::endPersistence() {
    Framebuffer::unbind();
}

void Visualizer::drawPersistenceBuffer() {
    if (!m_persistenceBuffer.ready()) return;
    drawTexture(m_persistenceBuffer.texture());
}

void Visualizer::drawTexture(GLuint texture, float opacity) {
    m_quadRenderer.drawTexture(texture, opacity);
}

void Visualizer::drawTextureRegion(
    GLuint texture,
    float centerX,
    float centerY,
    float halfWidth,
    float halfHeight,
    float textureX,
    float textureY,
    float textureWidth,
    float textureHeight,
    float opacity
) {
    m_quadRenderer.drawTextureRegion(
        texture, centerX, centerY, halfWidth, halfHeight,
        textureX, textureY, textureWidth, textureHeight, opacity);
}

bool Visualizer::loadBackground(const std::string& path) {
    return m_quadRenderer.loadBackground(path);
}

void Visualizer::clearBackground() {
    m_quadRenderer.clearBackground();
}

void Visualizer::drawBackground(float scale, float shakeX, float shakeY, float rotation) {
    m_quadRenderer.drawBackground(scale, shakeX, shakeY, rotation);
}

void Visualizer::drawRoundedRect(float x, float y, float w, float h, float radius, const float color[4]) {
    m_quadRenderer.drawRoundedRect(x, y, w, h, radius, color);
}

void Visualizer::drawSolidRects(const std::vector<float>& vertices, const float color[4]) {
    m_spectrumRenderer.drawSolidRects(m_pipeline, vertices, color);
}

bool Visualizer::loadFont(const std::string& path) {
    return m_textRenderer.loadFont(path);
}

bool Visualizer::loadLyricsFont(const std::string& path) {
    return m_textRenderer.loadLyricsFont(path);
}

float Visualizer::measureTextWidth(const std::string& text, float scale, VisualizerFont font) {
    return m_textRenderer.measureTextWidth(text, scale, m_viewportWidth, font);
}

void Visualizer::drawText(
    const std::string& text,
    float x,
    float y,
    float scale,
    const float color[4],
    VisualizerFont font
) {
    m_textRenderer.drawText(
        m_quadRenderer, text, x, y, scale,
        m_viewportWidth, m_viewportHeight, color, font);
}

void Visualizer::drawFullscreenDimmer(float decayRate) {
    m_quadRenderer.drawFullscreenDimmer(decayRate);
}

void Visualizer::renderGrid() {
    if (!m_showGrid) return;
    m_xyTraceRenderer.renderGrid(
        m_pipeline, m_gridColumns, m_gridRows,
        m_viewportWidth, m_viewportHeight,
        m_style.r, m_style.g, m_style.b, m_style.a);
}

void Visualizer::render(const std::vector<float>& magnitudes) {
    m_spectrumRenderer.render(m_pipeline, magnitudes, m_style, m_viewportWidth, m_viewportHeight);
}

void Visualizer::renderXY(const XYTraceBatch& trace) {
    m_xyTraceRenderer.renderTrace(
        m_pipeline, trace, m_viewportWidth, m_viewportHeight,
        m_style.traceWidth, m_bloomIntensity,
        m_style.r, m_style.g, m_style.b, m_style.a);
}
