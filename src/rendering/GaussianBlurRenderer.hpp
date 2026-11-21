#pragma once

#include "Framebuffer.hpp"
#include "FullscreenQuad.hpp"
#include "ShaderProgram.hpp"

class GaussianBlurRenderer {
public:
    GaussianBlurRenderer() = default;
    ~GaussianBlurRenderer() = default;
    GaussianBlurRenderer(const GaussianBlurRenderer&) = delete;
    GaussianBlurRenderer& operator=(const GaussianBlurRenderer&) = delete;
    GaussianBlurRenderer(GaussianBlurRenderer&&) = delete;
    GaussianBlurRenderer& operator=(GaussianBlurRenderer&&) = delete;

    GLuint blur(GLuint sourceTexture, int width, int height, int passes = 8);
    bool reloadShaders(bool force = false, std::string* errorOut = nullptr);

private:
    void initialize();
    void cacheUniforms() noexcept;

    ShaderProgram m_program;
    GLint m_directionLoc = -1;
    Framebuffer m_ping;
    Framebuffer m_pong;
    FullscreenQuad m_quad;
    bool m_initialized = false;
};
