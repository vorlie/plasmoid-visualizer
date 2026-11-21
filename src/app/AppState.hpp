#ifndef APP_STATE_HPP
#define APP_STATE_HPP

#include "PersistentConfig.hpp"
#include "imgui.h"

#include <string>
#include <vector>

struct VideoRenderStatus {
    bool isRendering = false;
    float progress = 0.0f;
    int currentFrame = 0;
    int totalFrames = 0;
    std::string errorMessage;
};

struct RuntimeFxState {
    // Active file & scanned playlist
    std::string filePath;
    std::vector<std::string> musicFiles;

    // Dynamic beat effects state (updated every frame)
    float currentBgScale = 1.0f;
    float currentShakeX = 0.0f;
    float currentShakeY = 0.0f;
    float currentShakeTilt = 0.0f;
    float currentShakeZoom = 0.0f;
    float bgPulseEnergy = 0.0f;
    float shakeEnergy = 0.0f;

    // Active video export status
    VideoRenderStatus videoStatus;
};

struct UiState {
    // Panel visibility flags
    bool showLayerEditor = false;
    bool showPlaylist = false;
    bool showLayerManager = true;
    bool showDebugInfo = false;
    bool showGlobalSettings = true;
    bool showAudioSettings = true;
    bool showZenKunSettings = false;
    bool showLyricsEditor = false;
    bool showPresetsPanel = false;
    bool showRenderDialog = false;

    // Presets panel inputs
    std::string presetSaveName = "My Preset";
    std::string presetFilePath;

    // Selection state
    int selectedLayerIdx = 0;
    int selectedLyricIdx = -1;

    // Status notification
    std::string statusMessage;
    ImVec4 statusColor = ImVec4(1, 1, 1, 1);

    // Test tone panel controls
    float testToneFreq = 440.0f;
    float testToneFreqRight = 440.0f;
    bool testToneStereo = false;
    float testToneVol = 0.5f;
    int testToneType = 0;

    // Oscilloscope music editor panel controls
    std::string oscXExpr;
    std::string oscYExpr;
    std::string oscZExpr;
    float oscDuration = 2.0f;
    float oscBaseFreq = 440.0f;
    bool oscEditorSynced = false;
    std::vector<float> oscPreviewPoints;
    bool oscPreviewDirty = true;

    // Live GLSL shader hot-reload controls & status
#ifndef NDEBUG
    bool shaderHotReloadEnabled = true;
#else
    bool shaderHotReloadEnabled = false;
#endif
    bool requestShaderReload = false;
    int shaderReloadCount = 0;
    bool shaderReloadError = false;
    std::string shaderReloadStatus;
};

struct AppState {
    AppState();

    PersistentConfig config;
    RuntimeFxState fx;
    UiState ui;

    VisualizerLayer* getSelectedLayer() {
        if (ui.selectedLayerIdx >= 0 && ui.selectedLayerIdx < static_cast<int>(config.layers.size())) {
            return &config.layers[ui.selectedLayerIdx];
        }
        return nullptr;
    }

    LayerId allocateLayerId() { return config.allocateLayerId(); }
};

#endif // APP_STATE_HPP
