#include "RenderManager.hpp"
#include "AssetPaths.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>

static float frameIndependentDecay(float decayAt60Fps, float deltaTime) {
    const float decay = std::clamp(decayAt60Fps, 0.0f, 1.0f);
    if (decay >= 1.0f) return 1.0f;
    return 1.0f - std::pow(1.0f - decay, std::max(deltaTime, 0.0f) * 60.0f);
}

static float phosphorDecay(const OscilloscopeDisplaySettings& settings, float deltaTime) {
    const float seconds = std::max(deltaTime, 0.0f);
    const float fast = std::exp(-seconds * 1000.0f / std::max(settings.phosphorFastDecayMs, 1.0f));
    return 1.0f - std::clamp(fast, 0.0f, 1.0f);
}

void RenderManager::renderFrame(
    AppState& state,
    AudioEngine& audioEngine,
    AnalysisEngine& analysisEngine,
    Visualizer& visualizer,
    ParticleSystem& particleSystem,
    int display_w,
    int display_h,
    float deltaTime
) {
    renderToTarget(state, audioEngine, analysisEngine, visualizer, particleSystem, display_w, display_h, deltaTime, false);
}

void RenderManager::renderToTarget(
    AppState& state,
    AudioEngine& audioEngine,
    AnalysisEngine& analysisEngine,
    Visualizer& visualizer,
    ParticleSystem& particleSystem,
    int width,
    int height,
    float deltaTime,
    bool isOffline
) {
    bool isBeat = false;
    std::vector<float> audioBuffer;
    
    // Background loading/update
    static char lastBgPath[512] = "";
    if (strcmp(state.backgroundImagePath, lastBgPath) != 0) {
        if (strlen(state.backgroundImagePath) > 0) {
            visualizer.loadBackground(state.backgroundImagePath);
        } else {
            visualizer.clearBackground();
        }
        strncpy(lastBgPath, state.backgroundImagePath, sizeof(lastBgPath) - 1);
        lastBgPath[sizeof(lastBgPath) - 1] = '\0';
    }

    // Beat Effects interpolation
    if (!state.zenKunModeEnabled) {
        state.currentBgScale = 1.0f;
        state.currentShakeX = 0.0f;
        state.currentShakeY = 0.0f;
    }

    try {
        if (!isOffline) {
            audioEngine.getBuffer(audioBuffer, 8192);
            // Apply global gain
            if (state.globalGain != 1.0f) {
                for (auto& s : audioBuffer) s *= state.globalGain;
            }
            analysisEngine.computeFFT(audioBuffer);
            m_overlayAnalysis.computeFFT(audioBuffer);
        }

        const float overlayTime = audioEngine.getPosition();
        const GLuint overlayBackgroundTexture = m_overlayBackground.update(
            state.mediaOverlay.enabled
                ? state.mediaOverlay.backgroundType
                : OverlayBackgroundType::None,
            state.mediaOverlay.backgroundFit,
            state.mediaOverlay.backgroundPath,
            overlayTime, width, height,
            !isOffline,
            state.mediaOverlay.videoDecoder,
            state.mediaOverlay.videoFps);
        const std::string overlayFont = state.mediaOverlay.fontPath[0] != '\0'
            ? state.mediaOverlay.fontPath
            : AssetPaths::defaultFont().string();
        if (state.mediaOverlay.enabled && !overlayFont.empty() &&
            m_loadedOverlayFont != overlayFont) {
            visualizer.loadFont(overlayFont);
            m_loadedOverlayFont = overlayFont;
        }
        const std::string lyricsFont = state.mediaOverlay.lyricsFontPath[0] != '\0'
            ? state.mediaOverlay.lyricsFontPath
            : overlayFont;
        if (state.mediaOverlay.enabled && !lyricsFont.empty() &&
            m_loadedLyricsFont != lyricsFont) {
            visualizer.loadLyricsFont(lyricsFont);
            m_loadedLyricsFont = lyricsFont;
        }
        
        if (state.particlesEnabled || state.zenKunModeEnabled) {
            analysisEngine.setBeatSensitivity(state.beatSensitivity);
            isBeat = analysisEngine.detectBeat(deltaTime);
            if (state.particlesEnabled) particleSystem.update(deltaTime, isBeat);
        }

        // Zen-Kun Beat Effects (Smoother Energy-based approach)
        if (state.zenKunModeEnabled) {
            if (isBeat) {
                state.bgPulseEnergy = 1.0f;
                state.shakeEnergy = 1.0f;
            }

            // Decay energy exponentially
            state.bgPulseEnergy = std::max(0.0f, state.bgPulseEnergy - deltaTime * 4.0f); // Fast decay for pulse
            state.shakeEnergy = std::max(0.0f, state.shakeEnergy - deltaTime * 8.0f);     // Faster decay for shake

            // Apply energy to visual variables
            state.currentBgScale = 1.0f + (state.bgPulseEnergy * state.bgPulseIntensity);
            
            // Random shake that vibrates as long as energy > 0
            if (state.shakeEnergy > 0.001f) {
                state.currentShakeX = ((float)rand() / RAND_MAX * 2.0f - 1.0f) * state.shakeIntensity * state.shakeEnergy;
                state.currentShakeY = ((float)rand() / RAND_MAX * 2.0f - 1.0f) * state.shakeIntensity * state.shakeEnergy;
                state.currentShakeTilt = ((float)rand() / RAND_MAX * 2.0f - 1.0f) * state.shakeTiltIntensity * state.shakeEnergy;
                state.currentShakeZoom = ((float)rand() / RAND_MAX * 2.0f - 1.0f) * state.shakeZoomIntensity * state.shakeEnergy;
            } else {
                state.currentShakeX = 0.0f;
                state.currentShakeY = 0.0f;
                state.currentShakeTilt = 0.0f;
                state.currentShakeZoom = 0.0f;
            }
        }

        // Preserve the caller's target while the persistence and bloom passes
        // temporarily bind their own framebuffers.
        GLint targetFramebuffer = 0;
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &targetFramebuffer);

        // 1. PERSISTENCE PASS (Render to FBO)
        visualizer.setupPersistence(width, height);
        visualizer.beginPersistence();

        bool usePersistence = false;
        bool useBloom = false;
        for (auto& l : state.layers) {
            const bool isXY = l.shape == VisualizerShape::OscilloscopeXY ||
                              l.shape == VisualizerShape::OscilloscopeXY_Clean;
            if (l.visible && isXY) {
                usePersistence |= l.useLayerPersistence;
                useBloom |= l.bloom > 0.01f;
            }
        }

        if (usePersistence) {
            visualizer.drawFullscreenDimmer(phosphorDecay(state.oscilloscopeDisplay, deltaTime));
        } else {
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
        }

        // 0. BACKGROUND PASS
        if (state.zenKunModeEnabled) {
            visualizer.drawBackground(state.currentBgScale + state.currentShakeZoom, state.currentShakeX, state.currentShakeY, state.currentShakeTilt);
        }
        renderMediaBackground(
            state, visualizer, m_overlayBackground,
            overlayBackgroundTexture, width, height);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        renderPersistentLayers(state, audioEngine, visualizer);

        if (state.particlesEnabled) {
            particleSystem.render();
        }
        
        visualizer.endPersistence();

        // The regular persistence target is the focused, fast phosphor.  Feed
        // a separately decaying history at low energy for the CRT afterglow.
        m_slowPhosphorBuffer.resize(width, height);
        m_slowPhosphorBuffer.bind();
        glViewport(0, 0, width, height);
        visualizer.drawFullscreenDimmer(1.0f - std::exp(-std::max(deltaTime, 0.0f) * 1000.0f /
            std::max(state.oscilloscopeDisplay.phosphorSlowDecayMs, 1.0f)));
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        visualizer.drawTexture(visualizer.persistenceTexture(), 0.08f);
        Framebuffer::unbind();

        // 2. SCENE PASS: combine persistent and direct layers in HDR before
        // displaying them. This lets bloom work even when persistence is off.
        m_sceneBuffer.resize(width, height);
        m_sceneBuffer.bind();
        glViewport(0, 0, width, height);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        if (state.oscilloscopeDisplay.graticuleEnabled) {
            visualizer.setGridEnabled(true);
            visualizer.setGridDivisions(state.oscilloscopeDisplay.graticuleColumns, state.oscilloscopeDisplay.graticuleRows);
            visualizer.setColor(0.3f, 0.75f, 0.55f, state.oscilloscopeDisplay.graticuleOpacity);
            visualizer.renderGrid();
        }

        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
        visualizer.drawPersistenceBuffer();
        visualizer.drawTexture(m_slowPhosphorBuffer.texture(), state.oscilloscopeDisplay.phosphorSlowWeight);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        renderDirectLayers(state, audioEngine, analysisEngine, visualizer);

        // 3. OUTPUT PASS: draw the sharp HDR scene, then add blurred bright
        // pixels over it. UI is drawn afterward and stays crisp.
        glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(targetFramebuffer));
        glViewport(0, 0, width, height);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glBlendFunc(GL_ONE, GL_ONE);
        visualizer.drawTexture(m_sceneBuffer.texture());

        if (useBloom) {
            m_bloomRenderer.render(
                m_sceneBuffer.texture(),
                static_cast<GLuint>(targetFramebuffer),
                width,
                height);
        }
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // 4. Transparent metadata/spectrum/lyrics preset. This native OpenGL
        // pass is shared with offline rendering; ImGui remains editor-only.
        if (state.mediaOverlay.enabled) {
            LayerConfig overlaySpectrum;
            overlaySpectrum.numBars = 192;
            overlaySpectrum.gain = 0.012f;
            overlaySpectrum.attack = 0.82f;
            overlaySpectrum.falloff = 0.88f;
            overlaySpectrum.smoothing = 1;
            const auto spectrum = m_overlayAnalysis.computeLayerMagnitudes(
                overlaySpectrum, m_overlayPrevMagnitudes);
            OverlayFrameData frame;
            frame.fft = &spectrum;
            frame.lyrics = &state.mediaOverlay.lyrics;
            frame.timestampSeconds = audioEngine.getPosition();
            frame.artist = state.mediaOverlay.artist;
            frame.title = state.mediaOverlay.title;
            frame.previewLyric = state.mediaOverlay.lyricPreview;
            if (state.mediaOverlay.style.blurredLyricsBand) {
                frame.blurredSceneTexture = m_overlayBlurRenderer.blur(
                    m_sceneBuffer.texture(), width, height);
                glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(targetFramebuffer));
                glViewport(0, 0, width, height);
            }
            m_overlayRenderer.render(visualizer, state.mediaOverlay.style, frame);
        }

    } catch (const std::exception& e) {
        std::cerr << "EXCEPTION in RenderManager: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "UNKNOWN EXCEPTION in RenderManager" << std::endl;
    }
}

void RenderManager::renderOfflineFrame(
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
) {
    bool isBeat = false;

    try {
        if (state.videoStatus.currentFrame == 0) {
            m_offlineOverlayPrevMagnitudes.clear();
            m_offlineLayerPrevMagnitudes.clear();
            m_offlineOverlayBackground.reset();
        }
        analysisEngine.computeFFT(monoBuffer);
        m_overlayAnalysis.computeFFT(monoBuffer);

        const float overlayTime =
            static_cast<float>(state.videoStatus.currentFrame) /
            static_cast<float>(std::max(state.videoSettings.fps, 1));
        const GLuint overlayBackgroundTexture = m_offlineOverlayBackground.update(
            state.mediaOverlay.enabled
                ? state.mediaOverlay.backgroundType
                : OverlayBackgroundType::None,
            state.mediaOverlay.backgroundFit,
            state.mediaOverlay.backgroundPath,
            overlayTime, width, height,
            false,
            state.mediaOverlay.videoDecoder,
            state.mediaOverlay.videoFps);
        const std::string overlayFont = state.mediaOverlay.fontPath[0] != '\0'
            ? state.mediaOverlay.fontPath
            : AssetPaths::defaultFont().string();
        if (state.mediaOverlay.enabled && !overlayFont.empty() &&
            m_loadedOverlayFont != overlayFont) {
            visualizer.loadFont(overlayFont);
            m_loadedOverlayFont = overlayFont;
        }
        const std::string lyricsFont = state.mediaOverlay.lyricsFontPath[0] != '\0'
            ? state.mediaOverlay.lyricsFontPath
            : overlayFont;
        if (state.mediaOverlay.enabled && !lyricsFont.empty() &&
            m_loadedLyricsFont != lyricsFont) {
            visualizer.loadLyricsFont(lyricsFont);
            m_loadedLyricsFont = lyricsFont;
        }

        // Setup capture FBO if needed
        if (m_captureFbo == 0 || m_captureW != width || m_captureH != height) {
            if (m_captureFbo != 0) {
                glDeleteFramebuffers(1, &m_captureFbo);
                glDeleteTextures(1, &m_captureTex);
                glDeleteRenderbuffers(1, &m_captureRbo);
            }
            glGenFramebuffers(1, &m_captureFbo);
            glBindFramebuffer(GL_FRAMEBUFFER, m_captureFbo);
            glGenTextures(1, &m_captureTex);
            glBindTexture(GL_TEXTURE_2D, m_captureTex);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_captureTex, 0);
            glGenRenderbuffers(1, &m_captureRbo);
            glBindRenderbuffer(GL_RENDERBUFFER, m_captureRbo);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_captureRbo);
            m_captureW = width;
            m_captureH = height;
        }
        
        glBindFramebuffer(GL_FRAMEBUFFER, m_captureFbo);

        if (state.particlesEnabled || state.zenKunModeEnabled) {
            analysisEngine.setBeatSensitivity(state.beatSensitivity);
            isBeat = analysisEngine.detectBeat(deltaTime);
            if (state.particlesEnabled) particleSystem.update(deltaTime, isBeat);
        }

        // Zen-Kun Beat Effects (Sync for offline, Smoothed)
        if (state.zenKunModeEnabled) {
            if (isBeat) {
                state.bgPulseEnergy = 1.0f;
                state.shakeEnergy = 1.0f;
            }
            state.bgPulseEnergy = std::max(0.0f, state.bgPulseEnergy - deltaTime * 4.0f);
            state.shakeEnergy = std::max(0.0f, state.shakeEnergy - deltaTime * 8.0f);

            state.currentBgScale = 1.0f + (state.bgPulseEnergy * state.bgPulseIntensity);
            if (state.shakeEnergy > 0.001f) {
                state.currentShakeX = ((float)rand() / RAND_MAX * 2.0f - 1.0f) * state.shakeIntensity * state.shakeEnergy;
                state.currentShakeY = ((float)rand() / RAND_MAX * 2.0f - 1.0f) * state.shakeIntensity * state.shakeEnergy;
                state.currentShakeTilt = ((float)rand() / RAND_MAX * 2.0f - 1.0f) * state.shakeTiltIntensity * state.shakeEnergy;
                state.currentShakeZoom = ((float)rand() / RAND_MAX * 2.0f - 1.0f) * state.shakeZoomIntensity * state.shakeEnergy;
            } else {
                state.currentShakeX = 0.0f;
                state.currentShakeY = 0.0f;
                state.currentShakeTilt = 0.0f;
                state.currentShakeZoom = 0.0f;
            }
        }

        GLint targetFramebuffer = 0;
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &targetFramebuffer);

        // 1. PERSISTENCE PASS (Render to FBO)
        visualizer.setupPersistence(width, height);
        visualizer.beginPersistence();

        bool usePersistence = false;
        bool useBloom = false;
        for (auto& l : state.layers) {
            const bool isXY = l.shape == VisualizerShape::OscilloscopeXY ||
                              l.shape == VisualizerShape::OscilloscopeXY_Clean;
            if (l.visible && isXY) {
                usePersistence |= l.useLayerPersistence;
                useBloom |= l.bloom > 0.01f;
            }
        }

        if (usePersistence) {
            visualizer.drawFullscreenDimmer(phosphorDecay(state.oscilloscopeDisplay, deltaTime));
        } else {
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
        }

        // 0. BACKGROUND PASS
        if (state.zenKunModeEnabled) {
            visualizer.drawBackground(state.currentBgScale + state.currentShakeZoom, state.currentShakeX, state.currentShakeY, state.currentShakeTilt);
        }
        renderMediaBackground(
            state, visualizer, m_offlineOverlayBackground,
            overlayBackgroundTexture, width, height);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        XYInputChunk offlineContinuous;
        offlineContinuous.firstFrame = audioStartFrame;
        offlineContinuous.sampleRate = sampleRate;
        offlineContinuous.discontinuityGeneration = 1;
        const size_t continuousFrames = std::min(
            stereoBuffer.size() / 2,
            static_cast<size_t>(std::ceil(deltaTime * sampleRate)));
        offlineContinuous.samples.reserve(continuousFrames);
        for (size_t i = 0; i < continuousFrames; ++i) {
            offlineContinuous.samples.push_back({stereoBuffer[i * 2], stereoBuffer[i * 2 + 1], 1.0f});
        }

        XYInputChunk offlineSnapshot;
        offlineSnapshot.firstFrame = audioStartFrame;
        offlineSnapshot.sampleRate = sampleRate;
        offlineSnapshot.discontinuityGeneration = 1;
        offlineSnapshot.samples.reserve(stereoBuffer.size() / 2);
        for (size_t i = 0; i < stereoBuffer.size() / 2; ++i) {
            offlineSnapshot.samples.push_back({stereoBuffer[i * 2], stereoBuffer[i * 2 + 1], 1.0f});
        }

        // For persistent layers, consume exactly this video frame's samples.
        AudioEngine dummyAudio; // Not used in offline path
        renderPersistentLayers(state, dummyAudio, visualizer, &offlineContinuous);

        if (state.particlesEnabled) {
            particleSystem.render();
        }
        
        visualizer.endPersistence();

        m_slowPhosphorBuffer.resize(width, height);
        m_slowPhosphorBuffer.bind();
        glViewport(0, 0, width, height);
        visualizer.drawFullscreenDimmer(1.0f - std::exp(-std::max(deltaTime, 0.0f) * 1000.0f /
            std::max(state.oscilloscopeDisplay.phosphorSlowDecayMs, 1.0f)));
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        visualizer.drawTexture(visualizer.persistenceTexture(), 0.08f);
        Framebuffer::unbind();

        // 2. HDR scene pass.
        m_sceneBuffer.resize(width, height);
        m_sceneBuffer.bind();
        glViewport(0, 0, width, height);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        if (state.oscilloscopeDisplay.graticuleEnabled) {
            visualizer.setGridEnabled(true);
            visualizer.setGridDivisions(state.oscilloscopeDisplay.graticuleColumns, state.oscilloscopeDisplay.graticuleRows);
            visualizer.setColor(0.3f, 0.75f, 0.55f, state.oscilloscopeDisplay.graticuleOpacity);
            visualizer.renderGrid();
        }

        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
        visualizer.drawPersistenceBuffer();
        visualizer.drawTexture(m_slowPhosphorBuffer.texture(), state.oscilloscopeDisplay.phosphorSlowWeight);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        renderDirectLayers(state, dummyAudio, analysisEngine, visualizer, &offlineSnapshot, &monoBuffer);

        // 3. Output to the offline capture framebuffer.
        glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(targetFramebuffer));
        glViewport(0, 0, width, height);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glBlendFunc(GL_ONE, GL_ONE);
        visualizer.drawTexture(m_sceneBuffer.texture());

        if (useBloom) {
            m_bloomRenderer.render(
                m_sceneBuffer.texture(),
                static_cast<GLuint>(targetFramebuffer),
                width,
                height);
        }
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // 4. Matching offline overlay for deterministic video exports.
        if (state.mediaOverlay.enabled) {
            LayerConfig overlaySpectrum;
            overlaySpectrum.numBars = 192;
            overlaySpectrum.gain = 0.012f;
            overlaySpectrum.attack = 0.82f;
            overlaySpectrum.falloff = 0.88f;
            overlaySpectrum.smoothing = 1;
            const auto spectrum = m_overlayAnalysis.computeLayerMagnitudes(
                overlaySpectrum, m_offlineOverlayPrevMagnitudes);
            OverlayFrameData frame;
            frame.fft = &spectrum;
            frame.lyrics = &state.mediaOverlay.lyrics;
            frame.timestampSeconds =
                static_cast<float>(state.videoStatus.currentFrame) /
                static_cast<float>(std::max(state.videoSettings.fps, 1));
            frame.artist = state.mediaOverlay.artist;
            frame.title = state.mediaOverlay.title;
            frame.previewLyric = state.mediaOverlay.lyricPreview;
            if (state.mediaOverlay.style.blurredLyricsBand) {
                frame.blurredSceneTexture = m_overlayBlurRenderer.blur(
                    m_sceneBuffer.texture(), width, height);
                glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(targetFramebuffer));
                glViewport(0, 0, width, height);
            }
            m_overlayRenderer.render(visualizer, state.mediaOverlay.style, frame);
        }
        if (state.videoStatus.currentFrame + 1 >= state.videoStatus.totalFrames) {
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
    const auto& layer = state.mediaOverlay;
    float halfWidth = 1.0f;
    float halfHeight = 1.0f;
    float textureX = 0.0f;
    float textureY = 0.0f;
    float textureWidth = 1.0f;
    float textureHeight = 1.0f;

    // Video frames are already fitted by FFmpeg. Still images and GIFs retain
    // their native dimensions, so fit/crop them here without distortion.
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

void RenderManager::renderPersistentLayers(
    AppState& state,
    AudioEngine& audioEngine,
    Visualizer& visualizer,
    const XYInputChunk* offlineXY
) {
    for (auto& layer : state.layers) {
        if (!layer.visible || !layer.useLayerPersistence) continue;
        if (layer.shape != VisualizerShape::OscilloscopeXY && layer.shape != VisualizerShape::OscilloscopeXY_Clean) continue;

        if (layer.id == 0) layer.id = state.allocateLayerId();
        XYInputChunk input;
        if (offlineXY) {
            input = *offlineXY;
        } else {
            auto cursor = m_xyCursors.find(layer.id);
            if (cursor == m_xyCursors.end()) {
                m_xyCursors[layer.id] = audioEngine.latestXYFrame();
                continue;
            }
            input = audioEngine.readXYSince(cursor->second, 32768);
            cursor->second = input.firstFrame + input.samples.size();
        }
        if (state.globalGain != 1.0f) {
            for (auto& sample : input.samples) { sample.x *= state.globalGain; sample.y *= state.globalGain; }
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
    AudioEngine& audioEngine,
    AnalysisEngine& analysisEngine,
    Visualizer& visualizer,
    const XYInputChunk* offlineXY,
    const std::vector<float>* offlineMono
) {
    for (auto& layer : state.layers) {
        if (!layer.visible) continue;
        if (layer.id == 0) layer.id = state.allocateLayerId();
        // Skip persistent XY layers (they are handled in renderPersistentLayers)
        if ((layer.shape == VisualizerShape::OscilloscopeXY || layer.shape == VisualizerShape::OscilloscopeXY_Clean) && layer.useLayerPersistence) continue;

        std::vector<float> renderData;
        if (layer.shape == VisualizerShape::OscilloscopeXY ||
            layer.shape == VisualizerShape::OscilloscopeXY_Clean) {
            if (layer.id == 0) layer.id = state.allocateLayerId();
            XYInputChunk input = offlineXY ? *offlineXY : audioEngine.snapshotXY(8192);
            if (state.globalGain != 1.0f) {
                for (auto& sample : input.samples) { sample.x *= state.globalGain; sample.y *= state.globalGain; }
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
            std::vector<float> channelBuffer;
            if (offlineXY) {
                // Extract channel from offline stereo buffer
                channelBuffer.resize(offlineXY->samples.size());
                int channelIdx = (int)layer.channel;
                if (channelIdx == 0) { // Mixed
                    for(size_t i=0; i<channelBuffer.size(); ++i) 
                        channelBuffer[i] = (offlineXY->samples[i].x + offlineXY->samples[i].y) * 0.5f;
                } else {
                    for(size_t i=0; i<channelBuffer.size(); ++i)
                        channelBuffer[i] = channelIdx == 1 ? offlineXY->samples[i].x : offlineXY->samples[i].y;
                }
            } else {
                audioEngine.getChannelBuffer(channelBuffer, 8192, (int)layer.channel);
                if (state.globalGain != 1.0f) {
                    for (auto& s : channelBuffer) s *= state.globalGain;
                }
            }
            
            size_t triggerOffset = 0;
            for (size_t i = 0; i < 512 && i + 1 < channelBuffer.size(); ++i) {
                if (channelBuffer[i] < 0.0f && channelBuffer[i+1] >= 0.0f) {
                    triggerOffset = i; break;
                }
            }
            size_t copyLen = std::min((size_t)2048, channelBuffer.size() - triggerOffset);
            renderData.assign(channelBuffer.begin() + triggerOffset, channelBuffer.begin() + triggerOffset + copyLen);
        } else {
            std::vector<float> channelBuffer;
            if (offlineMono) {
                channelBuffer = *offlineMono;
            } else {
                audioEngine.getChannelBuffer(channelBuffer, 8192, (int)layer.channel);
                if (state.globalGain != 1.0f) {
                    for (auto& s : channelBuffer) s *= state.globalGain;
                }
            }
            analysisEngine.computeFFT(channelBuffer);
            auto& previous = offlineMono
                ? m_offlineLayerPrevMagnitudes[layer.id]
                : layer.prevMagnitudes;
            renderData = analysisEngine.computeLayerMagnitudes(layer.config, previous);
        }

        visualizer.setColor(layer.color[0], layer.color[1], layer.color[2], layer.color[3]);
        visualizer.setHeightScale(layer.barHeight);
        visualizer.setMirrored(layer.mirrored);
        visualizer.setShape(layer.shape);
        visualizer.setCornerRadius(layer.cornerRadius);
        visualizer.setRotation(layer.rotation * 3.14159f / 180.0f);
        visualizer.setFlip(layer.flipX, layer.flipY);
        visualizer.setBloomIntensity(layer.bloom);
        visualizer.setGridEnabled(layer.showGrid);
        visualizer.setTraceWidth(layer.traceWidth);
        visualizer.setFillOpacity(layer.fillOpacity);
        visualizer.setBeamHeadSize(layer.beamHeadSize);
        visualizer.setVelocityModulation(layer.velocityModulation);
        visualizer.setXYAutoGain(layer.xyAutoGain);
        visualizer.setOffset(layer.xOffset, layer.yOffset);
        visualizer.setScale(layer.xScale, layer.yScale);
        visualizer.setPersistenceEnabled(layer.useLayerPersistence);
        visualizer.setBarAnchor(layer.barAnchor);
        visualizer.render(renderData);
    }
}
