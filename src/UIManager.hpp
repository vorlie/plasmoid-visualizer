#ifndef UI_MANAGER_HPP
#define UI_MANAGER_HPP

#include "AppState.hpp"
#include "AudioEngine.hpp"
#include "ParticleSystem.hpp"
#include "OscMusicEditor.hpp"
#include "SystemStats.hpp"
#include "imgui.h"

class VideoRenderManager;

class UIManager {
public:
    void renderUI(
        AppState& state,
        AudioEngine& audioEngine,
        ParticleSystem& particleSystem,
        OscMusicEditor& oscMusicEditor,
        SystemStats& systemStats,
        VideoRenderManager& videoRenderManager
    );

private:
    void renderMainMenu(AppState& state, AudioEngine& audioEngine);
    void renderAudioSettings(AppState& state, AudioEngine& audioEngine, OscMusicEditor& oscMusicEditor, VideoRenderManager& videoRenderManager);
    void renderLayerManager(AppState& state);
    void renderLayerEditor(AppState& state);
    void renderPlaylist(AppState& state, AudioEngine& audioEngine);
    void renderParticleSettings(AppState& state, ParticleSystem& particleSystem);
    void renderDebugInfo(AppState& state, SystemStats& systemStats);
    void renderGlobalSettings(AppState& state);
    void renderStatusMessage(AppState& state);
};

#endif // UI_MANAGER_HPP
