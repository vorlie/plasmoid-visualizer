#include "AnimatedBackground.hpp"

#include "stb_image.h"
#include "Utf8Paths.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>

namespace {
void flipRowsInPlace(unsigned char* pixels, int width, int height, std::vector<unsigned char>& scratchRow) {
    const size_t stride = static_cast<size_t>(width) * 4;
    if (scratchRow.size() < stride) scratchRow.resize(stride);
    for (int y = 0; y < height / 2; ++y) {
        unsigned char* top = pixels + static_cast<size_t>(y) * stride;
        unsigned char* bottom = pixels + static_cast<size_t>(height - y - 1) * stride;
        std::copy(top, top + stride, scratchRow.data());
        std::copy(bottom, bottom + stride, top);
        std::copy(scratchRow.data(), scratchRow.data() + stride, bottom);
    }
}
}

AnimatedBackground::~AnimatedBackground() {
    reset();
}

void AnimatedBackground::upload(const unsigned char* pixels, int width, int height) {
    glBindTexture(GL_TEXTURE_2D, m_texture.ensureCreated());
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    if (width != m_textureWidth || height != m_textureHeight) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        m_textureWidth = width;
        m_textureHeight = height;
    } else {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    }
}

bool AnimatedBackground::loadStill(const std::string& path) {
    std::ifstream input(Utf8Paths::fromUtf8(path), std::ios::binary);
    const std::vector<unsigned char> bytes{
        std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    if (bytes.empty()) {
        m_error = "Could not read background image.";
        return false;
    }
    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* pixels = stbi_load_from_memory(
        bytes.data(), static_cast<int>(bytes.size()),
        &width, &height, &channels, 4);
    if (!pixels) {
        m_error = "Could not decode background image.";
        return false;
    }
    upload(pixels, width, height);
    stbi_image_free(pixels);
    return true;
}

bool AnimatedBackground::loadGif(const std::string& path) {
    std::ifstream input(Utf8Paths::fromUtf8(path), std::ios::binary);
    const std::vector<unsigned char> bytes{
        std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    if (bytes.empty()) {
        m_error = "Could not read animated GIF.";
        return false;
    }

    int* delays = nullptr;
    int channels = 0;
    stbi_set_flip_vertically_on_load(false);
    unsigned char* frames = stbi_load_gif_from_memory(
        bytes.data(), static_cast<int>(bytes.size()), &delays,
        &m_gifWidth, &m_gifHeight, &m_gifFrameCount, &channels, 4);
    if (!frames || m_gifFrameCount <= 0) {
        if (frames) stbi_image_free(frames);
        if (delays) stbi_image_free(delays);
        m_error = "Could not decode animated GIF.";
        return false;
    }

    const size_t frameBytes = static_cast<size_t>(m_gifWidth) * m_gifHeight * 4;
    m_gifFrames.assign(frames, frames + frameBytes * m_gifFrameCount);
    m_gifDelays.assign(delays, delays + m_gifFrameCount);
    stbi_image_free(frames);
    stbi_image_free(delays);
    m_gifTotalDelay = 0;
    for (const int delay : m_gifDelays) {
        m_gifTotalDelay += std::max(delay, 10);
    }
    m_lastGifFrame = -1;
    std::vector<unsigned char> scratchRow;
    for (int frame = 0; frame < m_gifFrameCount; ++frame) {
        flipRowsInPlace(
            m_gifFrames.data() + frameBytes * static_cast<size_t>(frame),
            m_gifWidth, m_gifHeight, scratchRow);
    }
    return true;
}

void AnimatedBackground::stopVideo() {
    m_stopVideoThread.store(true, std::memory_order_release);
    // Terminate FFmpeg BEFORE joining so a reader thread blocked in readExact()
    // receives EOF/broken pipe immediately instead of hanging on -stream_loop -1.
    m_videoProcess.terminateChild();
    if (m_videoThread.joinable()) m_videoThread.join();
    m_videoProcess.closeAndWait();
    m_stopVideoThread.store(false, std::memory_order_release);
}

bool AnimatedBackground::startVideo(
    const std::string& path,
    OverlayBackgroundFit fit,
    float timestampSeconds,
    int width,
    int height,
    bool realtime,
    OverlayVideoDecoder decoder,
    int videoFps
) {
    stopVideo();
    m_videoWidth = std::max(width, 1);
    m_videoHeight = std::max(height, 1);
    const size_t frameBytes = static_cast<size_t>(m_videoWidth) * static_cast<size_t>(m_videoHeight) * 4;
    m_videoFrame.assign(frameBytes, 0);
    m_videoFrameIndex = -1;
    m_videoStartTimestamp = std::max(timestampSeconds, 0.0f);
    m_videoFit = fit;
    m_videoRealtime = realtime;
    m_videoDecoder = decoder;
    m_videoFps = std::clamp(videoFps, 1, 60);
    {
        std::lock_guard<std::mutex> lock(m_videoFrameMutex);
        m_pendingVideoFrame.clear();
        m_pendingVideoFrameReady = false;
    }

    std::string videoFilter;
    if (fit == OverlayBackgroundFit::Contain) {
        videoFilter =
            "fps=" + std::to_string(m_videoFps) + ",scale=" + std::to_string(m_videoWidth) + ":" +
            std::to_string(m_videoHeight) +
            ":force_original_aspect_ratio=decrease,pad=" +
            std::to_string(m_videoWidth) + ":" + std::to_string(m_videoHeight) +
            ":(ow-iw)/2:(oh-ih)/2,vflip";
    } else if (fit == OverlayBackgroundFit::Stretch) {
        videoFilter =
            "fps=" + std::to_string(m_videoFps) + ",scale=" + std::to_string(m_videoWidth) + ":" +
            std::to_string(m_videoHeight) + ",vflip";
    } else {
        videoFilter =
            "fps=" + std::to_string(m_videoFps) + ",scale=" + std::to_string(m_videoWidth) + ":" +
            std::to_string(m_videoHeight) +
            ":force_original_aspect_ratio=increase,crop=" +
            std::to_string(m_videoWidth) + ":" + std::to_string(m_videoHeight) + ",vflip";
    }

    std::vector<std::string> args = { "ffmpeg", "-hide_banner", "-loglevel", "error" };
#ifdef _WIN32
    if (decoder == OverlayVideoDecoder::D3D11VA) {
        args.push_back("-hwaccel");
        args.push_back("d3d11va");
    } else if (decoder == OverlayVideoDecoder::Auto) {
        args.push_back("-hwaccel");
        args.push_back("auto");
    }
#else
    if (decoder == OverlayVideoDecoder::Auto) {
        args.push_back("-hwaccel");
        args.push_back("auto");
    }
#endif
    if (realtime) {
        args.push_back("-re");
    }
    args.insert(args.end(), {
        "-stream_loop", "-1",
        "-ss", std::to_string(m_videoStartTimestamp),
        "-i", path,
        "-an",
        "-vf", videoFilter,
        "-f", "rawvideo",
        "-pix_fmt", "rgba",
        "-"
    });

    if (!m_videoProcess.start(args, SubprocessPipe::Mode::ReadStdout, /*captureStderr=*/false)) {
        m_error = "Could not start FFmpeg video decoder.";
        return false;
    }
    if (realtime) {
        m_stopVideoThread.store(false, std::memory_order_release);
        m_videoThread = std::thread(&AnimatedBackground::videoReaderLoop, this);
    }
    return true;
}

bool AnimatedBackground::readVideoFrame() {
    if (!m_videoProcess.isOpen() || m_videoFrame.empty()) return false;
    if (!m_videoProcess.readExact(m_videoFrame.data(), m_videoFrame.size())) {
        m_error = "FFmpeg stopped while decoding the looped background.";
        m_videoProcess.closeAndWait();
        return false;
    }
    upload(m_videoFrame.data(), m_videoWidth, m_videoHeight);
    ++m_videoFrameIndex;
    return true;
}

void AnimatedBackground::videoReaderLoop() {
    std::vector<unsigned char> frame(m_videoFrame.size());
    while (!m_stopVideoThread.load(std::memory_order_acquire)) {
        if (!m_videoProcess.readExact(frame.data(), frame.size())) break;
        std::lock_guard<std::mutex> lock(m_videoFrameMutex);
        m_pendingVideoFrame.swap(frame);
        if (frame.size() != m_pendingVideoFrame.size()) {
            frame.resize(m_pendingVideoFrame.size());
        }
        m_pendingVideoFrameReady = true;
    }
}

GLuint AnimatedBackground::update(
    OverlayBackgroundType type,
    OverlayBackgroundFit fit,
    const std::string& path,
    float timestampSeconds,
    int outputWidth,
    int outputHeight,
    bool realtime,
    OverlayVideoDecoder decoder,
    int videoFps
) {
    if (type == OverlayBackgroundType::None || path.empty()) {
        if (m_type != OverlayBackgroundType::None || m_texture.valid() || m_videoProcess.isOpen()) reset();
        return 0;
    }
    const bool sourceChanged = type != m_type || path != m_path;
    if (sourceChanged) {
        reset();
        std::error_code ec;
        if (!std::filesystem::exists(Utf8Paths::fromUtf8(path), ec) || ec) {
            m_error = "Background file does not exist.";
            return 0;
        }
        m_type = type;
        m_path = path;
        if (type == OverlayBackgroundType::StillImage && !loadStill(path)) return 0;
        if (type == OverlayBackgroundType::AnimatedGif && !loadGif(path)) return 0;
    }

    if (type == OverlayBackgroundType::AnimatedGif && m_gifFrameCount > 0) {
        int clock = static_cast<int>(std::max(timestampSeconds, 0.0f) * 1000.0f);
        clock = m_gifTotalDelay > 0 ? clock % m_gifTotalDelay : 0;
        int frame = 0;
        for (; frame + 1 < m_gifFrameCount; ++frame) {
            const int delay = std::max(m_gifDelays[frame], 10);
            if (clock < delay) break;
            clock -= delay;
        }
        if (frame != m_lastGifFrame || !m_texture.valid()) {
            const size_t frameBytes = static_cast<size_t>(m_gifWidth) * m_gifHeight * 4;
            upload(m_gifFrames.data() + frameBytes * static_cast<size_t>(frame), m_gifWidth, m_gifHeight);
            m_lastGifFrame = frame;
        }
    } else if (type == OverlayBackgroundType::LoopedVideo) {
        if (outputWidth <= 0 || outputHeight <= 0) return m_texture.get();
        const bool dimensionsChanged = outputWidth != m_videoWidth || outputHeight != m_videoHeight;
        const bool fitChanged = fit != m_videoFit;
        const bool decoderChanged = decoder != m_videoDecoder;
        const bool fpsChanged = std::clamp(videoFps, 1, 60) != m_videoFps;
        const bool modeChanged = realtime != m_videoRealtime;
        const bool movedBackwards = timestampSeconds + 0.001f < m_videoStartTimestamp;
        if (!m_videoProcess.isOpen() || dimensionsChanged || fitChanged || decoderChanged ||
            fpsChanged || modeChanged || (!realtime && movedBackwards)) {
            if (!startVideo(
                    path, fit, timestampSeconds, outputWidth, outputHeight,
                    realtime, decoder, videoFps)) return 0;
        }
        if (realtime) {
            bool hasNewFrame = false;
            {
                std::lock_guard<std::mutex> lock(m_videoFrameMutex);
                if (m_pendingVideoFrameReady) {
                    m_uploadStagingFrame.swap(m_pendingVideoFrame);
                    m_pendingVideoFrameReady = false;
                    hasNewFrame = true;
                }
            }
            if (hasNewFrame && m_uploadStagingFrame.size() ==
                    static_cast<size_t>(m_videoWidth) * static_cast<size_t>(m_videoHeight) * 4) {
                upload(m_uploadStagingFrame.data(), m_videoWidth, m_videoHeight);
            }
        } else {
            const int desiredFrame = std::max(
                0, static_cast<int>(
                    (timestampSeconds - m_videoStartTimestamp) * m_videoFps));
            while (m_videoFrameIndex < desiredFrame) {
                if (!readVideoFrame()) break;
            }
        }
    }
    return m_texture.get();
}

void AnimatedBackground::reset() {
    stopVideo();
    m_texture.reset();
    m_textureWidth = 0;
    m_textureHeight = 0;
    m_type = OverlayBackgroundType::None;
    m_path.clear();
    m_error.clear();
    m_gifFrames.clear();
    m_gifDelays.clear();
    m_gifWidth = m_gifHeight = m_gifFrameCount = 0;
    m_gifTotalDelay = 0;
    m_lastGifFrame = -1;
    m_videoFrame.clear();
    m_uploadStagingFrame.clear();
    m_videoWidth = m_videoHeight = 0;
    m_videoFrameIndex = -1;
    m_videoStartTimestamp = 0.0f;
    m_videoFit = OverlayBackgroundFit::Cover;
    m_videoDecoder = OverlayVideoDecoder::Auto;
    m_videoFps = 30;
    m_videoRealtime = false;
    {
        std::lock_guard<std::mutex> lock(m_videoFrameMutex);
        m_pendingVideoFrame.clear();
        m_pendingVideoFrameReady = false;
    }
}
