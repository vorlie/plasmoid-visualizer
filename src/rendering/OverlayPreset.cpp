#include "OverlayPreset.hpp"

#include "Visualizer.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <regex>
#include <sstream>

namespace {
std::string trim(std::string value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

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
}

void LyricsTimeline::sortAndNormalize() {
    events.erase(
        std::remove_if(events.begin(), events.end(), [](const LyricEvent& event) {
            return event.text.empty();
        }),
        events.end());
    std::stable_sort(events.begin(), events.end(), [](const LyricEvent& a, const LyricEvent& b) {
        return a.startSeconds < b.startSeconds;
    });
    for (size_t i = 0; i < events.size(); ++i) {
        const float nextStart = i + 1 < events.size()
            ? events[i + 1].startSeconds
            : events[i].startSeconds + 5.0f;
        if (events[i].endSeconds <= events[i].startSeconds) {
            events[i].endSeconds = nextStart;
        }
        events[i].endSeconds = std::max(events[i].endSeconds, events[i].startSeconds + 0.05f);
    }
}

const LyricEvent* LyricsTimeline::activeEvent(float timeSeconds) const {
    const LyricEvent* result = nullptr;
    for (const auto& event : events) {
        if (event.startSeconds > timeSeconds) break;
        if (timeSeconds < event.endSeconds) result = &event;
    }
    return result;
}

bool LyricsTimeline::parseLrc(const std::string& source, std::string* error) {
    static const std::regex timeTag(R"(\[(\d{1,3}):(\d{2})(?:[.:](\d{1,3}))?\])");
    std::vector<LyricEvent> parsed;
    std::istringstream lines(source);
    std::string line;
    int lineNumber = 0;

    while (std::getline(lines, line)) {
        ++lineNumber;
        std::sregex_iterator match(line.begin(), line.end(), timeTag);
        const std::sregex_iterator end;
        if (match == end) {
            if (!trim(line).empty() && error) {
                *error = "Line " + std::to_string(lineNumber) + " has no [mm:ss.xx] timestamp.";
            }
            continue;
        }

        size_t textStart = 0;
        for (auto cursor = match; cursor != end; ++cursor) {
            textStart = static_cast<size_t>(cursor->position() + cursor->length());
        }
        const std::string lyric = trim(line.substr(textStart));
        for (; match != end; ++match) {
            const float minutes = std::stof((*match)[1].str());
            const float seconds = std::stof((*match)[2].str());
            float fraction = 0.0f;
            if ((*match)[3].matched) {
                const std::string digits = (*match)[3].str();
                fraction = std::stof(digits) / std::pow(10.0f, static_cast<float>(digits.size()));
            }
            parsed.push_back({minutes * 60.0f + seconds + fraction, 0.0f, lyric});
        }
    }

    if (parsed.empty()) {
        if (error) *error = "No timestamped lyric lines were found.";
        return false;
    }
    events = std::move(parsed);
    sortAndNormalize();
    if (error) error->clear();
    return true;
}

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

    visualizer.drawRoundedRect(
        0.0f, baselineY, 0.94f,
        std::clamp(lineThickness, 0.0005f, 0.02f), 0.0f, color);
    if (fft.empty()) return;

    for (int i = 0; i < barCount; ++i) {
        const size_t begin = static_cast<size_t>(i) * fft.size() / barCount;
        const size_t finish = std::max(begin + 1, static_cast<size_t>(i + 1) * fft.size() / barCount);
        float magnitude = 0.0f;
        for (size_t j = begin; j < std::min(finish, fft.size()); ++j) {
            magnitude = std::max(magnitude, fft[j]);
        }
        const float amplitudeCap = std::clamp(requestedAmplitudeCap, 0.05f, 8.0f);
        const float height =
            std::clamp(magnitude * gain, 0.0f, amplitudeCap) * maxHeight;
        if (height < 0.002f) continue;
        const float x = left + (static_cast<float>(i) + 0.5f) * cell;
        const float y = baselineY + direction * height;
        visualizer.drawRoundedRect(
            x, y, cell * 0.5f * std::clamp(requestedBarWidth, 0.05f, 1.0f),
            height, 0.0f, color);
    }
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
