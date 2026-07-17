#pragma once

#include <GL/glew.h>
#include <array>
#include <filesystem>

struct GlyphInfo {
    float advanceX = 0.0f;
    float bearingLeft = 0.0f;
    float bearingTop = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    float textureX = 0.0f;
    float textureY = 0.0f;
};

class FontAtlas {
public:
    FontAtlas() = default;
    ~FontAtlas();

    FontAtlas(const FontAtlas&) = delete;
    FontAtlas& operator=(const FontAtlas&) = delete;

    bool load(const std::filesystem::path& path, float pixelSize);
    void reset() noexcept;

    [[nodiscard]] bool loaded() const noexcept { return m_texture != 0; }
    [[nodiscard]] GLuint texture() const noexcept { return m_texture; }
    [[nodiscard]] const GlyphInfo& glyph(unsigned char character) const noexcept {
        return m_glyphs[character];
    }

private:
    GLuint m_texture = 0;
    std::array<GlyphInfo, 128> m_glyphs{};
};
