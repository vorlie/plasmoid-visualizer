#include "RenderManager.hpp"
#include <algorithm>
#include <iostream>

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
            analysisEngine.computeFFT(audioBuffer);
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

        // 1. PERSISTENCE PASS (Render to FBO)
        visualizer.setupPersistence(width, height);
        visualizer.beginPersistence();

        bool usePersistence = false;
        for (auto& l : state.layers) {
            if (l.visible && (l.shape == VisualizerShape::OscilloscopeXY || l.shape == VisualizerShape::OscilloscopeXY_Clean)) {
                usePersistence = true; break;
            }
        }

        if (usePersistence) {
            visualizer.drawFullscreenDimmer(state.phosphorDecay); 
        } else {
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
        }

        // 0. BACKGROUND PASS
        if (state.zenKunModeEnabled) {
            visualizer.drawBackground(state.currentBgScale + state.currentShakeZoom, state.currentShakeX, state.currentShakeY, state.currentShakeTilt);
        }

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        renderPersistentLayers(state, audioEngine, visualizer);

        if (state.particlesEnabled) {
            particleSystem.render();
        }
        
        visualizer.endPersistence();

        // 2. MAIN PASS (Draw to screen/FBO)
        glViewport(0, 0, width, height);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // 2.a Draw Persistence results with additive blending
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE); 
        visualizer.drawPersistenceBuffer();
        
        // 2.b Restore standard blending for UI and other layers
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // 2.c Draw Grid (on top)
        //visualizer.renderGrid();

        // 2.d Render non-persistent layers directly
        renderDirectLayers(state, audioEngine, analysisEngine, visualizer);

        // 2.e Draw UI Overlay (Song Info)
        if (state.showSongInfo) {
            float textColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
            float shadowColor[4] = { 0.0f, 0.0f, 0.0f, 0.5f };
            float pillColor[4] = { 0.0f, 0.0f, 0.0f, 0.7f };
            
            std::string info = std::string(state.artistName) + " - " + std::string(state.songTitle);
            
            // Bottom Left Pill & Text
            visualizer.drawRoundedRect(-0.7f, -0.85f, 0.55f, 0.1f, 0.05f, pillColor);
            visualizer.drawText(info, -0.92f, -0.865f, 0.7f, textColor);
            
            // Bottom Right (Time)
            float sec = audioEngine.getPosition();
            int m = (int)sec / 60;
            int s = (int)sec % 60;
            char timeBuf[32];
            snprintf(timeBuf, sizeof(timeBuf), "%d:%02d", m, s);
            
            visualizer.drawRoundedRect(0.85f, -0.85f, 0.12f, 0.1f, 0.05f, pillColor);
            visualizer.drawText(timeBuf, 0.76f, -0.865f, 0.8f, textColor);
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
    float deltaTime
) {
    bool isBeat = false;

    try {
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

        // 1. PERSISTENCE PASS (Render to FBO)
        visualizer.setupPersistence(width, height);
        visualizer.beginPersistence();

        bool usePersistence = false;
        for (auto& l : state.layers) {
            if (l.visible && (l.shape == VisualizerShape::OscilloscopeXY || l.shape == VisualizerShape::OscilloscopeXY_Clean)) {
                usePersistence = true; break;
            }
        }

        if (usePersistence) {
            visualizer.drawFullscreenDimmer(state.phosphorDecay); 
        } else {
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
        }

        // 0. BACKGROUND PASS
        if (state.zenKunModeEnabled) {
            visualizer.drawBackground(state.currentBgScale + state.currentShakeZoom, state.currentShakeX, state.currentShakeY, state.currentShakeTilt);
        }

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // For persistent layers (oscilloscopes), use stereoBuffer
        AudioEngine dummyAudio; // Not used in offline path
        renderPersistentLayers(state, dummyAudio, visualizer, &stereoBuffer);

        if (state.particlesEnabled) {
            particleSystem.render();
        }
        
        visualizer.endPersistence();
        
        // RE-BIND capture FBO because endPersistence() binds 0
        glBindFramebuffer(GL_FRAMEBUFFER, m_captureFbo);

        // 2. MAIN PASS (Draw to FBO)
        glViewport(0, 0, width, height);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // 2.a Draw Persistence results with additive blending
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE); 
        visualizer.drawPersistenceBuffer();
        
        // 2.b Restore standard blending
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // 2.c Draw Grid
        //visualizer.renderGrid();

        // 2.d Render non-persistent layers directly
        renderDirectLayers(state, dummyAudio, analysisEngine, visualizer, &stereoBuffer, &monoBuffer);

        // 2.e Draw UI Overlay (Song Info - Offline)
        if (state.showSongInfo) {
            float textColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
            float pillColor[4] = { 0.0f, 0.0f, 0.0f, 0.7f };
            
            std::string info = std::string(state.artistName) + " - " + std::string(state.songTitle);
            
            float sec = (float)state.videoStatus.currentFrame / (float)state.videoSettings.fps;
            int m = (int)sec / 60;
            int s = (int)sec % 60;
            char timeBuf[32];
            snprintf(timeBuf, sizeof(timeBuf), "%d:%02d", m, s);

            visualizer.drawRoundedRect(-0.7f, -0.85f, 0.55f, 0.1f, 0.05f, pillColor);
            visualizer.drawText(info, -0.92f, -0.865f, 0.7f, textColor);

            visualizer.drawRoundedRect(0.85f, -0.85f, 0.12f, 0.1f, 0.05f, pillColor);
            visualizer.drawText(timeBuf, 0.76f, -0.865f, 0.8f, textColor);
        }

    } catch (const std::exception& e) {
        std::cerr << "EXCEPTION in RenderManager (Offline): " << e.what() << std::endl;
    }
}

void RenderManager::renderPersistentLayers(
    AppState& state,
    AudioEngine& audioEngine,
    Visualizer& visualizer,
    const std::vector<float>* offlineStereo
) {
    for (auto& layer : state.layers) {
        if (!layer.visible) continue;
        if (layer.shape != VisualizerShape::OscilloscopeXY && layer.shape != VisualizerShape::OscilloscopeXY_Clean) continue;

        std::vector<float> renderData;
        size_t requestFrames = (size_t)(2048 * layer.timeScale);
        std::vector<float> stereo;
        
        if (offlineStereo) {
            stereo = *offlineStereo;
        } else {
            audioEngine.getStereoBuffer(stereo, 8192);
        }
        
        size_t triggerOffset = 0;
        for (size_t i = 0; i < 512 && (i + 1) * 2 < stereo.size(); ++i) {
            if (stereo[i * 2] < 0.0f && stereo[(i + 1) * 2] >= 0.05f) {
                triggerOffset = i * 2; break;
            }
        }
        size_t copySize = std::min(stereo.size() - triggerOffset, requestFrames * 2);
        renderData.assign(stereo.begin() + triggerOffset, stereo.begin() + triggerOffset + copySize);

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
        visualizer.setBarAnchor(layer.barAnchor);
        visualizer.render(renderData);
    }
}

void RenderManager::renderDirectLayers(
    AppState& state,
    AudioEngine& audioEngine,
    AnalysisEngine& analysisEngine,
    Visualizer& visualizer,
    const std::vector<float>* offlineStereo,
    const std::vector<float>* offlineMono
) {
    for (auto& layer : state.layers) {
        if (!layer.visible) continue;
        if (layer.shape == VisualizerShape::OscilloscopeXY || layer.shape == VisualizerShape::OscilloscopeXY_Clean) continue;

        std::vector<float> renderData;
        if (layer.shape == VisualizerShape::Waveform) {
            std::vector<float> channelBuffer;
            if (offlineStereo) {
                // Extract channel from offline stereo buffer
                channelBuffer.resize(offlineStereo->size() / 2);
                int channelIdx = (int)layer.channel;
                if (channelIdx == 0) { // Mixed
                    for(size_t i=0; i<channelBuffer.size(); ++i) 
                        channelBuffer[i] = ((*offlineStereo)[i*2] + (*offlineStereo)[i*2+1]) * 0.5f;
                } else {
                    int offset = (channelIdx == 1) ? 0 : 1;
                    for(size_t i=0; i<channelBuffer.size(); ++i) 
                        channelBuffer[i] = (*offlineStereo)[i*2 + offset];
                }
            } else {
                audioEngine.getChannelBuffer(channelBuffer, 8192, (int)layer.channel);
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
            }
            analysisEngine.computeFFT(channelBuffer);
            renderData = analysisEngine.computeLayerMagnitudes(layer.config, layer.prevMagnitudes);
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
        visualizer.setBarAnchor(layer.barAnchor);
        visualizer.render(renderData);
    }
}
