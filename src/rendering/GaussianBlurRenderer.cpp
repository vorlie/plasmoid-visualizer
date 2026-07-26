#include "GaussianBlurRenderer.hpp"

#include "AssetPaths.hpp"
#include <algorithm>

GaussianBlurRenderer::~GaussianBlurRenderer() {
    if (m_quadVao != 0) glDeleteVertexArrays(1, &m_quadVao);
    if (m_quadVbo != 0) glDeleteBuffers(1, &m_quadVbo);
}

void GaussianBlurRenderer::initialize() {
    if (m_initialized) return;
    m_program.load(AssetPaths::shader("quad.vert"), AssetPaths::shader("blur.frag"));

    constexpr float vertices[] = {
        -1,  1, 0, 1, -1, -1, 0, 0,  1, -1, 1, 0,
        -1,  1, 0, 1,  1, -1, 1, 0,  1,  1, 1, 1
    };
    glGenVertexArrays(1, &m_quadVao);
    glGenBuffers(1, &m_quadVbo);
    glBindVertexArray(m_quadVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_quadVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
        reinterpret_cast<void*>(2 * sizeof(float)));
    m_initialized = true;
}

GLuint GaussianBlurRenderer::blur(GLuint sourceTexture, int width, int height, int passes) {
    if (sourceTexture == 0 || width <= 0 || height <= 0) return 0;
    initialize();
    const int blurWidth = std::max(width / 2, 1);
    const int blurHeight = std::max(height / 2, 1);
    m_ping.resize(blurWidth, blurHeight);
    m_pong.resize(blurWidth, blurHeight);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glViewport(0, 0, blurWidth, blurHeight);
    glBindVertexArray(m_quadVao);
    glUseProgram(m_program);
    glUniform1i(glGetUniformLocation(m_program, "uTexture"), 0);
    glUniform1f(glGetUniformLocation(m_program, "uThreshold"), 0.0f);
    glUniform1f(glGetUniformLocation(m_program, "uSoftKnee"), 0.0f);
    glUniform1i(glGetUniformLocation(m_program, "uPrefilter"), 0);

    GLuint input = sourceTexture;
    const int count = std::max(passes, 2);
    for (int pass = 0; pass < count; ++pass) {
        Framebuffer& destination = pass % 2 == 0 ? m_ping : m_pong;
        destination.bind();
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, input);
        glUniform2f(
            glGetUniformLocation(m_program, "uDirection"),
            pass % 2 == 0 ? 1.0f : 0.0f,
            pass % 2 == 0 ? 0.0f : 1.0f);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        input = destination.texture();
    }

    glUseProgram(0);
    glBindVertexArray(0);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    return input;
}
