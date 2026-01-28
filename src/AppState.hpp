#ifndef APP_STATE_HPP
#define APP_STATE_HPP

#include <vector>
#include <string>
#include "AnalysisEngine.hpp"
#include "Visualizer.hpp"
#include "imgui.h"

enum class AudioMode {
    File = 0,
    Capture = 1,
    TestTone = 2,
    OscMusic = 3
};

struct VisualizerLayer {
    std::string name;
    LayerConfig config;
    float color[4] = { 0.0f, 0.8f, 1.0f, 1.0f };
    float barHeight = 0.2f;
    bool mirrored = false;
    VisualizerShape shape = VisualizerShape::Bars;
    float cornerRadius = 0.0f;
    std::vector<float> prevMagnitudes;
    bool visible = true;
    float timeScale = 1.0f;
    float rotation = 0.0f;
    bool flipX = false;
    bool flipY = false;
    float bloom = 1.0f;
    bool showGrid = true;
    float traceWidth = 2.0f;
    float fillOpacity = 0.0f;
    float beamHeadSize = 0.0f;
    float velocityModulation = 0.0f;
    AudioChannel channel = AudioChannel::Mixed;
    BarAnchor barAnchor = BarAnchor::Bottom;
};

struct VideoRenderSettings {
    int width = 1920;
    int height = 1080;
    int fps = 60;
    int codecIdx = 0; // 0=libx264, 1=h264_nvenc, 2=h264_vaapi, 3=h264_amf
    int crf = 16;
    char outputPath[256] = "output.mp4";
};

struct VideoRenderStatus {
    bool isRendering = false;
    float progress = 0.0f;
    int currentFrame = 0;
    int totalFrames = 0;
    std::string errorMessage;
};

struct AppState {
    // Visualizer layers
    std::vector<VisualizerLayer> layers;
    
    // Music playlist
    std::vector<std::string> musicFiles;
    char musicFolderPath[512] = "";
    char filePath[512] = "";
    
    // Particle system settings
    bool particlesEnabled = false;
    float beatSensitivity = 1.3f;
    int particleCount = 500;
    float particleSpeed = 1.0f;
    float particleSize = 5.0f;
    float particleColor[4] = { 0.0f, 1.0f, 1.0f, 1.0f };
    
    // Persistence settings
    float phosphorDecay = 0.1f;
    
    // Zen-Kun Mode & Background Support
    bool zenKunModeEnabled = false;
    char backgroundImagePath[512] = "";
    float bgPulseIntensity = 0.05f;
    float shakeIntensity = 0.02f;
    float shakeTiltIntensity = 0.05f;
    float shakeZoomIntensity = 0.02f;
    
    // Song Metadata
    char songTitle[256] = "Song Title";
    char artistName[256] = "Artist Name";
    bool showSongInfo = true;
    
    // Dynamic effects state (updated every frame)
    float currentBgScale = 1.0f;
    float currentShakeX = 0.0f;
    float currentShakeY = 0.0f;
    float currentShakeTilt = 0.0f;
    float currentShakeZoom = 0.0f;
    float bgPulseEnergy = 0.0f;
    float shakeEnergy = 0.0f;
    
    // Audio mode
    AudioMode currentAudioMode = AudioMode::File;
    char selectedCaptureDeviceName[256] = "";
    bool useSpecificCaptureDevice = false;
    
    // UI state flags
    bool showLayerEditor = false;
    bool showPlaylist = false;
    bool showAudioManager = false;
    bool showOscMusicEditor = false;
    bool showSettings = false;
    bool showSystemStats = false;
    bool showLayerManager = true;
    bool showDebugInfo = false;
    bool showParticleSettings = false;
    bool showGlobalSettings = true;
    bool showAudioSettings = true;
    bool showZenKunSettings = false;

    // Audio Processing
    float globalGain = 1.0f;

    int selectedLayerIdx = 0;

    // Video Rendering
    VideoRenderSettings videoSettings;
    VideoRenderStatus videoStatus;
    
    // Status message
    std::string statusMessage;
    ImVec4 statusColor = ImVec4(1, 1, 1, 1);

    VisualizerLayer* getSelectedLayer() {
        if (selectedLayerIdx >= 0 && selectedLayerIdx < (int)layers.size()) return &layers[selectedLayerIdx];
        return nullptr;
    }
};

#endif // APP_STATE_HPP
