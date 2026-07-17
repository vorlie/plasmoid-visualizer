#include "FontAtlas.hpp"

#include "stb_truetype.h"
#include <fstream>
#include <iostream>
#include <vector>

namespace {
constexpr int AtlasSize = 1024;
}

FontAtlas::~FontAtlas() {
    reset();
}

bool FontAtlas::load(const std::filesystem::path& path, float pixelSize) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        std::cerr << "Failed to open font file: " << path.string() << std::endl;
        return false;
    }

    const auto size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<unsigned char> fontData(static_cast<size_t>(size));
    file.read(reinterpret_cast<char*>(fontData.data()), size);

    std::vector<unsigned char> bitmap(AtlasSize * AtlasSize);
    stbtt_bakedchar baked[96]{};
    if (stbtt_BakeFontBitmap(
            fontData.data(), 0, pixelSize, bitmap.data(),
            AtlasSize, AtlasSize, 32, 96, baked) <= 0) {
        std::cerr << "Failed to bake font atlas: " << path.string() << std::endl;
        return false;
    }

    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(
        GL_TEXTURE_2D, 0, GL_RED, AtlasSize, AtlasSize, 0,
        GL_RED, GL_UNSIGNED_BYTE, bitmap.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    reset();
    m_texture = texture;
    for (int character = 32; character < 128; ++character) {
        const auto& source = baked[character - 32];
        auto& glyph = m_glyphs[character];
        glyph.advanceX = source.xadvance;
        glyph.width = static_cast<float>(source.x1 - source.x0);
        glyph.height = static_cast<float>(source.y1 - source.y0);
        glyph.bearingLeft = source.xoff;
        glyph.bearingTop = source.yoff;
        glyph.textureX = source.x0 / static_cast<float>(AtlasSize);
        glyph.textureY = source.y0 / static_cast<float>(AtlasSize);
    }
    return true;
}

void FontAtlas::reset() noexcept {
    if (m_texture != 0) glDeleteTextures(1, &m_texture);
    m_texture = 0;
    m_glyphs = {};
}
