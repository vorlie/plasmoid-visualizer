#pragma once

#include <string>
#include <vector>

class Visualizer;

struct LyricEvent {
    float startSeconds = 0.0f;
    float endSeconds = 0.0f;
    std::string text;
};

class LyricsTimeline {
public:
    std::vector<LyricEvent> events;

    void sortAndNormalize();
    const LyricEvent* activeEvent(float timeSeconds) const;
    bool parseLrc(const std::string& source, std::string* error = nullptr);
};

struct OverlayPresetStyle {
    bool showTopSpectrum = true;
    bool showBottomSpectrum = true;
    float opacity = 1.0f;
    float spectrumGain = 1.35f;
    float spectrumAmplitudeCap = 1.0f;
    int spectrumBarCount = 96;
    float spectrumBarWidth = 0.78f;
    float spectrumLineThickness = 0.0022f;
    float topSpectrumHeight = 0.027f;
    float bottomSpectrumHeight = 0.035f;
    float topMargin = 0.06f;
    float bottomMargin = 0.07f;
    float artistTextScale = 0.62f;
    float titleTextScale = 0.76f;
    float timestampTextScale = 0.74f;
    float lyricsTextScale = 1.18f;
    bool blurredLyricsBand = true;
    float lyricsBandHeight = 0.115f;
    float lyricsBandOpacity = 0.72f;
};

enum class OverlayBackgroundType {
    None = 0,
    StillImage = 1,
    AnimatedGif = 2,
    LoopedVideo = 3
};

enum class OverlayBackgroundFit {
    Cover = 0,
    Contain = 1,
    Stretch = 2
};

struct MediaOverlayLayer {
    bool enabled = true;
    OverlayPresetStyle style;
    OverlayBackgroundType backgroundType = OverlayBackgroundType::None;
    OverlayBackgroundFit backgroundFit = OverlayBackgroundFit::Cover;
    float backgroundOpacity = 1.0f;
    float backgroundDimming = 0.15f;
    float backgroundScale = 1.0f;
    float backgroundOffsetX = 0.0f;
    float backgroundOffsetY = 0.0f;
    char backgroundPath[512] = "";
    char fontPath[512] = "";
    char lyricsFontPath[512] = "";
    char artist[256] = "Alan Walker ft. K-391, Tungevaag, Mangoo";
    char title[256] = "Play";
    LyricsTimeline lyrics;
    char lyricSource[16384] =
        "[00:00.00]Lyrics\n"
        "[00:05.00]Add timestamped lines in View > Media Overlay";
    char lyricFilePath[512] = "lyrics.lrc";
    char lyricPreview[512] = "";
};

struct OverlayFrameData {
    const std::vector<float>* fft = nullptr;
    const LyricsTimeline* lyrics = nullptr;
    float timestampSeconds = 0.0f;
    std::string artist;
    std::string title;
    std::string previewLyric;
    unsigned int blurredSceneTexture = 0;
};

class OverlayPresetRenderer {
public:
    void render(
        Visualizer& visualizer,
        const OverlayPresetStyle& style,
        const OverlayFrameData& frame) const;

private:
    void renderSpectrum(
        Visualizer& visualizer,
        const std::vector<float>& fft,
        float baselineY,
        float direction,
        float maxHeight,
        float gain,
        float amplitudeCap,
        int barCount,
        float barWidth,
        float lineThickness,
        const float color[4]) const;
};
