#pragma once

#include "FullscreenQuad.hpp"
#include "ShaderProgram.hpp"
#include "Texture2D.hpp"

#include <GL/glew.h>
#include <string>

class QuadRenderer {
public:
    QuadRenderer() = default;
    ~QuadRenderer() = default;
    QuadRenderer(const QuadRenderer&) = delete;
    QuadRenderer& operator=(const QuadRenderer&) = delete;
    QuadRenderer(QuadRenderer&&) = delete;
    QuadRenderer& operator=(QuadRenderer&&) = delete;

    void initialize();
    bool reloadShaders(bool force = false, std::string* errorOut = nullptr);

    void drawTexture(GLuint texture, float opacity = 1.0f) const;
    void drawTextureRegion(
        GLuint texture,
        float centerX,
        float centerY,
        float halfWidth,
        float halfHeight,
        float textureX,
        float textureY,
        float textureWidth,
        float textureHeight,
        float opacity = 1.0f) const;
    void drawRoundedRect(float x, float y, float w, float h, float radius, const float color[4]) const;
    void drawFullscreenDimmer(float decayRate) const;

    bool loadBackground(const std::string& path);
    void clearBackground();
    void drawBackground(float scale, float shakeX, float shakeY, float rotation) const;

    // Draws a pre-bound text VAO batch using the quad shader in font mode.
    void drawFontTriangles(
        GLuint atlasTexture,
        const float color[4],
        GLint firstVertex,
        GLsizei vertexCount) const;

private:
    void cacheUniforms() noexcept;

    ShaderProgram m_program;
    GLint m_textureLoc = -1;
    GLint m_useTextureLoc = -1;
    GLint m_isFontLoc = -1;
    GLint m_colorLoc = -1;
    GLint m_sizeLoc = -1;
    GLint m_offsetLoc = -1;
    GLint m_rotationLoc = -1;
    GLint m_cornerRadiusLoc = -1;
    GLint m_aspectLoc = -1;
    GLint m_texRectLoc = -1;

    FullscreenQuad m_quad;
    Texture2D m_backgroundTexture;
};
