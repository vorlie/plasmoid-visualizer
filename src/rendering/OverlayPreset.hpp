#pragma once

#include "LyricsTimeline.hpp"
#include <string>
#include <vector>

class Visualizer;

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

enum class OverlayVideoDecoder {
    Auto = 0,
    Software = 1,
    D3D11VA = 2
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
    OverlayVideoDecoder videoDecoder = OverlayVideoDecoder::Auto;
    int videoFps = 30;
    std::string backgroundPath;
    std::string fontPath;
    std::string lyricsFontPath;
    std::string artist = "Alan Walker ft. K-391, Tungevaag, Mangoo";
    std::string title = "Play";
    LyricsTimeline lyrics;
    std::string lyricSource =
        "[00:00.00]Lyrics\n"
        "[00:05.00]Add timestamped lines in View > Media Overlay";
    std::string lyricFilePath = "lyrics.lrc";
    std::string lyricPreview;
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
    mutable std::vector<float> m_spectrumVertices;

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
