#include "RenderManager.hpp"
#include "AssetPaths.hpp"
#include "Utf8Paths.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <utility>

namespace {
float phosphorDecay(const OscilloscopeDisplaySettings& settings, float deltaTime) {
    const float seconds = std::max(deltaTime, 0.0f);
    const float fast = std::exp(-seconds * 1000.0f / std::max(settings.phosphorFastDecayMs, 1.0f));
    return 1.0f - std::clamp(fast, 0.0f, 1.0f);
}

bool isXYShape(VisualizerShape shape) {
    return shape == VisualizerShape::OscilloscopeXY ||
           shape == VisualizerShape::OscilloscopeXY_Clean;
}
} // namespace

void RenderManager::syncBackgroundImage(const std::string& path, Visualizer& visualizer) {
    if (path == m_loadedBackgroundPath) return;
    if (!path.empty()) {
        visualizer.loadBackground(path);
    } else {
        visualizer.clearBackground();
    }
    m_loadedBackgroundPath = path;
}

void RenderManager::syncOverlayFonts(const MediaOverlayLayer& overlay, Visualizer& visualizer) {
    if (!overlay.enabled) return;
    const std::string overlayFont = !overlay.fontPath.empty()
        ? overlay.fontPath
        : Utf8Paths::toUtf8(AssetPaths::defaultFont());
    if (!overlayFont.empty() && m_loadedOverlayFont != overlayFont) {
        visualizer.loadFont(overlayFont);
        m_loadedOverlayFont = overlayFont;
    }
    const std::string lyricsFont = !overlay.lyricsFontPath.empty()
        ? overlay.lyricsFontPath
        : overlayFont;
    if (!lyricsFont.empty() && m_loadedLyricsFont != lyricsFont) {
        visualizer.loadLyricsFont(lyricsFont);
        m_loadedLyricsFont = lyricsFont;
    }
}

void RenderManager::updateBeatEffects(
    AppState& state,
    AnalysisEngine& analysisEngine,
    float sampleRate,
    float deltaTime
) {
    if (!state.config.zenKunModeEnabled) {
        state.fx.currentBgScale = 1.0f;
        state.fx.currentShakeX = 0.0f;
        state.fx.currentShakeY = 0.0f;
        state.fx.currentShakeTilt = 0.0f;
        state.fx.currentShakeZoom = 0.0f;
        return;
    }

    analysisEngine.setBeatSensitivity(state.config.beatSensitivity);
    const bool isBeat = analysisEngine.detectBeat(deltaTime, sampleRate);
    if (isBeat) {
        state.fx.bgPulseEnergy = 1.0f;
        state.fx.shakeEnergy = 1.0f;
    }

    state.fx.bgPulseEnergy = std::max(0.0f, state.fx.bgPulseEnergy - deltaTime * 4.0f);
    state.fx.shakeEnergy = std::max(0.0f, state.fx.shakeEnergy - deltaTime * 8.0f);
    state.fx.currentBgScale = 1.0f + (state.fx.bgPulseEnergy * state.config.bgPulseIntensity);

    if (state.fx.shakeEnergy > 0.001f) {
        auto randomUnit = []() { return static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX) * 2.0f - 1.0f; };
        state.fx.currentShakeX = randomUnit() * state.config.shakeIntensity * state.fx.shakeEnergy;
        state.fx.currentShakeY = randomUnit() * state.config.shakeIntensity * state.fx.shakeEnergy;
        state.fx.currentShakeTilt = randomUnit() * state.config.shakeTiltIntensity * state.fx.shakeEnergy;
        state.fx.currentShakeZoom = randomUnit() * state.config.shakeZoomIntensity * state.fx.shakeEnergy;
    } else {
        state.fx.currentShakeX = 0.0f;
        state.fx.currentShakeY = 0.0f;
        state.fx.currentShakeTilt = 0.0f;
        state.fx.currentShakeZoom = 0.0f;
    }
}

bool RenderManager::ensureCaptureFramebuffer(int width, int height) {
    if (m_captureFbo.valid() && m_captureW == width && m_captureH == height) {
        return true;
    }

    m_captureFbo = gl::FramebufferHandle::create();
    glBindFramebuffer(GL_FRAMEBUFFER, m_captureFbo.get());
    m_captureTex = gl::Texture::create();
    glBindTexture(GL_TEXTURE_2D, m_captureTex.get());
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_captureTex.get(), 0);
    m_captureRbo = gl::Renderbuffer::create();
    glBindRenderbuffer(GL_RENDERBUFFER, m_captureRbo.get());
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_captureRbo.get());

    const GLenum captureStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (captureStatus != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "Offline capture framebuffer is not complete (status=0x"
                  << std::hex << captureStatus << std::dec << ")" << std::endl;
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        m_captureFbo.reset();
        m_captureTex.reset();
        m_captureRbo.reset();
        m_captureW = m_captureH = 0;
        return false;
    }
    m_captureW = width;
    m_captureH = height;
    return true;
}

void RenderManager::executePipeline(
    AppState& state,
    AnalysisEngine& analysisEngine,
    Visualizer& visualizer,
    const PipelineInputs& inputs
) {
    const auto& cfg = state.config;
    const auto& fx = state.fx;

    syncBackgroundImage(cfg.backgroundImagePath, visualizer);
    syncOverlayFonts(cfg.mediaOverlay, visualizer);

    const GLuint overlayBackgroundTexture = inputs.overlayBackground->update(
        cfg.mediaOverlay.enabled ? cfg.mediaOverlay.backgroundType : OverlayBackgroundType::None,
        cfg.mediaOverlay.backgroundFit,
        cfg.mediaOverlay.backgroundPath,
        inputs.overlayTime,
        inputs.width,
        inputs.height,
        inputs.realtimeBackground,
        cfg.mediaOverlay.videoDecoder,
        cfg.mediaOverlay.videoFps);

    updateBeatEffects(state, analysisEngine, inputs.sampleRate, inputs.deltaTime);

    GLint targetFramebuffer = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &targetFramebuffer);

    bool usePersistence = false;
    bool useBloom = false;
    for (const auto& l : cfg.layers) {
        if (l.visible && isXYShape(l.shape)) {
            usePersistence |= l.xy.persistence;
            useBloom |= l.xy.bloom > 0.01f;
        }
    }
    const bool hasBackground = cfg.zenKunModeEnabled || overlayBackgroundTexture != 0;
    const bool usePersistenceBuffer = usePersistence || hasBackground;
    const bool useSlowPhosphor =
        usePersistence && cfg.oscilloscopeDisplay.phosphorSlowWeight > 0.001f;
    const bool useOverlayBlur =
        cfg.mediaOverlay.enabled &&
        cfg.mediaOverlay.style.blurredLyricsBand &&
        cfg.mediaOverlay.style.lyricsBandOpacity > 0.001f;
    const bool useSceneBuffer = useBloom || useOverlayBlur;

    visualizer.setViewportSize(inputs.width, inputs.height);

    // 1. Persistence / Background pass (skipped when neither persistence nor background is active).
    if (usePersistenceBuffer) {
        visualizer.setupPersistence(inputs.width, inputs.height);
        if (!inputs.isOffline && usePersistence && !m_wasUsingPersistence) {
            visualizer.clearPersistence();
        }
        visualizer.beginPersistence();

        if (usePersistence) {
            visualizer.drawFullscreenDimmer(phosphorDecay(cfg.oscilloscopeDisplay, inputs.deltaTime));
        } else {
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
        }

        if (cfg.zenKunModeEnabled) {
            visualizer.drawBackground(
                fx.currentBgScale + fx.currentShakeZoom,
                fx.currentShakeX,
                fx.currentShakeY,
                fx.currentShakeTilt);
        }
        renderMediaBackground(
            state, visualizer, *inputs.overlayBackground,
            overlayBackgroundTexture, inputs.width, inputs.height);

        if (usePersistence) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            renderPersistentLayers(state, inputs.audioEngine, visualizer, inputs.offlineContinuousXY);
        }

        visualizer.endPersistence();
    }

    // Slow phosphor afterglow pass (only when persistent XY layers are active).
    if (useSlowPhosphor) {
        m_slowPhosphorBuffer.resize(inputs.width, inputs.height);
        m_slowPhosphorBuffer.bind();
        glViewport(0, 0, inputs.width, inputs.height);
        if (!inputs.isOffline && !m_wasUsingPersistence) {
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
        }
        visualizer.drawFullscreenDimmer(1.0f - std::exp(-std::max(inputs.deltaTime, 0.0f) * 1000.0f /
            std::max(cfg.oscilloscopeDisplay.phosphorSlowDecayMs, 1.0f)));
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        visualizer.drawTexture(visualizer.persistenceTexture(), 0.08f);
        Framebuffer::unbind();
    }
    if (!inputs.isOffline) {
        m_wasUsingPersistence = usePersistence;
    }

    // 2. Scene pass (HDR FBO only when bloom or blurred lyrics band needs the scene texture).
    if (useSceneBuffer) {
        m_sceneBuffer.resize(inputs.width, inputs.height);
        m_sceneBuffer.bind();
    } else {
        glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(targetFramebuffer));
    }
    glViewport(0, 0, inputs.width, inputs.height);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    if (cfg.oscilloscopeDisplay.graticuleEnabled) {
        visualizer.setGridEnabled(true);
        visualizer.setGridDivisions(
            cfg.oscilloscopeDisplay.graticuleColumns,
            cfg.oscilloscopeDisplay.graticuleRows);
        visualizer.setColor(0.3f, 0.75f, 0.55f, cfg.oscilloscopeDisplay.graticuleOpacity);
        visualizer.renderGrid();
    }

    glEnable(GL_BLEND);
    if (usePersistenceBuffer) {
        glBlendFunc(GL_ONE, GL_ONE);
        visualizer.drawPersistenceBuffer();
        if (useSlowPhosphor) {
            visualizer.drawTexture(
                m_slowPhosphorBuffer.texture(),
                cfg.oscilloscopeDisplay.phosphorSlowWeight);
        }
    }
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    renderDirectLayers(
        state, inputs.audioEngine, analysisEngine, visualizer,
        inputs.sampleRate, inputs.deltaTime,
        inputs.offlineSnapshotXY, inputs.offlineMono);

    // 3. Output pass + Bloom.
    if (useSceneBuffer) {
        glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(targetFramebuffer));
        glViewport(0, 0, inputs.width, inputs.height);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glBlendFunc(GL_ONE, GL_ONE);
        visualizer.drawTexture(m_sceneBuffer.texture());

        if (useBloom) {
            m_bloomRenderer.render(
                m_sceneBuffer.texture(),
                static_cast<GLuint>(targetFramebuffer),
                inputs.width,
                inputs.height);
        }
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    // 4. Native OpenGL Media Overlay pass (shared across live & offline exports).
    if (cfg.mediaOverlay.enabled) {
        LayerConfig overlaySpectrum;
        overlaySpectrum.numBars = 192;
        overlaySpectrum.gain = 0.012f;
        overlaySpectrum.attack = 0.82f;
        overlaySpectrum.falloff = 0.88f;
        overlaySpectrum.smoothing = 1;
        const auto& mixedMags = m_channelFftValid[0]
            ? m_cachedFftMagnitudes[0]
            : analysisEngine.rawMagnitudes();
        analysisEngine.computeLayerMagnitudesFrom(
            mixedMags, overlaySpectrum, *inputs.overlayPrevMagnitudes,
            m_overlaySpectrumScratch, inputs.sampleRate, inputs.deltaTime);

        OverlayFrameData frame;
        frame.fft = &m_overlaySpectrumScratch;
        frame.lyrics = &cfg.mediaOverlay.lyrics;
        frame.timestampSeconds = inputs.overlayTime;
        frame.artist = cfg.mediaOverlay.artist;
        frame.title = cfg.mediaOverlay.title;
        frame.previewLyric = cfg.mediaOverlay.lyricPreview;
        if (useOverlayBlur) {
            frame.blurredSceneTexture = m_overlayBlurRenderer.blur(
                m_sceneBuffer.texture(), inputs.width, inputs.height);
            glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(targetFramebuffer));
            glViewport(0, 0, inputs.width, inputs.height);
        }
        m_overlayRenderer.render(visualizer, cfg.mediaOverlay.style, frame);
    }
}

void RenderManager::renderFrame(
    AppState& state,
    AudioEngine& audioEngine,
    AnalysisEngine& analysisEngine,
    Visualizer& visualizer,
    int display_w,
    int display_h,
    float deltaTime
) {
    renderToTarget(state, audioEngine, analysisEngine, visualizer, display_w, display_h, deltaTime, false);
}

void RenderManager::renderToTarget(
    AppState& state,
    AudioEngine& audioEngine,
    AnalysisEngine& analysisEngine,
    Visualizer& visualizer,
    int width,
    int height,
    float deltaTime,
    bool isOffline
) {
    if (width <= 0 || height <= 0) return;
    m_channelFftValid.fill(false);

    try {
        if (!isOffline) {
            audioEngine.getBuffer(m_audioScratch, 8192);
            if (state.config.globalGain != 1.0f) {
                for (auto& s : m_audioScratch) s *= state.config.globalGain;
            }
            analysisEngine.computeFFT(m_audioScratch);
            m_cachedFftMagnitudes[0] = analysisEngine.rawMagnitudes();
            m_channelFftValid[0] = true;
        }

        PipelineInputs inputs;
        inputs.audioEngine = &audioEngine;
        inputs.overlayBackground = &m_overlayBackground;
        inputs.overlayPrevMagnitudes = &m_overlayPrevMagnitudes;
        inputs.overlayTime = audioEngine.getPosition();
        inputs.sampleRate = static_cast<float>(audioEngine.getSampleRate());
        inputs.deltaTime = deltaTime;
        inputs.width = width;
        inputs.height = height;
        inputs.realtimeBackground = !isOffline;
        inputs.isOffline = isOffline;

        executePipeline(state, analysisEngine, visualizer, inputs);
    } catch (const std::exception& e) {
        std::cerr << "EXCEPTION in RenderManager: " << e.what() << std::endl;
    }
}

void RenderManager::renderOfflineFrame(
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
) {
    if (width <= 0 || height <= 0) return;

    try {
        if (state.fx.videoStatus.currentFrame == 0) {
            m_offlineOverlayPrevMagnitudes.clear();
            m_offlineLayerPrevMagnitudes.clear();
            m_offlineOverlayBackground.reset();
            m_xyEngine.resetAll();
            visualizer.setupPersistence(width, height);
            visualizer.clearPersistence();
            m_slowPhosphorBuffer.resize(width, height);
            if (m_slowPhosphorBuffer.ready()) {
                m_slowPhosphorBuffer.bind();
                glViewport(0, 0, width, height);
                glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
                glClear(GL_COLOR_BUFFER_BIT);
                Framebuffer::unbind();
            }
            state.fx.bgPulseEnergy = 0.0f;
            state.fx.shakeEnergy = 0.0f;
            state.fx.currentBgScale = 1.0f;
            state.fx.currentShakeX = 0.0f;
            state.fx.currentShakeY = 0.0f;
            state.fx.currentShakeTilt = 0.0f;
            state.fx.currentShakeZoom = 0.0f;
        }

        m_channelFftValid.fill(false);
        m_offlineScaledMonoScratch = monoBuffer;
        if (state.config.globalGain != 1.0f) {
            for (auto& s : m_offlineScaledMonoScratch) s *= state.config.globalGain;
        }
        analysisEngine.computeFFT(m_offlineScaledMonoScratch);
        m_cachedFftMagnitudes[0] = analysisEngine.rawMagnitudes();
        m_channelFftValid[0] = true;

        if (!ensureCaptureFramebuffer(width, height)) return;
        glBindFramebuffer(GL_FRAMEBUFFER, m_captureFbo.get());

        m_offlineSnapshotScratch.firstFrame = audioStartFrame;
        m_offlineSnapshotScratch.sampleRate = sampleRate;
        m_offlineSnapshotScratch.discontinuityGeneration = 1;
        m_offlineSnapshotScratch.hasZ = false;
        m_offlineSnapshotScratch.dropped = false;
        const size_t snapshotFrames = stereoBuffer.size() / 2;
        m_offlineSnapshotScratch.samples.resize(snapshotFrames);
        for (size_t i = 0; i < snapshotFrames; ++i) {
            m_offlineSnapshotScratch.samples[i] = {stereoBuffer[i * 2], stereoBuffer[i * 2 + 1], 1.0f};
        }

        m_offlineContinuousScratch.firstFrame = audioStartFrame;
        m_offlineContinuousScratch.sampleRate = sampleRate;
        m_offlineContinuousScratch.discontinuityGeneration = 1;
        m_offlineContinuousScratch.hasZ = false;
        m_offlineContinuousScratch.dropped = false;
        const size_t continuousFrames = std::min(
            snapshotFrames,
            static_cast<size_t>(std::ceil(deltaTime * sampleRate)));
        m_offlineContinuousScratch.samples.resize(continuousFrames);
        for (size_t i = 0; i < continuousFrames; ++i) {
            m_offlineContinuousScratch.samples[i] = {stereoBuffer[i * 2], stereoBuffer[i * 2 + 1], 1.0f};
        }

        PipelineInputs inputs;
        inputs.offlineContinuousXY = &m_offlineContinuousScratch;
        inputs.offlineSnapshotXY = &m_offlineSnapshotScratch;
        inputs.offlineMono = &m_offlineScaledMonoScratch;
        inputs.overlayBackground = &m_offlineOverlayBackground;
        inputs.overlayPrevMagnitudes = &m_offlineOverlayPrevMagnitudes;
        inputs.overlayTime =
            static_cast<float>(state.fx.videoStatus.currentFrame) /
            static_cast<float>(std::max(state.config.videoSettings.fps, 1));
        inputs.sampleRate = static_cast<float>(sampleRate);
        inputs.deltaTime = deltaTime;
        inputs.width = width;
        inputs.height = height;
        inputs.realtimeBackground = false;
        inputs.isOffline = true;

        executePipeline(state, analysisEngine, visualizer, inputs);

        if (state.fx.videoStatus.currentFrame + 1 >= state.fx.videoStatus.totalFrames) {
            m_offlineOverlayBackground.reset();
        }
    } catch (const std::exception& e) {
        std::cerr << "EXCEPTION in RenderManager (Offline): " << e.what() << std::endl;
    }
}

void RenderManager::renderMediaBackground(
    const AppState& state,
    Visualizer& visualizer,
    const AnimatedBackground& background,
    GLuint texture,
    int width,
    int height
) {
    if (texture == 0 || width <= 0 || height <= 0) return;
    const auto& layer = state.config.mediaOverlay;
    float halfWidth = 1.0f;
    float halfHeight = 1.0f;
    float textureX = 0.0f;
    float textureY = 0.0f;
    float textureWidth = 1.0f;
    float textureHeight = 1.0f;

    if (layer.backgroundType != OverlayBackgroundType::LoopedVideo &&
        layer.backgroundFit != OverlayBackgroundFit::Stretch &&
        background.width() > 0 && background.height() > 0) {
        const float sourceAspect =
            static_cast<float>(background.width()) /
            static_cast<float>(background.height());
        const float targetAspect = static_cast<float>(width) / static_cast<float>(height);
        if (layer.backgroundFit == OverlayBackgroundFit::Contain) {
            if (sourceAspect > targetAspect) {
                halfHeight = targetAspect / sourceAspect;
            } else {
                halfWidth = sourceAspect / targetAspect;
            }
        } else if (sourceAspect > targetAspect) {
            textureWidth = targetAspect / sourceAspect;
            textureX = (1.0f - textureWidth) * 0.5f;
        } else {
            textureHeight = sourceAspect / targetAspect;
            textureY = (1.0f - textureHeight) * 0.5f;
        }
    }

    const float scale = std::clamp(layer.backgroundScale, 0.1f, 4.0f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    visualizer.drawTextureRegion(
        texture,
        layer.backgroundOffsetX, layer.backgroundOffsetY,
        halfWidth * scale, halfHeight * scale,
        textureX, textureY, textureWidth, textureHeight,
        std::clamp(layer.backgroundOpacity, 0.0f, 1.0f));

    if (layer.backgroundDimming > 0.0f) {
        const float dim[4] = {
            0.0f, 0.0f, 0.0f,
            std::clamp(layer.backgroundDimming, 0.0f, 1.0f)};
        visualizer.drawRoundedRect(0.0f, 0.0f, 1.0f, 1.0f, 0.0f, dim);
    }
}

void RenderManager::presentOfflinePreview(Visualizer& visualizer, int windowWidth, int windowHeight) {
    if (windowWidth <= 0 || windowHeight <= 0) return;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, windowWidth, windowHeight);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    if (!m_captureTex.valid() || m_captureW <= 0 || m_captureH <= 0) return;

    const float srcAspect = static_cast<float>(m_captureW) / static_cast<float>(m_captureH);
    const float dstAspect = static_cast<float>(windowWidth) / static_cast<float>(windowHeight);
    float halfW = 1.0f;
    float halfH = 1.0f;
    if (srcAspect > dstAspect) {
        halfH = dstAspect / srcAspect;
    } else {
        halfW = srcAspect / dstAspect;
    }
    glDisable(GL_BLEND);
    visualizer.drawTextureRegion(
        m_captureTex.get(), 0.0f, 0.0f, halfW, halfH,
        0.0f, 0.0f, 1.0f, 1.0f, 1.0f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void RenderManager::pollShaderHotReload(
    Visualizer& visualizer,
    AppState& state,
    float deltaTime,
    bool force
) {
    force = force || std::exchange(state.ui.requestShaderReload, false);
    if (!force && !state.ui.shaderHotReloadEnabled) {
        m_shaderPollTimer = 0.0f;
        return;
    }

    if (!force) {
        m_shaderPollTimer += std::max(deltaTime, 0.0f);
        if (m_shaderPollTimer < 0.5f) {
            return;
        }
        m_shaderPollTimer = 0.0f;
    } else {
        m_shaderPollTimer = 0.0f;
    }

    std::string visErr;
    std::string bloomErr;
    std::string blurErr;
    const bool visReloaded = visualizer.reloadShaders(force, &visErr);
    const bool bloomReloaded = m_bloomRenderer.reloadShaders(force, &bloomErr);
    const bool blurReloaded = m_overlayBlurRenderer.reloadShaders(force, &blurErr);

    const bool anyReloaded = visReloaded || bloomReloaded || blurReloaded;
    const std::string& firstErr = !visErr.empty()
        ? visErr
        : (!bloomErr.empty() ? bloomErr : blurErr);

    if (!firstErr.empty()) {
        state.ui.shaderReloadError = true;
        state.ui.shaderReloadStatus = firstErr;
        std::cerr << "[Shader Hot-Reload] " << firstErr << std::endl;
    } else if (anyReloaded || force) {
        ++state.ui.shaderReloadCount;
        state.ui.shaderReloadError = false;
        state.ui.shaderReloadStatus = "Shaders reloaded cleanly.";
        std::cout << "[Shader Hot-Reload] Reloaded shaders (#"
                  << state.ui.shaderReloadCount << ")" << std::endl;
    }
}

void RenderManager::renderPersistentLayers(
    AppState& state,
    AudioEngine* audioEngine,
    Visualizer& visualizer,
    const XYInputChunk* offlineXY
) {
    for (auto& layer : state.config.layers) {
        if (!layer.visible || !layer.xy.persistence) continue;
        if (!isXYShape(layer.shape)) continue;

        if (layer.id == 0) layer.id = state.allocateLayerId();
        XYInputChunk input;
        if (offlineXY) {
            input = *offlineXY;
        } else if (audioEngine) {
            auto cursor = m_xyCursors.find(layer.id);
            if (cursor == m_xyCursors.end()) {
                m_xyCursors[layer.id] = audioEngine->latestXYFrame();
                continue;
            }
            input = audioEngine->readXYSince(cursor->second, 32768);
            cursor->second = input.firstFrame + input.samples.size();
        } else {
            continue;
        }
        if (state.config.globalGain != 1.0f) {
            for (auto& sample : input.samples) {
                sample.x *= state.config.globalGain;
                sample.y *= state.config.globalGain;
            }
        }
        if (input.samples.empty()) continue;

        layer.xy.persistence = true;
        auto trace = m_xyEngine.processContinuous(
            layer.id, layer.xy, input, visualizer.viewportWidth(), visualizer.viewportHeight());
        layer.xyMeasurements = trace.measurements;

        visualizer.setColor(layer.color[0], layer.color[1], layer.color[2], layer.color[3]);
        visualizer.setShape(layer.shape);
        visualizer.setBloomIntensity(layer.xy.bloom);
        visualizer.setTraceWidth(layer.xy.traceWidth);
        visualizer.renderXY(trace);
    }
}

void RenderManager::renderDirectLayers(
    AppState& state,
    AudioEngine* audioEngine,
    AnalysisEngine& analysisEngine,
    Visualizer& visualizer,
    float sampleRate,
    float deltaTime,
    const XYInputChunk* offlineXY,
    const std::vector<float>* offlineMono
) {
    const float globalGain = state.config.globalGain;
    for (auto& layer : state.config.layers) {
        if (!layer.visible) continue;
        if (layer.id == 0) layer.id = state.allocateLayerId();
        if (isXYShape(layer.shape) && layer.xy.persistence) continue;

        m_layerOutputScratch.clear();
        if (isXYShape(layer.shape)) {
            XYInputChunk input;
            if (offlineXY) {
                input = *offlineXY;
            } else if (audioEngine) {
                input = audioEngine->snapshotXY(8192);
            } else {
                continue;
            }
            if (globalGain != 1.0f) {
                for (auto& sample : input.samples) { sample.x *= globalGain; sample.y *= globalGain; }
            }
            if (input.samples.empty()) continue;
            layer.xy.persistence = false;
            auto trace = m_xyEngine.processTriggered(
                layer.id, layer.xy, input, visualizer.viewportWidth(), visualizer.viewportHeight());
            layer.xyMeasurements = trace.measurements;
            visualizer.setColor(layer.color[0], layer.color[1], layer.color[2], layer.color[3]);
            visualizer.setShape(layer.shape);
            visualizer.setBloomIntensity(layer.xy.bloom);
            visualizer.setTraceWidth(layer.xy.traceWidth);
            visualizer.renderXY(trace);
            continue;
        } else if (layer.shape == VisualizerShape::Waveform) {
            if (offlineXY) {
                m_channelScratch.resize(offlineXY->samples.size());
                const int channelIdx = static_cast<int>(layer.channel);
                if (channelIdx == 0) {
                    for (size_t i = 0; i < m_channelScratch.size(); ++i) {
                        m_channelScratch[i] = (offlineXY->samples[i].x + offlineXY->samples[i].y) * 0.5f;
                    }
                } else {
                    for (size_t i = 0; i < m_channelScratch.size(); ++i) {
                        m_channelScratch[i] = channelIdx == 1 ? offlineXY->samples[i].x : offlineXY->samples[i].y;
                    }
                }
                if (globalGain != 1.0f) {
                    for (auto& s : m_channelScratch) s *= globalGain;
                }
            } else if (audioEngine) {
                audioEngine->getChannelBuffer(m_channelScratch, 8192, static_cast<int>(layer.channel));
                if (globalGain != 1.0f) {
                    for (auto& s : m_channelScratch) s *= globalGain;
                }
            } else {
                m_channelScratch.clear();
            }

            size_t triggerOffset = 0;
            for (size_t i = 0; i < 512 && i + 1 < m_channelScratch.size(); ++i) {
                if (m_channelScratch[i] < 0.0f && m_channelScratch[i + 1] >= 0.0f) {
                    triggerOffset = i;
                    break;
                }
            }
            const size_t copyLen = std::min<size_t>(2048, m_channelScratch.size() - triggerOffset);
            m_layerOutputScratch.assign(
                m_channelScratch.begin() + static_cast<ptrdiff_t>(triggerOffset),
                m_channelScratch.begin() + static_cast<ptrdiff_t>(triggerOffset + copyLen));
        } else {
            const int chIdx = std::clamp(static_cast<int>(layer.channel), 0, 2);
            if (!m_channelFftValid[chIdx]) {
                if (offlineMono) {
                    if (chIdx == 0 || !offlineXY) {
                        m_channelScratch = *offlineMono;
                    } else {
                        m_channelScratch.resize(offlineXY->samples.size());
                        const bool left = (chIdx == 1);
                        for (size_t i = 0; i < m_channelScratch.size(); ++i) {
                            m_channelScratch[i] = left ? offlineXY->samples[i].x : offlineXY->samples[i].y;
                        }
                        if (globalGain != 1.0f) {
                            for (auto& s : m_channelScratch) s *= globalGain;
                        }
                    }
                } else if (audioEngine) {
                    audioEngine->getChannelBuffer(m_channelScratch, 8192, chIdx);
                    if (globalGain != 1.0f) {
                        for (auto& s : m_channelScratch) s *= globalGain;
                    }
                } else {
                    m_channelScratch.clear();
                }
                analysisEngine.computeFFT(m_channelScratch);
                m_cachedFftMagnitudes[chIdx] = analysisEngine.rawMagnitudes();
                m_channelFftValid[chIdx] = true;
            }
            auto& previous = offlineMono
                ? m_offlineLayerPrevMagnitudes[layer.id]
                : layer.prevMagnitudes;
            analysisEngine.computeLayerMagnitudesFrom(
                m_cachedFftMagnitudes[chIdx], layer.config, previous,
                m_layerOutputScratch, sampleRate, deltaTime);
        }

        visualizer.setColor(layer.color[0], layer.color[1], layer.color[2], layer.color[3]);
        visualizer.setHeightScale(layer.barHeight);
        visualizer.setMirrored(layer.mirrored);
        visualizer.setShape(layer.shape);
        visualizer.setCornerRadius(layer.cornerRadius);
        visualizer.setTraceWidth(layer.traceWidth);
        visualizer.setFillOpacity(layer.fillOpacity);
        visualizer.setBarAnchor(layer.barAnchor);
        visualizer.render(m_layerOutputScratch);
    }
}
