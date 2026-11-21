#ifndef UI_MANAGER_HPP
#define UI_MANAGER_HPP

#include "AppState.hpp"
#include "AudioEngine.hpp"
#include "ConfigManager.hpp"
#include "OscMusicEditor.hpp"
#include "SystemStats.hpp"
#include "imgui.h"

#include <filesystem>
#include <vector>

class VideoRenderManager;
struct GLFWwindow;

class UIManager {
public:
    void renderUI(
        AppState& state,
        AudioEngine& audioEngine,
        OscMusicEditor& oscMusicEditor,
        SystemStats& systemStats,
        VideoRenderManager& videoRenderManager,
        GLFWwindow* window
    );

private:
    void renderMainMenu(AppState& state, AudioEngine& audioEngine, GLFWwindow* window);
    void renderAudioSettings(AppState& state, AudioEngine& audioEngine, OscMusicEditor& oscMusicEditor, VideoRenderManager& videoRenderManager);
    void renderZenKunSettings(AppState& state);
    void renderLayerManager(AppState& state);
    void renderLayerEditor(AppState& state);
    void renderPlaylist(AppState& state, AudioEngine& audioEngine);
    void renderDebugInfo(AppState& state, SystemStats& systemStats, AudioEngine& audioEngine, GLFWwindow* window);
    void renderGlobalSettings(AppState& state);
    void renderLyricsEditor(AppState& state, AudioEngine& audioEngine);
    void renderPresetsPanel(AppState& state);
    void renderStatusMessage(AppState& state);

    const std::vector<std::filesystem::path>& getSystemFonts();
    void refreshCaptureDevices(AppState& state, AudioEngine& audioEngine);
    void refreshPresets();

    std::vector<AudioEngine::DeviceInfo> m_cachedCaptureDevices;
    bool m_captureDevicesLoaded = false;
    int m_selectedCaptureDeviceIdx = -1;

    std::vector<std::filesystem::path> m_systemFonts;
    bool m_systemFontsLoaded = false;

    std::vector<PresetEntry> m_cachedPresets;
    bool m_presetsLoaded = false;
};

#endif // UI_MANAGER_HPP
