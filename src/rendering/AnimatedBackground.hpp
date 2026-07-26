#pragma once

#include "OverlayPreset.hpp"
#include <GL/glew.h>
#include <cstdio>
#include <string>
#include <vector>

class AnimatedBackground {
public:
    ~AnimatedBackground();
    AnimatedBackground() = default;
    AnimatedBackground(const AnimatedBackground&) = delete;
    AnimatedBackground& operator=(const AnimatedBackground&) = delete;

    GLuint update(
        OverlayBackgroundType type,
        OverlayBackgroundFit fit,
        const std::string& path,
        float timestampSeconds,
        int outputWidth,
        int outputHeight);
    void reset();
    const std::string& error() const noexcept { return m_error; }
    int width() const noexcept { return m_textureWidth; }
    int height() const noexcept { return m_textureHeight; }

private:
    bool loadStill(const std::string& path);
    bool loadGif(const std::string& path);
    bool startVideo(
        const std::string& path, OverlayBackgroundFit fit,
        float timestampSeconds, int width, int height);
    bool readVideoFrame();
    void upload(const unsigned char* pixels, int width, int height);
    void stopVideo();

    GLuint m_texture = 0;
    int m_textureWidth = 0;
    int m_textureHeight = 0;
    OverlayBackgroundType m_type = OverlayBackgroundType::None;
    std::string m_path;
    std::string m_error;

    std::vector<unsigned char> m_gifFrames;
    std::vector<int> m_gifDelays;
    int m_gifWidth = 0;
    int m_gifHeight = 0;
    int m_gifFrameCount = 0;

    FILE* m_videoPipe = nullptr;
    std::vector<unsigned char> m_videoFrame;
    int m_videoWidth = 0;
    int m_videoHeight = 0;
    int m_videoFrameIndex = -1;
    float m_videoStartTimestamp = 0.0f;
    OverlayBackgroundFit m_videoFit = OverlayBackgroundFit::Cover;
};
