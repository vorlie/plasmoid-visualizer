#include "OverlayPreset.hpp"

#include "Visualizer.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {
void drawCentered(
    Visualizer& visualizer,
    const std::string& text,
    float centerX,
    float y,
    float scale,
    const float color[4],
    VisualizerFont font) {
    visualizer.drawText(
        text, centerX - visualizer.measureTextWidth(text, scale, font) * 0.5f,
        y, scale, color, font);
}
} // namespace

void OverlayPresetRenderer::renderSpectrum(
    Visualizer& visualizer,
    const std::vector<float>& fft,
    float baselineY,
    float direction,
    float maxHeight,
    float gain,
    float requestedAmplitudeCap,
    int requestedBarCount,
    float requestedBarWidth,
    float lineThickness,
    const float color[4]) const {
    const int barCount = std::clamp(requestedBarCount, 8, 512);
    constexpr float left = -0.94f;
    constexpr float right = 0.94f;
    const float cell = (right - left) / static_cast<float>(barCount);
    const float halfLineThickness = std::clamp(lineThickness, 0.0005f, 0.02f);

    m_spectrumVertices.clear();
    const size_t maxFloats = static_cast<size_t>(barCount + 1) * 30;
    if (m_spectrumVertices.capacity() < maxFloats) {
        m_spectrumVertices.reserve(maxFloats);
    }

    auto appendRect = [&](float cx, float cy, float hw, float hh) {
        const float x0 = cx - hw;
        const float x1 = cx + hw;
        const float y0 = cy - hh;
        const float y1 = cy + hh;
        m_spectrumVertices.push_back(x0); m_spectrumVertices.push_back(y1); m_spectrumVertices.push_back(0.0f); m_spectrumVertices.push_back(0.0f); m_spectrumVertices.push_back(1.0f);
        m_spectrumVertices.push_back(x0); m_spectrumVertices.push_back(y0); m_spectrumVertices.push_back(0.0f); m_spectrumVertices.push_back(0.0f); m_spectrumVertices.push_back(1.0f);
        m_spectrumVertices.push_back(x1); m_spectrumVertices.push_back(y0); m_spectrumVertices.push_back(0.0f); m_spectrumVertices.push_back(0.0f); m_spectrumVertices.push_back(1.0f);

        m_spectrumVertices.push_back(x0); m_spectrumVertices.push_back(y1); m_spectrumVertices.push_back(0.0f); m_spectrumVertices.push_back(0.0f); m_spectrumVertices.push_back(1.0f);
        m_spectrumVertices.push_back(x1); m_spectrumVertices.push_back(y0); m_spectrumVertices.push_back(0.0f); m_spectrumVertices.push_back(0.0f); m_spectrumVertices.push_back(1.0f);
        m_spectrumVertices.push_back(x1); m_spectrumVertices.push_back(y1); m_spectrumVertices.push_back(0.0f); m_spectrumVertices.push_back(0.0f); m_spectrumVertices.push_back(1.0f);
    };

    appendRect(0.0f, baselineY, 0.94f, halfLineThickness);
    if (!fft.empty()) {
        const float amplitudeCap = std::clamp(requestedAmplitudeCap, 0.05f, 8.0f);
        const float halfBarWidth = cell * 0.5f * std::clamp(requestedBarWidth, 0.05f, 1.0f);
        for (int i = 0; i < barCount; ++i) {
            const size_t begin = static_cast<size_t>(i) * fft.size() / barCount;
            const size_t finish = std::max(begin + 1, static_cast<size_t>(i + 1) * fft.size() / barCount);
            float magnitude = 0.0f;
            for (size_t j = begin; j < std::min(finish, fft.size()); ++j) {
                magnitude = std::max(magnitude, fft[j]);
            }
            const float height =
                std::clamp(magnitude * gain, 0.0f, amplitudeCap) * maxHeight;
            if (height < 0.002f) continue;
            const float x = left + (static_cast<float>(i) + 0.5f) * cell;
            const float y = baselineY + direction * height;
            appendRect(x, y, halfBarWidth, height);
        }
    }

    visualizer.drawSolidRects(m_spectrumVertices, color);
}

void OverlayPresetRenderer::render(
    Visualizer& visualizer,
    const OverlayPresetStyle& style,
    const OverlayFrameData& frame) const {
    const float white[4] = {1.0f, 1.0f, 1.0f, std::clamp(style.opacity, 0.0f, 1.0f)};
    const float topY = 1.0f - style.topMargin;
    const float topBaseline = topY - 0.17f;
    const float bottomBaseline = -1.0f + style.bottomMargin + 0.14f;
    const float resolutionScale =
        static_cast<float>(std::max(visualizer.viewportHeight(), 1)) / 1080.0f;
    const std::vector<float> empty;
    const auto& fft = frame.fft ? *frame.fft : empty;

    // The logo slot is intentionally omitted. Metadata starts at the left margin.
    visualizer.drawText(
        frame.artist, -0.94f, topY - 0.028f,
        style.artistTextScale * resolutionScale, white);
    visualizer.drawText(
        frame.title, -0.94f, topY - 0.095f,
        style.titleTextScale * resolutionScale, white);

    const int totalSeconds = std::max(0, static_cast<int>(frame.timestampSeconds));
    char time[16]{};
    std::snprintf(time, sizeof(time), "%d:%02d", totalSeconds / 60, totalSeconds % 60);
    const std::string timeText(time);
    visualizer.drawText(
        timeText,
        0.94f - visualizer.measureTextWidth(
            timeText, style.timestampTextScale * resolutionScale),
        topY - 0.095f, style.timestampTextScale * resolutionScale, white);

    if (style.showTopSpectrum) {
        renderSpectrum(
            visualizer, fft, topBaseline, -1.0f, style.topSpectrumHeight,
            style.spectrumGain, style.spectrumAmplitudeCap,
            style.spectrumBarCount, style.spectrumBarWidth,
            style.spectrumLineThickness, white);
    }
    if (style.blurredLyricsBand && frame.blurredSceneTexture != 0) {
        const float halfHeight = std::clamp(style.lyricsBandHeight, 0.02f, 0.45f);
        const float centerY = -1.0f + halfHeight;
        visualizer.drawTextureRegion(
            frame.blurredSceneTexture,
            0.0f, centerY, 1.0f, halfHeight,
            0.0f, 0.0f, 1.0f, halfHeight,
            std::clamp(style.lyricsBandOpacity, 0.0f, 1.0f));
    }
    if (style.showBottomSpectrum) {
        renderSpectrum(
            visualizer, fft, bottomBaseline, 1.0f, style.bottomSpectrumHeight,
            style.spectrumGain, style.spectrumAmplitudeCap,
            style.spectrumBarCount, style.spectrumBarWidth,
            style.spectrumLineThickness, white);
    }

    std::string lyric = frame.previewLyric;
    if (lyric.empty() && frame.lyrics) {
        if (const auto* active = frame.lyrics->activeEvent(frame.timestampSeconds)) lyric = active->text;
    }
    if (!lyric.empty()) {
        drawCentered(
            visualizer, lyric, 0.0f, bottomBaseline - 0.09f,
            style.lyricsTextScale * resolutionScale,
            white, VisualizerFont::Lyrics);
    }
}
