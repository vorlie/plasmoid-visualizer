#include "VideoRenderManager.hpp"
#include "FfmpegArgs.hpp"
#include "Utf8Paths.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <iostream>

namespace {
constexpr size_t kWriterBufferPoolSize = 4;
constexpr auto kUpdateTickBudget = std::chrono::milliseconds(16);
constexpr auto kBackpressureWait = std::chrono::milliseconds(4);

std::string summarizeStderr(const std::string& text) {
    if (text.empty()) return {};
    size_t end = text.find_last_not_of(" \t\r\n");
    if (end == std::string::npos) return {};
    size_t start = text.find_last_of("\r\n", end);
    start = (start == std::string::npos) ? 0 : start + 1;
    std::string line = text.substr(start, end - start + 1);
    if (line.size() > 240) line = line.substr(line.size() - 240);
    return line;
}
}

VideoRenderManager::~VideoRenderManager() {
    stopWriterThread(/*abortPending=*/true);
    m_pipe.closeAndWait();
    releasePbos();
    closeExportDecoder();
}

void VideoRenderManager::closeExportDecoder() {
    if (m_exportDecoderOpen) {
        ma_decoder_uninit(&m_exportDecoder);
        m_exportDecoderOpen = false;
    }
}

void VideoRenderManager::allocatePbos(size_t frameBytes) {
    releasePbos();
    for (int i = 0; i < 2; ++i) {
        m_pbos[i] = gl::Buffer::create();
        glBindBuffer(GL_PIXEL_PACK_BUFFER, m_pbos[i].get());
        glBufferData(
            GL_PIXEL_PACK_BUFFER,
            static_cast<GLsizeiptr>(frameBytes),
            nullptr,
            GL_STREAM_READ);
    }
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    m_pboInFlight[0] = false;
    m_pboInFlight[1] = false;
    m_pboWriteIdx = 0;
}

void VideoRenderManager::releasePbos() {
    if (m_pbos[0].valid() || m_pbos[1].valid()) {
        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        m_pbos[0].reset();
        m_pbos[1].reset();
    }
    m_pboInFlight[0] = false;
    m_pboInFlight[1] = false;
    m_pboWriteIdx = 0;
}

void VideoRenderManager::startWriterThread(size_t frameBytes) {
    if (m_writerThread.joinable()) {
        stopWriterThread(/*abortPending=*/true);
    }
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_freeBuffers.clear();
        m_readyBuffers.clear();
        for (size_t i = 0; i < kWriterBufferPoolSize; ++i) {
            m_freeBuffers.emplace_back(frameBytes);
        }
        m_writerFinish = false;
        m_writerAbort = false;
        m_writerError.clear();
    }
    m_writerFailed.store(false, std::memory_order_release);
    m_writerThread = std::thread(&VideoRenderManager::writerLoop, this);
}

void VideoRenderManager::stopWriterThread(bool abortPending) {
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        if (abortPending) {
            m_writerAbort = true;
        } else {
            m_writerFinish = true;
        }
    }
    m_queueCv.notify_all();
    if (abortPending && m_writerThread.joinable()) {
        // Terminate FFmpeg first so a writer blocked in WriteFile/write unblocks immediately.
        m_pipe.terminateChild();
    }
    if (m_writerThread.joinable()) {
        m_writerThread.join();
    }
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_freeBuffers.clear();
        m_readyBuffers.clear();
        m_writerFinish = false;
        m_writerAbort = false;
    }
}

void VideoRenderManager::writerLoop() {
    for (;;) {
        std::vector<uint8_t> frame;
        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            m_queueCv.wait(lock, [this]() {
                return m_writerAbort || !m_readyBuffers.empty() || m_writerFinish;
            });
            if (m_writerAbort) break;
            if (m_readyBuffers.empty()) {
                if (m_writerFinish) break;
                continue;
            }
            frame = std::move(m_readyBuffers.front());
            m_readyBuffers.pop_front();
        }

        if (!m_pipe.writeAll(frame.data(), frame.size())) {
            const std::string errDetail = summarizeStderr(m_pipe.stderrOutput());
            std::string msg = "FFmpeg pipe closed unexpectedly";
            if (!errDetail.empty()) msg += ": " + errDetail;
            {
                std::lock_guard<std::mutex> lock(m_queueMutex);
                m_writerError = std::move(msg);
                m_freeBuffers.push_back(std::move(frame));
            }
            m_writerFailed.store(true, std::memory_order_release);
            m_queueCv.notify_all();
            break;
        }

        {
            std::lock_guard<std::mutex> lock(m_queueMutex);
            m_freeBuffers.push_back(std::move(frame));
        }
        m_queueCv.notify_one();
    }
}

bool VideoRenderManager::tryDrainPbo(int pboIdx, bool waitBriefly, bool& hadFreeBuffer) {
    hadFreeBuffer = true;
    if (!m_pboInFlight[pboIdx]) return true;

    std::vector<uint8_t> buffer;
    {
        std::unique_lock<std::mutex> lock(m_queueMutex);
        if (m_freeBuffers.empty() && waitBriefly) {
            m_queueCv.wait_for(lock, kBackpressureWait, [this]() {
                return !m_freeBuffers.empty() || m_writerFailed.load(std::memory_order_acquire) || m_writerAbort;
            });
        }
        if (m_writerFailed.load(std::memory_order_acquire)) {
            return false;
        }
        if (m_freeBuffers.empty()) {
            hadFreeBuffer = false;
            return true;
        }
        buffer = std::move(m_freeBuffers.front());
        m_freeBuffers.pop_front();
    }

    glBindBuffer(GL_PIXEL_PACK_BUFFER, m_pbos[pboIdx].get());
    void* mapped = glMapBufferRange(
        GL_PIXEL_PACK_BUFFER,
        0,
        static_cast<GLsizeiptr>(m_frameBytes),
        GL_MAP_READ_BIT);
    if (mapped) {
        std::memcpy(buffer.data(), mapped, m_frameBytes);
        glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
    } else {
        glGetBufferSubData(
            GL_PIXEL_PACK_BUFFER,
            0,
            static_cast<GLsizeiptr>(m_frameBytes),
            buffer.data());
    }
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    m_pboInFlight[pboIdx] = false;

    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_readyBuffers.push_back(std::move(buffer));
    }
    m_queueCv.notify_one();
    ++m_readbackFrames;
    return true;
}

void VideoRenderManager::finishRender(AppState& state) {
    stopWriterThread(/*abortPending=*/false);
    const bool failedDuringDrain = m_writerFailed.load(std::memory_order_acquire);
    std::string writerErr;
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        writerErr = m_writerError;
    }

    const int exitCode = m_pipe.closeAndWait();
    const std::string errDetail = summarizeStderr(m_pipe.stderrOutput());
    releasePbos();
    closeExportDecoder();
    state.fx.videoStatus.isRendering = false;

    if (!failedDuringDrain && exitCode == 0) {
        state.fx.videoStatus.progress = 1.0f;
        state.ui.statusMessage = "Video rendering complete!";
        state.ui.statusColor = ImVec4(0, 1, 0, 1);
    } else {
        std::string msg = !writerErr.empty()
            ? writerErr
            : ("FFmpeg exited with code " + std::to_string(exitCode));
        if (writerErr.empty() && !errDetail.empty()) msg += ": " + errDetail;
        state.fx.videoStatus.errorMessage = msg;
        state.ui.statusMessage = msg;
        state.ui.statusColor = ImVec4(1, 0, 0, 1);
        std::cerr << msg << std::endl;
    }
}

void VideoRenderManager::failRender(AppState& state, const std::string& message) {
    const bool alreadyFailed = m_writerFailed.load(std::memory_order_acquire);
    stopWriterThread(/*abortPending=*/!alreadyFailed);
    const int exitCode = m_pipe.closeAndWait();
    const std::string errDetail = summarizeStderr(m_pipe.stderrOutput());
    releasePbos();
    closeExportDecoder();
    state.fx.videoStatus.isRendering = false;

    std::string finalMsg = message;
    if (!errDetail.empty() && finalMsg.find(errDetail) == std::string::npos) {
        finalMsg += ": " + errDetail;
    } else if (errDetail.empty() && exitCode != 0 && exitCode != -1) {
        finalMsg += " (exit code " + std::to_string(exitCode) + ")";
    }
    state.fx.videoStatus.errorMessage = finalMsg;
    state.ui.statusMessage = "Video render failed: " + finalMsg;
    state.ui.statusColor = ImVec4(1, 0, 0, 1);
    std::cerr << "Video render failed: " << finalMsg << std::endl;
}

std::vector<std::string> VideoRenderManager::buildFfmpegArgs(const AppState& state) const {
    return FfmpegArgs::buildExportArgs(state.config.videoSettings, state.fx.filePath);
}

bool VideoRenderManager::readExportAudioFrames(size_t offsetFrames, size_t count, std::vector<float>& outStereo) {
    if (!m_exportDecoderOpen) return false;

    ma_result result = ma_decoder_seek_to_pcm_frame(&m_exportDecoder, static_cast<ma_uint64>(offsetFrames));
    if (result != MA_SUCCESS) return false;

    outStereo.resize(count * 2);
    ma_uint64 framesRead = 0;
    result = ma_decoder_read_pcm_frames(
        &m_exportDecoder, outStereo.data(), static_cast<ma_uint64>(count), &framesRead);

    if (framesRead < count) {
        std::fill(outStereo.begin() + static_cast<ptrdiff_t>(framesRead * 2), outStereo.end(), 0.0f);
    }
    return (result == MA_SUCCESS || result == MA_AT_END);
}

void VideoRenderManager::startRender(AppState& state, AudioEngine& audioEngine) {
    auto& status = state.fx.videoStatus;
    if (status.isRendering) return;

    status.errorMessage.clear();
    status.progress = 0.0f;
    status.currentFrame = 0;
    status.totalFrames = 0;

    const auto& s = state.config.videoSettings;
    if (state.fx.filePath.empty()) {
        status.errorMessage = "No audio file selected.";
        return;
    }
    if (s.outputPath.empty()) {
        status.errorMessage = "Output file path is empty.";
        return;
    }
    if (s.width <= 0 || s.height <= 0 || s.width > 7680 || s.height > 4320) {
        status.errorMessage = "Invalid video resolution (must be between 1x1 and 7680x4320).";
        return;
    }
    if ((s.width % 2) != 0 || (s.height % 2) != 0) {
        status.errorMessage = "Video width and height must be even numbers for H.264 (yuv420p).";
        return;
    }
    if (s.fps <= 0 || s.fps > 240) {
        status.errorMessage = "Invalid FPS (must be between 1 and 240).";
        return;
    }

    // Open a dedicated stereo f32 decoder for offline export so playback's ma_decoder is never touched
    // and mono/surround sources are normalized cleanly to 2 channels.
    closeExportDecoder();
    ma_decoder_config decoderConfig = ma_decoder_config_init(ma_format_f32, 2, 0);
    ma_result decResult = MA_ERROR;
#ifdef _WIN32
    const std::wstring wideInput = Utf8Paths::toWide(state.fx.filePath);
    if (!wideInput.empty()) {
        decResult = ma_decoder_init_file_w(wideInput.c_str(), &decoderConfig, &m_exportDecoder);
    }
#else
    decResult = ma_decoder_init_file(state.fx.filePath.c_str(), &decoderConfig, &m_exportDecoder);
#endif
    if (decResult != MA_SUCCESS) {
        status.errorMessage = "Failed to open audio file for export (miniaudio error " + std::to_string(decResult) + ").";
        return;
    }
    m_exportDecoderOpen = true;

    ma_uint64 lengthFrames = 0;
    if (ma_decoder_get_length_in_pcm_frames(&m_exportDecoder, &lengthFrames) != MA_SUCCESS ||
        lengthFrames == 0 || m_exportDecoder.outputSampleRate == 0) {
        closeExportDecoder();
        status.errorMessage = "Could not determine audio duration for export.";
        return;
    }

    m_sampleRate = m_exportDecoder.outputSampleRate;
    m_dt = 1.0 / static_cast<double>(s.fps);
    const double durationSec = static_cast<double>(lengthFrames) / static_cast<double>(m_sampleRate);
    status.totalFrames = std::max(1, static_cast<int>(durationSec * static_cast<double>(s.fps)));

    const std::vector<std::string> args = buildFfmpegArgs(state);
    std::cout << "Starting FFmpeg render (" << s.width << "x" << s.height
              << " @ " << s.fps << "fps, " << status.totalFrames << " frames)" << std::endl;

    if (!m_pipe.start(args, SubprocessPipe::Mode::WriteStdin, /*captureStderr=*/true)) {
        closeExportDecoder();
        status.errorMessage = "Failed to launch ffmpeg. Ensure ffmpeg is installed and in PATH.";
        return;
    }

    m_exportWidth = s.width;
    m_exportHeight = s.height;
    m_frameBytes = static_cast<size_t>(m_exportWidth) * static_cast<size_t>(m_exportHeight) * 3;
    m_submittedFrames = 0;
    m_readbackFrames = 0;

    allocatePbos(m_frameBytes);
    startWriterThread(m_frameBytes);
    audioEngine.pause();
    status.isRendering = true;
}

void VideoRenderManager::update(
    AppState& state,
    AudioEngine& audioEngine,
    AnalysisEngine& analysisEngine,
    Visualizer& visualizer,
    RenderManager& renderManager
) {
    (void)audioEngine;
    auto& status = state.fx.videoStatus;
    if (!status.isRendering || !m_pipe.isOpen()) return;

    if (m_writerFailed.load(std::memory_order_acquire)) {
        std::string err;
        {
            std::lock_guard<std::mutex> lock(m_queueMutex);
            err = m_writerError;
        }
        failRender(state, err.empty() ? "FFmpeg pipe closed unexpectedly" : err);
        return;
    }

    const auto tickStart = std::chrono::steady_clock::now();
    int framesThisTick = 0;

    while (m_submittedFrames < status.totalFrames) {
        if (framesThisTick > 0 && (std::chrono::steady_clock::now() - tickStart) >= kUpdateTickBudget) {
            break;
        }

        const int writeIdx = m_pboWriteIdx;
        const int readIdx = 1 - writeIdx;

        // If the previous frame is in flight and needs a free buffer to drain, check backpressure
        // BEFORE submitting another frame so the UI stays responsive when FFmpeg is the bottleneck.
        if (m_pboInFlight[readIdx]) {
            bool canProceed = false;
            {
                std::unique_lock<std::mutex> lock(m_queueMutex);
                if (m_freeBuffers.empty() && framesThisTick == 0) {
                    m_queueCv.wait_for(lock, kBackpressureWait, [this]() {
                        return !m_freeBuffers.empty() || m_writerFailed.load(std::memory_order_acquire);
                    });
                }
                if (m_writerFailed.load(std::memory_order_acquire)) {
                    const std::string err = m_writerError;
                    lock.unlock();
                    failRender(state, err.empty() ? "FFmpeg pipe closed unexpectedly" : err);
                    return;
                }
                canProceed = !m_freeBuffers.empty();
            }
            if (!canProceed) {
                break;
            }
        }

        const int f = m_submittedFrames;
        status.currentFrame = f;
        const size_t offset = static_cast<size_t>(static_cast<double>(f) * m_dt * static_cast<double>(m_sampleRate));

        if (!readExportAudioFrames(offset, 8192, m_frameAudio)) {
            m_frameAudio.assign(8192 * 2, 0.0f);
        }
        const size_t stereoFrames = m_frameAudio.size() / 2;
        m_monoAudio.resize(stereoFrames);
        for (size_t i = 0; i < stereoFrames; ++i) {
            m_monoAudio[i] = (m_frameAudio[i * 2] + m_frameAudio[i * 2 + 1]) * 0.5f;
        }

        renderManager.renderOfflineFrame(
            state,
            m_frameAudio,
            m_monoAudio,
            analysisEngine,
            visualizer,
            m_exportWidth,
            m_exportHeight,
            static_cast<float>(m_dt),
            static_cast<std::uint64_t>(offset),
            static_cast<std::uint32_t>(m_sampleRate)
        );

        // Kick off asynchronous DMA readback of the just-rendered frame into writeIdx.
        glBindBuffer(GL_PIXEL_PACK_BUFFER, m_pbos[writeIdx].get());
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glReadPixels(
            0, 0,
            m_exportWidth, m_exportHeight,
            GL_RGB, GL_UNSIGNED_BYTE, nullptr);
        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        m_pboInFlight[writeIdx] = true;

        // While the GPU transfers writeIdx, map and hand off the previous frame in readIdx.
        bool hadFreeBuffer = true;
        if (!tryDrainPbo(readIdx, /*waitBriefly=*/false, hadFreeBuffer)) {
            std::string err;
            {
                std::lock_guard<std::mutex> lock(m_queueMutex);
                err = m_writerError;
            }
            failRender(state, err.empty() ? "FFmpeg pipe closed unexpectedly" : err);
            return;
        }

        m_pboWriteIdx = readIdx;
        ++m_submittedFrames;
        ++framesThisTick;
        status.currentFrame = m_submittedFrames;
        status.progress =
            static_cast<float>(m_readbackFrames) /
            static_cast<float>(std::max(status.totalFrames, 1));
    }

    // Drain the final in-flight PBO once all frames have been submitted, then finalize FFmpeg.
    if (m_submittedFrames >= status.totalFrames) {
        const int lastIdx = 1 - m_pboWriteIdx;
        if (m_pboInFlight[lastIdx]) {
            bool hadFreeBuffer = true;
            if (!tryDrainPbo(lastIdx, /*waitBriefly=*/true, hadFreeBuffer)) {
                std::string err;
                {
                    std::lock_guard<std::mutex> lock(m_queueMutex);
                    err = m_writerError;
                }
                failRender(state, err.empty() ? "FFmpeg pipe closed unexpectedly" : err);
                return;
            }
            if (!hadFreeBuffer) {
                return; // Retry next UI tick without stalling the UI
            }
        }
        status.progress =
            static_cast<float>(m_readbackFrames) /
            static_cast<float>(std::max(status.totalFrames, 1));
        finishRender(state);
    }
}

void VideoRenderManager::cancelRender(AppState& state) {
    stopWriterThread(/*abortPending=*/true);
    m_pipe.terminateChild();
    m_pipe.closeAndWait();
    releasePbos();
    closeExportDecoder();
    state.fx.videoStatus.isRendering = false;
    state.fx.videoStatus.progress = 0.0f;
}
