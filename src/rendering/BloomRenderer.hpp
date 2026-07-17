#pragma once

#include "Framebuffer.hpp"
#include "ShaderProgram.hpp"

#include <GL/glew.h>

class BloomRenderer {
public:
    BloomRenderer() = default;
    ~BloomRenderer();

    BloomRenderer(const BloomRenderer&) = delete;
    BloomRenderer& operator=(const BloomRenderer&) = delete;

    void render(
        GLuint sourceTexture,
        GLuint targetFramebuffer,
        int width,
        int height,
        float strength = 0.8f);

private:
    void initialize();

    ShaderProgram m_blurProgram;
    ShaderProgram m_compositeProgram;
    Framebuffer m_ping;
    Framebuffer m_pong;
    GLuint m_quadVao = 0;
    GLuint m_quadVbo = 0;
    bool m_initialized = false;
};
