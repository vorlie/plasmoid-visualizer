#pragma once

#include "GlResources.hpp"
#include <cstdint>
#include <filesystem>
#include <memory>

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
    FontAtlas();
    ~FontAtlas();

    FontAtlas(const FontAtlas&) = delete;
    FontAtlas& operator=(const FontAtlas&) = delete;
    FontAtlas(FontAtlas&&) noexcept = default;
    FontAtlas& operator=(FontAtlas&&) noexcept = default;

    bool load(const std::filesystem::path& path, float pixelSize);
    void reset() noexcept;

    [[nodiscard]] bool loaded() const noexcept { return m_texture.valid(); }
    [[nodiscard]] GLuint texture() const noexcept { return m_texture.get(); }
    const GlyphInfo* glyph(std::uint32_t codepoint);
    [[nodiscard]] int atlasSize() const noexcept;

private:
    struct Impl;
    gl::Texture m_texture;
    std::unique_ptr<Impl> m_impl;
};
