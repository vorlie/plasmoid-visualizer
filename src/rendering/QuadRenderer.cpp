#include "QuadRenderer.hpp"
#include "AssetPaths.hpp"
#include "Utf8Paths.hpp"

#include <cmath>

void QuadRenderer::cacheUniforms() noexcept {
    m_textureLoc = glGetUniformLocation(m_program, "uTexture");
    m_useTextureLoc = glGetUniformLocation(m_program, "uUseTexture");
    m_isFontLoc = glGetUniformLocation(m_program, "uIsFont");
    m_colorLoc = glGetUniformLocation(m_program, "uColor");
    m_sizeLoc = glGetUniformLocation(m_program, "uSize");
    m_offsetLoc = glGetUniformLocation(m_program, "uOffset");
    m_rotationLoc = glGetUniformLocation(m_program, "uRotation");
    m_cornerRadiusLoc = glGetUniformLocation(m_program, "uCornerRadius");
    m_aspectLoc = glGetUniformLocation(m_program, "uAspect");
    m_texRectLoc = glGetUniformLocation(m_program, "uTexRect");
    m_saturationLoc = glGetUniformLocation(m_program, "uSaturation");
    m_colorShiftLoc = glGetUniformLocation(m_program, "uColorShift");

    glUseProgram(m_program);
    if (m_textureLoc != -1) glUniform1i(m_textureLoc, 0);
    if (m_saturationLoc != -1) glUniform1f(m_saturationLoc, 1.0f);
    if (m_colorShiftLoc != -1) glUniform1f(m_colorShiftLoc, 0.0f);
    glUseProgram(0);
}

void QuadRenderer::initialize() {
    m_program.load(
        AssetPaths::shader("quad.vert"),
        AssetPaths::shader("quad.frag"));
    cacheUniforms();
    m_quad.initialize();
}

bool QuadRenderer::reloadShaders(bool force, std::string* errorOut) {
    if (!m_program.reloadIfChanged(force, errorOut)) return false;
    cacheUniforms();
    return true;
}

void QuadRenderer::drawTexture(GLuint texture, float opacity) const {
    drawPhosphorTexture(texture, opacity, 1.0f, 0.0f);
}

void QuadRenderer::drawPhosphorTexture(
    GLuint texture,
    float opacity,
    float saturation,
    float colorShift
) const {
    if (texture == 0) return;
    glUseProgram(m_program);
    glUniform1i(m_useTextureLoc, 1);
    glUniform1i(m_isFontLoc, 0);
    glUniform4f(m_colorLoc, 1.0f, 1.0f, 1.0f, opacity);
    if (m_sizeLoc != -1) glUniform2f(m_sizeLoc, 1.0f, 1.0f);
    if (m_offsetLoc != -1) glUniform2f(m_offsetLoc, 0.0f, 0.0f);
    glUniform1f(m_rotationLoc, 0.0f);
    glUniform1f(m_cornerRadiusLoc, 0.0f);
    glUniform4f(m_texRectLoc, 0.0f, 0.0f, 1.0f, 1.0f);
    if (m_saturationLoc != -1) glUniform1f(m_saturationLoc, saturation);
    if (m_colorShiftLoc != -1) glUniform1f(m_colorShiftLoc, colorShift);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    m_quad.draw();
    if (m_saturationLoc != -1) glUniform1f(m_saturationLoc, 1.0f);
    if (m_colorShiftLoc != -1) glUniform1f(m_colorShiftLoc, 0.0f);
    glUseProgram(0);
}

void QuadRenderer::drawTextureRegion(
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
) const {
    if (texture == 0) return;
    glUseProgram(m_program);
    glUniform1i(m_useTextureLoc, 1);
    glUniform1i(m_isFontLoc, 0);
    glUniform4f(m_colorLoc, 1.0f, 1.0f, 1.0f, opacity);
    glUniform2f(m_sizeLoc, halfWidth, halfHeight);
    glUniform2f(m_offsetLoc, centerX, centerY);
    glUniform1f(m_rotationLoc, 0.0f);
    glUniform1f(m_cornerRadiusLoc, 0.0f);
    glUniform4f(m_texRectLoc, textureX, textureY, textureWidth, textureHeight);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    m_quad.draw();
    glUseProgram(0);
}

void QuadRenderer::drawRoundedRect(
    float x, float y, float w, float h, float radius, const float color[4]
) const {
    glUseProgram(m_program);
    glUniform2f(m_sizeLoc, w, h);
    glUniform2f(m_offsetLoc, x, y);
    glUniform1f(m_rotationLoc, 0.0f);
    glUniform1f(m_cornerRadiusLoc, radius);
    const float safeAspect = std::abs(h) > 1e-6f ? (w / h) : 1.0f;
    glUniform1f(m_aspectLoc, safeAspect);
    glUniform1i(m_useTextureLoc, 0);
    glUniform1i(m_isFontLoc, 0);
    glUniform4f(m_colorLoc, color[0], color[1], color[2], color[3]);
    glUniform4f(m_texRectLoc, 0.0f, 0.0f, 1.0f, 1.0f);

    m_quad.draw();
    glUseProgram(0);
}

void QuadRenderer::drawFullscreenDimmer(float decayRate) const {
    glUseProgram(m_program);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUniform4f(m_colorLoc, 0.0f, 0.0f, 0.0f, decayRate);
    glUniform1i(m_useTextureLoc, 0);
    glUniform1i(m_isFontLoc, 0);
    glUniform1f(m_cornerRadiusLoc, 0.0f);
    glUniform1f(m_rotationLoc, 0.0f);
    if (m_sizeLoc != -1) glUniform2f(m_sizeLoc, 1.0f, 1.0f);
    if (m_offsetLoc != -1) glUniform2f(m_offsetLoc, 0.0f, 0.0f);
    glUniform4f(m_texRectLoc, 0.0f, 0.0f, 1.0f, 1.0f);

    m_quad.draw();
    glUseProgram(0);
}

bool QuadRenderer::loadBackground(const std::string& path) {
    return m_backgroundTexture.loadRgba(Utf8Paths::fromUtf8(path), true);
}

void QuadRenderer::clearBackground() {
    m_backgroundTexture.reset();
}

void QuadRenderer::drawBackground(float scale, float shakeX, float shakeY, float rotation) const {
    if (!m_backgroundTexture.loaded()) return;

    glUseProgram(m_program);
    glUniform2f(m_sizeLoc, scale, scale);
    glUniform2f(m_offsetLoc, shakeX, shakeY);
    glUniform1f(m_rotationLoc, rotation);
    glUniform1f(m_cornerRadiusLoc, 0.0f);
    glUniform4f(m_texRectLoc, 0.0f, 0.0f, 1.0f, 1.0f);
    glUniform1i(m_useTextureLoc, 1);
    glUniform1i(m_isFontLoc, 0);
    glUniform4f(m_colorLoc, 1.0f, 1.0f, 1.0f, 1.0f);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_backgroundTexture.id());
    m_quad.draw();
    glUseProgram(0);
}

void QuadRenderer::drawFontTriangles(
    GLuint atlasTexture,
    const float color[4],
    GLint firstVertex,
    GLsizei vertexCount
) const {
    glUseProgram(m_program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, atlasTexture);
    glUniform1i(m_useTextureLoc, 1);
    glUniform1i(m_isFontLoc, 1);
    glUniform4f(m_colorLoc, color[0], color[1], color[2], color[3]);
    glUniform2f(m_sizeLoc, 1.0f, 1.0f);
    glUniform2f(m_offsetLoc, 0.0f, 0.0f);
    glUniform1f(m_cornerRadiusLoc, 0.0f);
    glUniform1f(m_rotationLoc, 0.0f);
    glUniform4f(m_texRectLoc, 0.0f, 0.0f, 1.0f, 1.0f);

    glDrawArrays(GL_TRIANGLES, firstVertex, vertexCount);
    glUniform1i(m_isFontLoc, 0);
    glUseProgram(0);
}
