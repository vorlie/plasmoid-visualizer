#pragma once

#include "Framebuffer.hpp"
#include "ShaderProgram.hpp"
#include <GL/glew.h>

class GaussianBlurRenderer {
public:
    ~GaussianBlurRenderer();
    GaussianBlurRenderer(const GaussianBlurRenderer&) = delete;
    GaussianBlurRenderer& operator=(const GaussianBlurRenderer&) = delete;
    GaussianBlurRenderer() = default;

    GLuint blur(GLuint sourceTexture, int width, int height, int passes = 8);

private:
    void initialize();

    ShaderProgram m_program;
    Framebuffer m_ping;
    Framebuffer m_pong;
    GLuint m_quadVao = 0;
    GLuint m_quadVbo = 0;
    bool m_initialized = false;
};
