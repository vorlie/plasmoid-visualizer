#include "AnimatedBackground.hpp"

#include "stb_image.h"
#include "Utf8Paths.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>

namespace {
void flipRows(std::vector<unsigned char>& pixels, int width, int height) {
    const size_t stride = static_cast<size_t>(width) * 4;
    std::vector<unsigned char> row(stride);
    for (int y = 0; y < height / 2; ++y) {
        unsigned char* top = pixels.data() + static_cast<size_t>(y) * stride;
        unsigned char* bottom = pixels.data() + static_cast<size_t>(height - y - 1) * stride;
        std::copy(top, top + stride, row.data());
        std::copy(bottom, bottom + stride, top);
        std::copy(row.data(), row.data() + stride, bottom);
    }
}

std::string quoted(const std::string& path) {
    std::string safe = path;
    size_t position = 0;
    while ((position = safe.find('"', position)) != std::string::npos) {
        safe.insert(position, "\\");
        position += 2;
    }
    return "\"" + safe + "\"";
}
}

AnimatedBackground::~AnimatedBackground() {
    reset();
}

void AnimatedBackground::upload(const unsigned char* pixels, int width, int height) {
    if (m_texture == 0) glGenTextures(1, &m_texture);
    glBindTexture(GL_TEXTURE_2D, m_texture);
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
    // GIF frames are flipped explicitly below so their orientation matches
    // OpenGL textures regardless of previous stb_image loads.
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
    for (int frame = 0; frame < m_gifFrameCount; ++frame) {
        std::vector<unsigned char> flipped(
            m_gifFrames.begin() + frameBytes * frame,
            m_gifFrames.begin() + frameBytes * (frame + 1));
        flipRows(flipped, m_gifWidth, m_gifHeight);
        std::copy(flipped.begin(), flipped.end(), m_gifFrames.begin() + frameBytes * frame);
    }
    return true;
}

void AnimatedBackground::stopVideo() {
    if (!m_videoPipe) return;
    m_stopVideoThread = true;
    if (m_videoThread.joinable()) m_videoThread.join();
#ifdef _WIN32
    _pclose(m_videoPipe);
#else
    pclose(m_videoPipe);
#endif
    m_videoPipe = nullptr;
    m_stopVideoThread = false;
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
    m_videoFrame.assign(static_cast<size_t>(m_videoWidth) * m_videoHeight * 4, 0);
    m_videoFrameIndex = -1;
    m_videoStartTimestamp = std::max(timestampSeconds, 0.0f);
    m_videoFit = fit;
    m_videoRealtime = realtime;
    m_videoDecoder = decoder;
    m_videoFps = std::clamp(videoFps, 1, 60);
    m_pendingVideoFrame.clear();
    m_pendingVideoFrameReady = false;

    const std::string nullDevice =
#ifdef _WIN32
        "NUL";
#else
        "/dev/null";
#endif
    std::string videoFilter;
    if (fit == OverlayBackgroundFit::Contain) {
        videoFilter =
            "fps=" + std::to_string(m_videoFps) + ",scale=" + std::to_string(m_videoWidth) + ":" +
            std::to_string(m_videoHeight) +
            ":force_original_aspect_ratio=decrease,pad=" +
            std::to_string(m_videoWidth) + ":" + std::to_string(m_videoHeight) +
            ":(ow-iw)/2:(oh-ih)/2";
    } else if (fit == OverlayBackgroundFit::Stretch) {
        videoFilter =
            "fps=" + std::to_string(m_videoFps) + ",scale=" + std::to_string(m_videoWidth) + ":" +
            std::to_string(m_videoHeight);
    } else {
        videoFilter =
            "fps=" + std::to_string(m_videoFps) + ",scale=" + std::to_string(m_videoWidth) + ":" +
            std::to_string(m_videoHeight) +
            ":force_original_aspect_ratio=increase,crop=" +
            std::to_string(m_videoWidth) + ":" + std::to_string(m_videoHeight);
    }
    std::string acceleration;
#ifdef _WIN32
    if (decoder == OverlayVideoDecoder::D3D11VA) {
        acceleration = "-hwaccel d3d11va ";
    } else if (decoder == OverlayVideoDecoder::Auto) {
        acceleration = "-hwaccel auto ";
    }
#else
    if (decoder == OverlayVideoDecoder::Auto) acceleration = "-hwaccel auto ";
#endif
    const std::string command =
        "ffmpeg -loglevel error " + acceleration +
        (realtime ? "-re " : "") +
        "-stream_loop -1 -ss " + std::to_string(m_videoStartTimestamp) +
        " -i " + quoted(path) + " -an -vf \"" + videoFilter +
        "\" -f rawvideo -pix_fmt rgba - 2>" + nullDevice;
#ifdef _WIN32
    const std::wstring wideCommand = Utf8Paths::toWide(command);
    m_videoPipe = wideCommand.empty()
        ? nullptr
        : _wpopen(wideCommand.c_str(), L"rb");
#else
    m_videoPipe = popen(command.c_str(), "r");
#endif
    if (!m_videoPipe) {
        m_error = "Could not start FFmpeg video decoder.";
        return false;
    }
    if (realtime) {
        m_stopVideoThread = false;
        m_videoThread = std::thread(&AnimatedBackground::videoReaderLoop, this);
    }
    return true;
}

bool AnimatedBackground::readVideoFrame() {
    if (!m_videoPipe || m_videoFrame.empty()) return false;
    const size_t read = std::fread(m_videoFrame.data(), 1, m_videoFrame.size(), m_videoPipe);
    if (read != m_videoFrame.size()) {
        m_error = "FFmpeg stopped while decoding the looped background.";
        return false;
    }
    flipRows(m_videoFrame, m_videoWidth, m_videoHeight);
    upload(m_videoFrame.data(), m_videoWidth, m_videoHeight);
    ++m_videoFrameIndex;
    return true;
}

void AnimatedBackground::videoReaderLoop() {
    std::vector<unsigned char> frame(m_videoFrame.size());
    while (!m_stopVideoThread) {
        const size_t read = std::fread(frame.data(), 1, frame.size(), m_videoPipe);
        if (read != frame.size()) break;
        flipRows(frame, m_videoWidth, m_videoHeight);
        {
            std::lock_guard<std::mutex> lock(m_videoFrameMutex);
            m_pendingVideoFrame = frame;
            m_pendingVideoFrameReady = true;
        }
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
        if (m_type != OverlayBackgroundType::None || m_texture != 0 || m_videoPipe) reset();
        return 0;
    }
    const bool sourceChanged = type != m_type || path != m_path;
    if (sourceChanged) {
        reset();
        if (!std::filesystem::exists(Utf8Paths::fromUtf8(path))) {
            m_error = "Background file does not exist.";
            return 0;
        }
        m_type = type;
        m_path = path;
        if (type == OverlayBackgroundType::StillImage && !loadStill(path)) return 0;
        if (type == OverlayBackgroundType::AnimatedGif && !loadGif(path)) return 0;
    }

    if (type == OverlayBackgroundType::AnimatedGif && m_gifFrameCount > 0) {
        int totalDelay = 0;
        for (const int delay : m_gifDelays) totalDelay += std::max(delay, 10);
        int clock = static_cast<int>(std::max(timestampSeconds, 0.0f) * 1000.0f);
        clock = totalDelay > 0 ? clock % totalDelay : 0;
        int frame = 0;
        for (; frame + 1 < m_gifFrameCount; ++frame) {
            const int delay = std::max(m_gifDelays[frame], 10);
            if (clock < delay) break;
            clock -= delay;
        }
        const size_t frameBytes = static_cast<size_t>(m_gifWidth) * m_gifHeight * 4;
        upload(m_gifFrames.data() + frameBytes * frame, m_gifWidth, m_gifHeight);
    } else if (type == OverlayBackgroundType::LoopedVideo) {
        const bool dimensionsChanged = outputWidth != m_videoWidth || outputHeight != m_videoHeight;
        const bool fitChanged = fit != m_videoFit;
        const bool decoderChanged = decoder != m_videoDecoder;
        const bool fpsChanged = std::clamp(videoFps, 1, 60) != m_videoFps;
        const bool modeChanged = realtime != m_videoRealtime;
        const bool movedBackwards = timestampSeconds + 0.001f < m_videoStartTimestamp;
        if (!m_videoPipe || dimensionsChanged || fitChanged || decoderChanged ||
            fpsChanged || modeChanged || (!realtime && movedBackwards)) {
            if (!startVideo(
                    path, fit, timestampSeconds, outputWidth, outputHeight,
                    realtime, decoder, videoFps)) return 0;
        }
        if (realtime) {
            std::lock_guard<std::mutex> lock(m_videoFrameMutex);
            if (m_pendingVideoFrameReady) {
                upload(m_pendingVideoFrame.data(), m_videoWidth, m_videoHeight);
                m_pendingVideoFrameReady = false;
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
    return m_texture;
}

void AnimatedBackground::reset() {
    stopVideo();
    if (m_texture != 0) glDeleteTextures(1, &m_texture);
    m_texture = 0;
    m_textureWidth = 0;
    m_textureHeight = 0;
    m_type = OverlayBackgroundType::None;
    m_path.clear();
    m_error.clear();
    m_gifFrames.clear();
    m_gifDelays.clear();
    m_gifWidth = m_gifHeight = m_gifFrameCount = 0;
    m_videoFrame.clear();
    m_videoWidth = m_videoHeight = 0;
    m_videoFrameIndex = -1;
    m_videoStartTimestamp = 0.0f;
    m_videoFit = OverlayBackgroundFit::Cover;
    m_videoDecoder = OverlayVideoDecoder::Auto;
    m_videoFps = 30;
    m_videoRealtime = false;
    m_pendingVideoFrame.clear();
    m_pendingVideoFrameReady = false;
}
