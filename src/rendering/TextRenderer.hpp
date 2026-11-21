#pragma once

#include "FontAtlas.hpp"
#include "QuadRenderer.hpp"
#include "StreamingVertexBuffer.hpp"

#include <cstdint>
#include <string>
#include <vector>

enum class VisualizerFont {
    Primary,
    Lyrics
};

class TextRenderer {
public:
    TextRenderer() = default;
    ~TextRenderer() = default;
    TextRenderer(const TextRenderer&) = delete;
    TextRenderer& operator=(const TextRenderer&) = delete;
    TextRenderer(TextRenderer&&) = delete;
    TextRenderer& operator=(TextRenderer&&) = delete;

    void initialize();

    bool loadFont(const std::string& path);
    bool loadLyricsFont(const std::string& path);

    [[nodiscard]] float measureTextWidth(
        const std::string& text,
        float scale,
        int viewportWidth,
        VisualizerFont font = VisualizerFont::Primary);

    void drawText(
        const QuadRenderer& quadRenderer,
        const std::string& text,
        float x,
        float y,
        float scale,
        int viewportWidth,
        int viewportHeight,
        const float color[4],
        VisualizerFont font = VisualizerFont::Primary);

private:
    FontAtlas m_fontAtlas;
    FontAtlas m_lyricsFontAtlas;
    StreamingVertexBuffer m_vertices;
    std::vector<float> m_textVertexScratch;
    std::vector<std::uint32_t> m_codepointsScratch;
};
