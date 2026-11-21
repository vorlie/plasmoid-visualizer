#include "TextRenderer.hpp"
#include "AssetPaths.hpp"
#include "Utf8Paths.hpp"

namespace {
void decodeUtf8Into(const std::string& text, std::vector<std::uint32_t>& result) {
    result.clear();
    if (result.capacity() < text.size()) {
        result.reserve(text.size());
    }
    for (size_t i = 0; i < text.size();) {
        const auto first = static_cast<unsigned char>(text[i]);
        std::uint32_t codepoint = 0;
        size_t length = 1;
        if (first < 0x80) {
            codepoint = first;
        } else if ((first & 0xE0) == 0xC0) {
            codepoint = first & 0x1F;
            length = 2;
        } else if ((first & 0xF0) == 0xE0) {
            codepoint = first & 0x0F;
            length = 3;
        } else if ((first & 0xF8) == 0xF0) {
            codepoint = first & 0x07;
            length = 4;
        } else {
            result.push_back(0xFFFD);
            ++i;
            continue;
        }
        if (i + length > text.size()) {
            result.push_back(0xFFFD);
            break;
        }
        bool valid = true;
        for (size_t j = 1; j < length; ++j) {
            const auto continuation = static_cast<unsigned char>(text[i + j]);
            if ((continuation & 0xC0) != 0x80) {
                valid = false;
                break;
            }
            codepoint = (codepoint << 6) | (continuation & 0x3F);
        }
        result.push_back(valid ? codepoint : 0xFFFD);
        i += valid ? length : 1;
    }
}
} // namespace

void TextRenderer::initialize() {
    const auto defaultFont = AssetPaths::defaultFont();
    if (!defaultFont.empty()) {
        const std::string defaultFontUtf8 = Utf8Paths::toUtf8(defaultFont);
        loadFont(defaultFontUtf8);
        loadLyricsFont(defaultFontUtf8);
    }

    m_vertices.initialize(4, 64 * 1024);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(2 * sizeof(float)));
    m_vertices.finishLayout();
}

bool TextRenderer::loadFont(const std::string& path) {
    return m_fontAtlas.load(Utf8Paths::fromUtf8(path), 48.0f);
}

bool TextRenderer::loadLyricsFont(const std::string& path) {
    return m_lyricsFontAtlas.load(Utf8Paths::fromUtf8(path), 48.0f);
}

float TextRenderer::measureTextWidth(
    const std::string& text,
    float scale,
    int viewportWidth,
    VisualizerFont font
) {
    FontAtlas& atlas = font == VisualizerFont::Lyrics ? m_lyricsFontAtlas : m_fontAtlas;
    if (!atlas.loaded() || viewportWidth <= 0) return 0.0f;
    decodeUtf8Into(text, m_codepointsScratch);
    const float invVpW = 2.0f * scale / static_cast<float>(viewportWidth);
    float width = 0.0f;
    for (const std::uint32_t codepoint : m_codepointsScratch) {
        if (const GlyphInfo* glyph = atlas.glyph(codepoint)) {
            width += glyph->advanceX * invVpW;
        }
    }
    return width;
}

void TextRenderer::drawText(
    const QuadRenderer& quadRenderer,
    const std::string& text,
    float x,
    float y,
    float scale,
    int viewportWidth,
    int viewportHeight,
    const float color[4],
    VisualizerFont font
) {
    FontAtlas& atlas = font == VisualizerFont::Lyrics ? m_lyricsFontAtlas : m_fontAtlas;
    if (!atlas.loaded() || viewportWidth <= 0 || viewportHeight <= 0 || text.empty()) return;

    decodeUtf8Into(text, m_codepointsScratch);
    m_textVertexScratch.clear();
    const size_t expectedFloats = m_codepointsScratch.size() * 24;
    if (m_textVertexScratch.capacity() < expectedFloats) {
        m_textVertexScratch.reserve(expectedFloats);
    }

    const float invVpW = scale / static_cast<float>(viewportWidth);
    const float invVpH = scale / static_cast<float>(viewportHeight);
    const float invAtlas = 1.0f / static_cast<float>(atlas.atlasSize());

    float curX = x;
    for (const std::uint32_t codepoint : m_codepointsScratch) {
        if (codepoint < 32) continue;
        const GlyphInfo* glyph = atlas.glyph(codepoint);
        if (!glyph) continue;
        const GlyphInfo& ch = *glyph;

        if (ch.width > 0.0f && ch.height > 0.0f) {
            const float w = ch.width * invVpW;
            const float h = ch.height * invVpH;
            const float leftOffset = 2.0f * ch.bearingLeft * invVpW;
            const float topOffset = -2.0f * ch.bearingTop * invVpH;

            const float x0 = curX + leftOffset;
            const float x1 = x0 + 2.0f * w;
            const float y1 = y + topOffset;
            const float y0 = y1 - 2.0f * h;

            const float u0 = ch.textureX;
            const float v0 = ch.textureY;
            const float u1 = u0 + ch.width * invAtlas;
            const float v1 = v0 + ch.height * invAtlas;

            m_textVertexScratch.push_back(x0); m_textVertexScratch.push_back(y1); m_textVertexScratch.push_back(u0); m_textVertexScratch.push_back(v1);
            m_textVertexScratch.push_back(x0); m_textVertexScratch.push_back(y0); m_textVertexScratch.push_back(u0); m_textVertexScratch.push_back(v0);
            m_textVertexScratch.push_back(x1); m_textVertexScratch.push_back(y0); m_textVertexScratch.push_back(u1); m_textVertexScratch.push_back(v0);

            m_textVertexScratch.push_back(x0); m_textVertexScratch.push_back(y1); m_textVertexScratch.push_back(u0); m_textVertexScratch.push_back(v1);
            m_textVertexScratch.push_back(x1); m_textVertexScratch.push_back(y0); m_textVertexScratch.push_back(u1); m_textVertexScratch.push_back(v0);
            m_textVertexScratch.push_back(x1); m_textVertexScratch.push_back(y1); m_textVertexScratch.push_back(u1); m_textVertexScratch.push_back(v1);
        }

        curX += 2.0f * ch.advanceX * invVpW;
    }

    if (m_textVertexScratch.empty()) return;

    const GLint firstVertex = m_vertices.upload(m_textVertexScratch);
    quadRenderer.drawFontTriangles(
        atlas.texture(),
        color,
        firstVertex,
        static_cast<GLsizei>(m_textVertexScratch.size() / 4));
}
