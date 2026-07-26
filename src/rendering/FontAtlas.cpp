#include "FontAtlas.hpp"

#include "stb_truetype.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <vector>

namespace {
constexpr int AtlasSize = 2048;
}

struct FontAtlas::Impl {
    std::vector<unsigned char> fontData;
    stbtt_fontinfo font{};
    float scale = 1.0f;
    int cursorX = 1;
    int cursorY = 1;
    int rowHeight = 0;
    std::unordered_map<std::uint32_t, GlyphInfo> glyphs;
};

FontAtlas::FontAtlas() = default;

FontAtlas::~FontAtlas() {
    reset();
}

bool FontAtlas::load(const std::filesystem::path& path, float pixelSize) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        std::cerr << "Failed to open font file: " << path.string() << std::endl;
        return false;
    }

    const auto fileSize = file.tellg();
    if (fileSize <= 0) return false;
    file.seekg(0, std::ios::beg);

    auto next = std::make_unique<Impl>();
    next->fontData.resize(static_cast<size_t>(fileSize));
    file.read(reinterpret_cast<char*>(next->fontData.data()), fileSize);
    const int offset = stbtt_GetFontOffsetForIndex(next->fontData.data(), 0);
    if (offset < 0 || !stbtt_InitFont(&next->font, next->fontData.data(), offset)) {
        std::cerr << "Failed to initialize font: " << path.string() << std::endl;
        return false;
    }
    next->scale = stbtt_ScaleForPixelHeight(&next->font, pixelSize);

    std::vector<unsigned char> blank(static_cast<size_t>(AtlasSize) * AtlasSize, 0);
    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(
        GL_TEXTURE_2D, 0, GL_RED, AtlasSize, AtlasSize, 0,
        GL_RED, GL_UNSIGNED_BYTE, blank.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    reset();
    m_texture = texture;
    m_impl = std::move(next);
    return true;
}

const GlyphInfo* FontAtlas::glyph(std::uint32_t codepoint) {
    if (!m_impl || m_texture == 0) return nullptr;
    if (const auto found = m_impl->glyphs.find(codepoint); found != m_impl->glyphs.end()) {
        return &found->second;
    }

    int advance = 0;
    int leftBearing = 0;
    stbtt_GetCodepointHMetrics(
        &m_impl->font, static_cast<int>(codepoint), &advance, &leftBearing);
    int x0 = 0;
    int y0 = 0;
    int x1 = 0;
    int y1 = 0;
    stbtt_GetCodepointBitmapBox(
        &m_impl->font, static_cast<int>(codepoint),
        m_impl->scale, m_impl->scale, &x0, &y0, &x1, &y1);

    const int width = std::max(x1 - x0, 0);
    const int height = std::max(y1 - y0, 0);
    GlyphInfo info;
    info.advanceX = advance * m_impl->scale;
    info.bearingLeft = static_cast<float>(x0);
    info.bearingTop = static_cast<float>(y0);
    info.width = static_cast<float>(width);
    info.height = static_cast<float>(height);

    if (width > 0 && height > 0) {
        if (m_impl->cursorX + width + 1 >= AtlasSize) {
            m_impl->cursorX = 1;
            m_impl->cursorY += m_impl->rowHeight + 1;
            m_impl->rowHeight = 0;
        }
        if (m_impl->cursorY + height + 1 >= AtlasSize) {
            std::cerr << "Dynamic font atlas is full; cannot cache U+"
                      << std::hex << codepoint << std::dec << std::endl;
            return nullptr;
        }

        std::vector<unsigned char> bitmap(static_cast<size_t>(width) * height);
        stbtt_MakeCodepointBitmap(
            &m_impl->font, bitmap.data(), width, height, width,
            m_impl->scale, m_impl->scale, static_cast<int>(codepoint));
        for (int y = 0; y < height / 2; ++y) {
            const int opposite = height - y - 1;
            for (int x = 0; x < width; ++x) {
                std::swap(
                    bitmap[static_cast<size_t>(y) * width + x],
                    bitmap[static_cast<size_t>(opposite) * width + x]);
            }
        }

        glBindTexture(GL_TEXTURE_2D, m_texture);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexSubImage2D(
            GL_TEXTURE_2D, 0, m_impl->cursorX, m_impl->cursorY,
            width, height, GL_RED, GL_UNSIGNED_BYTE, bitmap.data());
        info.textureX = m_impl->cursorX / static_cast<float>(AtlasSize);
        info.textureY = m_impl->cursorY / static_cast<float>(AtlasSize);
        m_impl->cursorX += width + 1;
        m_impl->rowHeight = std::max(m_impl->rowHeight, height);
    }

    const auto inserted = m_impl->glyphs.emplace(codepoint, info);
    return &inserted.first->second;
}

int FontAtlas::atlasSize() const noexcept {
    return AtlasSize;
}

void FontAtlas::reset() noexcept {
    if (m_texture != 0) glDeleteTextures(1, &m_texture);
    m_texture = 0;
    m_impl.reset();
}
