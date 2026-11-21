#include "BloomRenderer.hpp"

#include "AssetPaths.hpp"

#include <algorithm>

void BloomRenderer::cacheUniforms() noexcept {
    m_downPrefilterLoc = glGetUniformLocation(m_downsampleProgram, "uPrefilter");
    m_downThresholdLoc = glGetUniformLocation(m_downsampleProgram, "uThreshold");
    m_downSoftKneeLoc = glGetUniformLocation(m_downsampleProgram, "uSoftKnee");
    m_upRadiusLoc = glGetUniformLocation(m_upsampleProgram, "uFilterRadius");
    m_compStrengthLoc = glGetUniformLocation(m_compositeProgram, "uStrength");
    m_compUseBloomLoc = glGetUniformLocation(m_compositeProgram, "uUseBloom");

    glUseProgram(m_downsampleProgram);
    glUniform1i(glGetUniformLocation(m_downsampleProgram, "uTexture"), 0);
    glUniform1f(m_downThresholdLoc, 0.85f);
    glUniform1f(m_downSoftKneeLoc, 0.50f);

    glUseProgram(m_upsampleProgram);
    glUniform1i(glGetUniformLocation(m_upsampleProgram, "uTexture"), 0);
    glUniform1f(m_upRadiusLoc, 1.25f);

    glUseProgram(m_compositeProgram);
    glUniform1i(glGetUniformLocation(m_compositeProgram, "uScene"), 0);
    glUniform1i(glGetUniformLocation(m_compositeProgram, "uBloom"), 1);
    glUseProgram(0);
}

void BloomRenderer::initialize() {
    if (m_initialized) return;

    m_downsampleProgram.load(
        AssetPaths::shader("quad.vert"),
        AssetPaths::shader("bloom_downsample.frag"));
    m_upsampleProgram.load(
        AssetPaths::shader("quad.vert"),
        AssetPaths::shader("bloom_upsample.frag"));
    m_compositeProgram.load(
        AssetPaths::shader("quad.vert"),
        AssetPaths::shader("composite.frag"));
    cacheUniforms();
    m_quad.initialize();
    m_initialized = true;
}

bool BloomRenderer::reloadShaders(bool force, std::string* errorOut) {
    if (!m_initialized) return false;
    std::string downErr, upErr, compErr;
    const bool downReloaded = m_downsampleProgram.reloadIfChanged(force, &downErr);
    const bool upReloaded = m_upsampleProgram.reloadIfChanged(force, &upErr);
    const bool compReloaded = m_compositeProgram.reloadIfChanged(force, &compErr);
    if (downReloaded || upReloaded || compReloaded) {
        cacheUniforms();
    }
    if (errorOut && (!downErr.empty() || !upErr.empty() || !compErr.empty())) {
        *errorOut = !downErr.empty() ? downErr : (!upErr.empty() ? upErr : compErr);
    }
    return downReloaded || upReloaded || compReloaded;
}

void BloomRenderer::composite(
    GLuint sceneTexture,
    GLuint targetFramebuffer,
    int width,
    int height,
    bool enableBloom,
    float strength
) {
    if (sceneTexture == 0 || width <= 0 || height <= 0) return;
    initialize();

    const bool runBloom = enableBloom && strength > 0.001f;

    glDisable(GL_DEPTH_TEST);
    m_quad.bind();

    if (runBloom) {
        int mipW = width;
        int mipH = height;
        for (size_t i = 0; i < kMipLevels; ++i) {
            mipW = std::max(mipW / 2, 1);
            mipH = std::max(mipH / 2, 1);
            m_mips[i].resize(mipW, mipH);
        }

        // 1. Progressive 13-tap Jimenez downsample with Karis threshold prefilter on mip 0.
        glDisable(GL_BLEND);
        glUseProgram(m_downsampleProgram);
        glActiveTexture(GL_TEXTURE0);
        GLuint srcTex = sceneTexture;
        for (size_t i = 0; i < kMipLevels; ++i) {
            m_mips[i].bind();
            glViewport(0, 0, m_mips[i].width(), m_mips[i].height());
            glBindTexture(GL_TEXTURE_2D, srcTex);
            glUniform1i(m_downPrefilterLoc, i == 0 ? 1 : 0);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            srcTex = m_mips[i].texture();
        }

        // 2. Progressive additive 3x3 tent upsample for wide CRT optical halation.
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
        glUseProgram(m_upsampleProgram);
        glUniform1f(m_upRadiusLoc, 1.25f);
        for (size_t i = kMipLevels - 1; i > 0; --i) {
            Framebuffer& dst = m_mips[i - 1];
            dst.bind();
            glViewport(0, 0, dst.width(), dst.height());
            glBindTexture(GL_TEXTURE_2D, m_mips[i].texture());
            glDrawArrays(GL_TRIANGLES, 0, 6);
        }
    }

    // 3. Unified output composite + CRT hot-white phosphor core tone mapping.
    glBindFramebuffer(GL_FRAMEBUFFER, targetFramebuffer);
    glViewport(0, 0, width, height);
    glDisable(GL_BLEND);
    glUseProgram(m_compositeProgram);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sceneTexture);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, runBloom ? m_mips[0].texture() : sceneTexture);
    glUniform1i(m_compUseBloomLoc, runBloom ? 1 : 0);
    glUniform1f(m_compStrengthLoc, std::clamp(strength, 0.0f, 5.0f) * 0.28f);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    glActiveTexture(GL_TEXTURE0);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(0);
    glBindVertexArray(0);
}
