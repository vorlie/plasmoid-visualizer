#pragma once

#include "GlResources.hpp"
#include "OverlayPreset.hpp"
#include "SubprocessPipe.hpp"
#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class AnimatedBackground {
public:
    ~AnimatedBackground();
    AnimatedBackground() = default;
    AnimatedBackground(const AnimatedBackground&) = delete;
    AnimatedBackground& operator=(const AnimatedBackground&) = delete;
    AnimatedBackground(AnimatedBackground&&) = delete;
    AnimatedBackground& operator=(AnimatedBackground&&) = delete;

    GLuint update(
        OverlayBackgroundType type,
        OverlayBackgroundFit fit,
        const std::string& path,
        float timestampSeconds,
        int outputWidth,
        int outputHeight,
        bool realtime,
        OverlayVideoDecoder decoder,
        int videoFps);
    void reset();
    const std::string& error() const noexcept { return m_error; }
    int width() const noexcept { return m_textureWidth; }
    int height() const noexcept { return m_textureHeight; }

private:
    bool loadStill(const std::string& path);
    bool loadGif(const std::string& path);
    bool startVideo(
        const std::string& path, OverlayBackgroundFit fit,
        float timestampSeconds, int width, int height,
        bool realtime, OverlayVideoDecoder decoder, int videoFps);
    bool readVideoFrame();
    void videoReaderLoop();
    void upload(const unsigned char* pixels, int width, int height);
    void stopVideo();

    gl::Texture m_texture;
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
    int m_gifTotalDelay = 0;
    int m_lastGifFrame = -1;

    SubprocessPipe m_videoProcess;
    std::vector<unsigned char> m_videoFrame;
    std::vector<unsigned char> m_uploadStagingFrame;
    int m_videoWidth = 0;
    int m_videoHeight = 0;
    int m_videoFrameIndex = -1;
    float m_videoStartTimestamp = 0.0f;
    OverlayBackgroundFit m_videoFit = OverlayBackgroundFit::Cover;
    OverlayVideoDecoder m_videoDecoder = OverlayVideoDecoder::Auto;
    int m_videoFps = 30;
    bool m_videoRealtime = false;
    std::thread m_videoThread;
    std::atomic<bool> m_stopVideoThread{false};
    std::mutex m_videoFrameMutex;
    std::vector<unsigned char> m_pendingVideoFrame;
    bool m_pendingVideoFrameReady = false;
};
