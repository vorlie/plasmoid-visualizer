#ifndef VIDEO_RENDER_MANAGER_HPP
#define VIDEO_RENDER_MANAGER_HPP

#include "AppState.hpp"
#include "AudioEngine.hpp"
#include "AnalysisEngine.hpp"
#include "Visualizer.hpp"
#include "ParticleSystem.hpp"
#include "RenderManager.hpp"
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <cstdio>

class VideoRenderManager {
public:
    VideoRenderManager();
    ~VideoRenderManager();

    // Start the rendering process (non-blocking, but OpenGL calls must happen in main loop)
    void startRender(AppState& state, AudioEngine& audioEngine);
    
    // To be called from the main loop while state.videoStatus.isRendering is true
    void update(
        AppState& state, 
        AudioEngine& audioEngine, 
        AnalysisEngine& analysisEngine, 
        Visualizer& visualizer, 
        ParticleSystem& particleSystem,
        RenderManager& renderManager
    );

    void cancelRender(AppState& state);

private:
    std::string buildFfmpegCommand(const AppState& state);
    
    FILE* m_pipe = nullptr;
    std::vector<uint8_t> m_pixelBuffer;
    
    // Audio metadata for the current render session
    ma_uint32 m_sampleRate = 48000;
    double m_dt = 1.0 / 60.0;
};

#endif // VIDEO_RENDER_MANAGER_HPP
