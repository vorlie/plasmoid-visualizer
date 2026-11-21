#ifndef RENDER_MANAGER_HPP
#define RENDER_MANAGER_HPP

#include <GL/glew.h>
#include "AppState.hpp"
#include "AudioEngine.hpp"
#include "AnalysisEngine.hpp"
#include "Visualizer.hpp"
#include "BloomRenderer.hpp"
#include "XYOscilloscopeEngine.hpp"
#include "OverlayPreset.hpp"
#include "GaussianBlurRenderer.hpp"
#include "AnimatedBackground.hpp"
#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

class RenderManager {
public:
    RenderManager() = default;
    ~RenderManager() = default;
    RenderManager(const RenderManager&) = delete;
    RenderManager& operator=(const RenderManager&) = delete;
    RenderManager(RenderManager&&) = delete;
    RenderManager& operator=(RenderManager&&) = delete;

    void renderFrame(
        AppState& state,
        AudioEngine& audioEngine,
        AnalysisEngine& analysisEngine,
        Visualizer& visualizer,
        int display_w,
        int display_h,
        float deltaTime
    );

    void renderToTarget(
        AppState& state,
        AudioEngine& audioEngine,
        AnalysisEngine& analysisEngine,
        Visualizer& visualizer,
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
        int width,
        int height,
        float deltaTime,
        std::uint64_t audioStartFrame,
        std::uint32_t sampleRate
    );

    void renderPersistentLayers(
        AppState& state,
        AudioEngine* audioEngine,
        Visualizer& visualizer,
        const XYInputChunk* offlineXY = nullptr
    );

    void renderDirectLayers(
        AppState& state,
        AudioEngine* audioEngine,
        AnalysisEngine& analysisEngine,
        Visualizer& visualizer,
        float sampleRate,
        float deltaTime,
        const XYInputChunk* offlineXY = nullptr,
        const std::vector<float>* offlineMono = nullptr
    );

    void presentOfflinePreview(Visualizer& visualizer, int windowWidth, int windowHeight);

    void pollShaderHotReload(
        Visualizer& visualizer,
        AppState& state,
        float deltaTime,
        bool force = false);

    void renderMediaBackground(
        const AppState& state,
        Visualizer& visualizer,
        const AnimatedBackground& background,
        GLuint texture,
        int width,
        int height);

private:
    struct PipelineInputs {
        AudioEngine* audioEngine = nullptr;
        const XYInputChunk* offlineContinuousXY = nullptr;
        const XYInputChunk* offlineSnapshotXY = nullptr;
        const std::vector<float>* offlineMono = nullptr;
        AnimatedBackground* overlayBackground = nullptr;
        std::vector<float>* overlayPrevMagnitudes = nullptr;
        float overlayTime = 0.0f;
        float sampleRate = 48000.0f;
        float deltaTime = 1.0f / 60.0f;
        int width = 0;
        int height = 0;
        bool realtimeBackground = true;
        bool isOffline = false;
    };

    void syncBackgroundImage(const std::string& path, Visualizer& visualizer);
    void syncOverlayFonts(const MediaOverlayLayer& overlay, Visualizer& visualizer);
    void updateBeatEffects(AppState& state, AnalysisEngine& analysisEngine, float sampleRate, float deltaTime);
    bool ensureCaptureFramebuffer(int width, int height);
    void executePipeline(
        AppState& state,
        AnalysisEngine& analysisEngine,
        Visualizer& visualizer,
        const PipelineInputs& inputs);

    BloomRenderer m_bloomRenderer;
    Framebuffer m_sceneBuffer;
    Framebuffer m_slowPhosphorBuffer;
    XYOscilloscopeEngine m_xyEngine;
    OverlayPresetRenderer m_overlayRenderer;
    GaussianBlurRenderer m_overlayBlurRenderer;
    AnimatedBackground m_overlayBackground;
    AnimatedBackground m_offlineOverlayBackground;
    std::string m_loadedBackgroundPath;
    std::string m_loadedOverlayFont;
    std::string m_loadedLyricsFont;
    std::vector<float> m_overlayPrevMagnitudes;
    std::vector<float> m_offlineOverlayPrevMagnitudes;
    std::unordered_map<LayerId, std::vector<float>> m_offlineLayerPrevMagnitudes;
    std::unordered_map<LayerId, std::uint64_t> m_xyCursors;

    // Per-frame FFT cache (0=Mixed, 1=Left, 2=Right) & reusable scratch buffers
    std::array<std::vector<float>, 3> m_cachedFftMagnitudes;
    std::array<bool, 3> m_channelFftValid{};
    std::vector<float> m_audioScratch;
    std::vector<float> m_channelScratch;
    std::vector<float> m_layerOutputScratch;
    std::vector<float> m_overlaySpectrumScratch;
    std::vector<float> m_offlineScaledMonoScratch;
    XYInputChunk m_offlineContinuousScratch;
    XYInputChunk m_offlineSnapshotScratch;

    gl::FramebufferHandle m_captureFbo;
    gl::Texture m_captureTex;
    gl::Renderbuffer m_captureRbo;
    int m_captureW = 0;
    int m_captureH = 0;
    bool m_wasUsingPersistence = false;
    float m_shaderPollTimer = 0.0f;
};

#endif // RENDER_MANAGER_HPP
