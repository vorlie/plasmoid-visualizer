#include "GaussianBlurRenderer.hpp"

#include "AssetPaths.hpp"
#include <algorithm>

void GaussianBlurRenderer::cacheUniforms() noexcept {
    m_directionLoc = glGetUniformLocation(m_program, "uDirection");
    glUseProgram(m_program);
    glUniform1i(glGetUniformLocation(m_program, "uTexture"), 0);
    glUniform1f(glGetUniformLocation(m_program, "uThreshold"), 0.0f);
    glUniform1f(glGetUniformLocation(m_program, "uSoftKnee"), 0.0f);
    glUniform1i(glGetUniformLocation(m_program, "uPrefilter"), 0);
    glUseProgram(0);
}

void GaussianBlurRenderer::initialize() {
    if (m_initialized) return;
    m_program.load(AssetPaths::shader("quad.vert"), AssetPaths::shader("blur.frag"));
    cacheUniforms();
    m_quad.initialize();
    m_initialized = true;
}

bool GaussianBlurRenderer::reloadShaders(bool force, std::string* errorOut) {
    if (!m_initialized) return false;
    if (!m_program.reloadIfChanged(force, errorOut)) return false;
    cacheUniforms();
    return true;
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
    m_quad.bind();
    glUseProgram(m_program);
    glActiveTexture(GL_TEXTURE0);

    GLuint input = sourceTexture;
    const int count = std::max(passes, 2);
    for (int pass = 0; pass < count; ++pass) {
        Framebuffer& destination = pass % 2 == 0 ? m_ping : m_pong;
        destination.bind();
        glBindTexture(GL_TEXTURE_2D, input);
        glUniform2f(
            m_directionLoc,
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
