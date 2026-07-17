#include "BloomRenderer.hpp"

#include "AssetPaths.hpp"

#include <algorithm>

BloomRenderer::~BloomRenderer() {
    if (m_quadVao != 0) glDeleteVertexArrays(1, &m_quadVao);
    if (m_quadVbo != 0) glDeleteBuffers(1, &m_quadVbo);
}

void BloomRenderer::initialize() {
    if (m_initialized) return;

    m_blurProgram.load(
        AssetPaths::shader("quad.vert"),
        AssetPaths::shader("blur.frag"));
    m_compositeProgram.load(
        AssetPaths::shader("quad.vert"),
        AssetPaths::shader("composite.frag"));

    constexpr float quadVertices[] = {
        -1.0f,  1.0f, 0.0f, 1.0f,
        -1.0f, -1.0f, 0.0f, 0.0f,
         1.0f, -1.0f, 1.0f, 0.0f,
        -1.0f,  1.0f, 0.0f, 1.0f,
         1.0f, -1.0f, 1.0f, 0.0f,
         1.0f,  1.0f, 1.0f, 1.0f
    };

    glGenVertexArrays(1, &m_quadVao);
    glGenBuffers(1, &m_quadVbo);
    glBindVertexArray(m_quadVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_quadVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
        reinterpret_cast<void*>(2 * sizeof(float)));

    m_initialized = true;
}

void BloomRenderer::render(
    GLuint sourceTexture,
    GLuint targetFramebuffer,
    int width,
    int height,
    float strength
) {
    if (sourceTexture == 0 || width <= 0 || height <= 0 || strength <= 0.0f) return;
    initialize();

    const int bloomWidth = std::max(width / 2, 1);
    const int bloomHeight = std::max(height / 2, 1);
    m_ping.resize(bloomWidth, bloomHeight);
    m_pong.resize(bloomWidth, bloomHeight);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glViewport(0, 0, bloomWidth, bloomHeight);
    glBindVertexArray(m_quadVao);
    glUseProgram(m_blurProgram);
    glUniform1i(glGetUniformLocation(m_blurProgram, "uTexture"), 0);
    glUniform1f(glGetUniformLocation(m_blurProgram, "uThreshold"), 1.0f);
    glUniform1f(glGetUniformLocation(m_blurProgram, "uSoftKnee"), 0.5f);

    GLuint inputTexture = sourceTexture;
    constexpr int BlurPasses = 8;
    for (int pass = 0; pass < BlurPasses; ++pass) {
        Framebuffer& destination = (pass % 2 == 0) ? m_ping : m_pong;
        destination.bind();
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, inputTexture);
        glUniform2f(
            glGetUniformLocation(m_blurProgram, "uDirection"),
            pass % 2 == 0 ? 1.0f : 0.0f,
            pass % 2 == 0 ? 0.0f : 1.0f);
        glUniform1i(glGetUniformLocation(m_blurProgram, "uPrefilter"), pass == 0 ? 1 : 0);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        inputTexture = destination.texture();
    }

    glBindFramebuffer(GL_FRAMEBUFFER, targetFramebuffer);
    glViewport(0, 0, width, height);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    glUseProgram(m_compositeProgram);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, inputTexture);
    glUniform1i(glGetUniformLocation(m_compositeProgram, "uBloom"), 0);
    glUniform1f(glGetUniformLocation(m_compositeProgram, "uStrength"), strength);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(0);
    glBindVertexArray(0);
}
