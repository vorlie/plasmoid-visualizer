#ifndef RENDER_MANAGER_HPP
#define RENDER_MANAGER_HPP

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "AppState.hpp"
#include "AudioEngine.hpp"
#include "AnalysisEngine.hpp"
#include "Visualizer.hpp"
#include "ParticleSystem.hpp"
#include "BloomRenderer.hpp"
#include "XYOscilloscopeEngine.hpp"
#include <vector>
#include <unordered_map>

class RenderManager {
public:
    void renderFrame(
        AppState& state,
        AudioEngine& audioEngine,
        AnalysisEngine& analysisEngine,
        Visualizer& visualizer,
        ParticleSystem& particleSystem,
        int display_w,
        int display_h,
        float deltaTime
    );

    void renderToTarget(
        AppState& state,
        AudioEngine& audioEngine,
        AnalysisEngine& analysisEngine,
        Visualizer& visualizer,
        ParticleSystem& particleSystem,
        int width,
        int height,
        float deltaTime,
        bool isOffline = false
    );

    void renderOfflineFrame(
        AppState& state,
        const std::vector<float>& stereoBuffer,
        const std::vector<float>& monoBuffer,
        AnalysisEngine& analysisEngine,
        Visualizer& visualizer,
        ParticleSystem& particleSystem,
        int width,
        int height,
        float deltaTime,
        std::uint64_t audioStartFrame,
        std::uint32_t sampleRate
    );

    void renderPersistentLayers(
        AppState& state,
        AudioEngine& audioEngine,
        Visualizer& visualizer,
        const XYInputChunk* offlineXY = nullptr
    );

    void renderDirectLayers(
        AppState& state,
        AudioEngine& audioEngine,
        AnalysisEngine& analysisEngine,
        Visualizer& visualizer,
        const XYInputChunk* offlineXY = nullptr,
        const std::vector<float>* offlineMono = nullptr
    );

private:
    BloomRenderer m_bloomRenderer;
    Framebuffer m_sceneBuffer;
    Framebuffer m_slowPhosphorBuffer;
    XYOscilloscopeEngine m_xyEngine;
    std::unordered_map<LayerId, std::uint64_t> m_xyCursors;
    GLuint m_captureFbo = 0;
    GLuint m_captureTex = 0;
    GLuint m_captureRbo = 0;
    int m_captureW = 0;
    int m_captureH = 0;
};

#endif // RENDER_MANAGER_HPP
