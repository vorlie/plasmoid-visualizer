#include "BloomRenderer.hpp"

#include "AssetPaths.hpp"

#include <algorithm>

void BloomRenderer::cacheUniforms() noexcept {
    m_blurDirectionLoc = glGetUniformLocation(m_blurProgram, "uDirection");
    m_blurPrefilterLoc = glGetUniformLocation(m_blurProgram, "uPrefilter");
    m_compStrengthLoc = glGetUniformLocation(m_compositeProgram, "uStrength");

    glUseProgram(m_blurProgram);
    glUniform1i(glGetUniformLocation(m_blurProgram, "uTexture"), 0);
    glUniform1f(glGetUniformLocation(m_blurProgram, "uThreshold"), 1.0f);
    glUniform1f(glGetUniformLocation(m_blurProgram, "uSoftKnee"), 0.5f);
    glUseProgram(m_compositeProgram);
    glUniform1i(glGetUniformLocation(m_compositeProgram, "uBloom"), 0);
    glUseProgram(0);
}

void BloomRenderer::initialize() {
    if (m_initialized) return;

    m_blurProgram.load(
        AssetPaths::shader("quad.vert"),
        AssetPaths::shader("blur.frag"));
    m_compositeProgram.load(
        AssetPaths::shader("quad.vert"),
        AssetPaths::shader("composite.frag"));
    cacheUniforms();
    m_quad.initialize();
    m_initialized = true;
}

bool BloomRenderer::reloadShaders(bool force, std::string* errorOut) {
    if (!m_initialized) return false;
    std::string blurErr, compErr;
    const bool blurReloaded = m_blurProgram.reloadIfChanged(force, &blurErr);
    const bool compReloaded = m_compositeProgram.reloadIfChanged(force, &compErr);
    if (blurReloaded || compReloaded) {
        cacheUniforms();
    }
    if (errorOut && (!blurErr.empty() || !compErr.empty())) {
        *errorOut = !blurErr.empty() ? blurErr : compErr;
    }
    return blurReloaded || compReloaded;
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
    m_quad.bind();
    glUseProgram(m_blurProgram);
    glActiveTexture(GL_TEXTURE0);

    GLuint inputTexture = sourceTexture;
    constexpr int BlurPasses = 8;
    for (int pass = 0; pass < BlurPasses; ++pass) {
        Framebuffer& destination = (pass % 2 == 0) ? m_ping : m_pong;
        destination.bind();

        glBindTexture(GL_TEXTURE_2D, inputTexture);
        glUniform2f(
            m_blurDirectionLoc,
            pass % 2 == 0 ? 1.0f : 0.0f,
            pass % 2 == 0 ? 0.0f : 1.0f);
        if (pass == 0 || pass == 1) {
            glUniform1i(m_blurPrefilterLoc, pass == 0 ? 1 : 0);
        }
        glDrawArrays(GL_TRIANGLES, 0, 6);
        inputTexture = destination.texture();
    }

    glBindFramebuffer(GL_FRAMEBUFFER, targetFramebuffer);
    glViewport(0, 0, width, height);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    glUseProgram(m_compositeProgram);
    glBindTexture(GL_TEXTURE_2D, inputTexture);
    glUniform1f(m_compStrengthLoc, strength);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(0);
    glBindVertexArray(0);
}
