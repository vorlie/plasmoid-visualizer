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

    int selectedLayerIdx = 0;
    
    // Status message
    std::string statusMessage;
    ImVec4 statusColor = ImVec4(1, 1, 1, 1);
};

#endif // APP_STATE_HPP
