#pragma once

#include "Framebuffer.hpp"
#include "FullscreenQuad.hpp"
#include "ShaderProgram.hpp"

#include <array>
#include <string>

class BloomRenderer {
public:
    BloomRenderer() = default;
    ~BloomRenderer() = default;

    BloomRenderer(const BloomRenderer&) = delete;
    BloomRenderer& operator=(const BloomRenderer&) = delete;
    BloomRenderer(BloomRenderer&&) = delete;
    BloomRenderer& operator=(BloomRenderer&&) = delete;

    // Unified output pass: runs 4-level mip pyramid CRT bloom (when enableBloom is true)
    // and presents the HDR scene + bloom with CRT hot-core tone mapping into targetFramebuffer.
    void composite(
        GLuint sceneTexture,
        GLuint targetFramebuffer,
        int width,
        int height,
        bool enableBloom,
        float strength = 0.8f);

    bool reloadShaders(bool force = false, std::string* errorOut = nullptr);

private:
    void initialize();
    void cacheUniforms() noexcept;

    ShaderProgram m_downsampleProgram;
    ShaderProgram m_upsampleProgram;
    ShaderProgram m_compositeProgram;

    GLint m_downPrefilterLoc = -1;
    GLint m_downThresholdLoc = -1;
    GLint m_downSoftKneeLoc = -1;
    GLint m_upRadiusLoc = -1;
    GLint m_compStrengthLoc = -1;
    GLint m_compUseBloomLoc = -1;

    static constexpr size_t kMipLevels = 4;
    std::array<Framebuffer, kMipLevels> m_mips;
    FullscreenQuad m_quad;
    bool m_initialized = false;
};
