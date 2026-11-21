#ifndef VIDEO_RENDER_MANAGER_HPP
#define VIDEO_RENDER_MANAGER_HPP

#include "AppState.hpp"
#include "AudioEngine.hpp"
#include "AnalysisEngine.hpp"
#include "Visualizer.hpp"
#include "RenderManager.hpp"
#include "GlResources.hpp"
#include "SubprocessPipe.hpp"
#include "miniaudio.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class VideoRenderManager {
public:
    VideoRenderManager() = default;
    ~VideoRenderManager();

    VideoRenderManager(const VideoRenderManager&) = delete;
    VideoRenderManager& operator=(const VideoRenderManager&) = delete;
    VideoRenderManager(VideoRenderManager&&) = delete;
    VideoRenderManager& operator=(VideoRenderManager&&) = delete;

    // Start the rendering process (non-blocking, OpenGL calls happen in main loop)
    void startRender(AppState& state, AudioEngine& audioEngine);

    // Called from the main loop while state.videoStatus.isRendering is true
    void update(
        AppState& state,
        AudioEngine& audioEngine,
        AnalysisEngine& analysisEngine,
        Visualizer& visualizer,
        RenderManager& renderManager
    );

    void cancelRender(AppState& state);

private:
    std::vector<std::string> buildFfmpegArgs(const AppState& state) const;
    bool readExportAudioFrames(size_t offsetFrames, size_t count, std::vector<float>& outStereo);
    void closeExportDecoder();
    void allocatePbos(size_t frameBytes);
    void releasePbos();
    void startWriterThread(size_t frameBytes);
    void stopWriterThread(bool abortPending);
    void writerLoop();
    bool tryDrainPbo(int pboIdx, bool waitBriefly, bool& hadFreeBuffer);
    void finishRender(AppState& state);
    void failRender(AppState& state, const std::string& message);

    SubprocessPipe m_pipe;
    ma_decoder m_exportDecoder{};
    bool m_exportDecoderOpen = false;
    std::vector<float> m_frameAudio;
    std::vector<float> m_monoAudio;

    // Double-buffered PBO state for asynchronous GPU -> CPU pixel readback
    gl::Buffer m_pbos[2];
    bool m_pboInFlight[2] = { false, false };
    int m_pboWriteIdx = 0;
    int m_exportWidth = 0;
    int m_exportHeight = 0;
    size_t m_frameBytes = 0;
    int m_submittedFrames = 0;
    int m_readbackFrames = 0;

    // Dedicated writer thread and bounded preallocated buffer pool
    std::thread m_writerThread;
    std::mutex m_queueMutex;
    std::condition_variable m_queueCv;
    std::deque<std::vector<uint8_t>> m_freeBuffers;
    std::deque<std::vector<uint8_t>> m_readyBuffers;
    bool m_writerFinish = false;
    bool m_writerAbort = false;
    std::atomic<bool> m_writerFailed{false};
    std::string m_writerError;

    // Audio metadata for the current render session
    ma_uint32 m_sampleRate = 48000;
    double m_dt = 1.0 / 60.0;
};

#endif // VIDEO_RENDER_MANAGER_HPP
