#ifndef RENDER_MANAGER_HPP
#define RENDER_MANAGER_HPP

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "AppState.hpp"
#include "AudioEngine.hpp"
#include "AnalysisEngine.hpp"
#include "Visualizer.hpp"
#include "ParticleSystem.hpp"
#include <vector>

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
        float deltaTime
    );

    void renderPersistentLayers(
        AppState& state,
        AudioEngine& audioEngine,
        Visualizer& visualizer,
        const std::vector<float>* offlineStereo = nullptr
    );

    void renderDirectLayers(
        AppState& state,
        AudioEngine& audioEngine,
        AnalysisEngine& analysisEngine,
        Visualizer& visualizer,
        const std::vector<float>* offlineStereo = nullptr,
        const std::vector<float>* offlineMono = nullptr
    );

private:
    GLuint m_captureFbo = 0;
    GLuint m_captureTex = 0;
    GLuint m_captureRbo = 0;
    int m_captureW = 0;
    int m_captureH = 0;
};

#endif // RENDER_MANAGER_HPP
