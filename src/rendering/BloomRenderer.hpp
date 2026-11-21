#pragma once

#include "Framebuffer.hpp"
#include "FullscreenQuad.hpp"
#include "ShaderProgram.hpp"

class BloomRenderer {
public:
    BloomRenderer() = default;
    ~BloomRenderer() = default;

    BloomRenderer(const BloomRenderer&) = delete;
    BloomRenderer& operator=(const BloomRenderer&) = delete;
    BloomRenderer(BloomRenderer&&) = delete;
    BloomRenderer& operator=(BloomRenderer&&) = delete;

    void render(
        GLuint sourceTexture,
        GLuint targetFramebuffer,
        int width,
        int height,
        float strength = 0.8f);
    bool reloadShaders(bool force = false, std::string* errorOut = nullptr);

private:
    void initialize();
    void cacheUniforms() noexcept;

    ShaderProgram m_blurProgram;
    ShaderProgram m_compositeProgram;
    GLint m_blurDirectionLoc = -1;
    GLint m_blurPrefilterLoc = -1;
    GLint m_compStrengthLoc = -1;
    Framebuffer m_ping;
    Framebuffer m_pong;
    FullscreenQuad m_quad;
    bool m_initialized = false;
};
