#pragma once

#include "AnalysisEngine.hpp"
#include "OverlayPreset.hpp"
#include "VisualizerTypes.hpp"
#include "XYOscilloscopeTypes.hpp"

#include <string>
#include <vector>

enum class AudioMode {
    File = 0,
    Capture = 1,
    TestTone = 2,
    OscMusic = 3
};

struct VisualizerLayer {
    LayerId id = 0;
    std::string name;
    LayerConfig config;
    float color[4] = { 0.0f, 0.8f, 1.0f, 1.0f };
    float barHeight = 0.2f;
    bool mirrored = false;
    VisualizerShape shape = VisualizerShape::Bars;
    float cornerRadius = 0.0f;
    std::vector<float> prevMagnitudes;
    bool visible = true;
    float traceWidth = 2.0f;
    float fillOpacity = 0.0f;
    AudioChannel channel = AudioChannel::Mixed;
    BarAnchor barAnchor = BarAnchor::Bottom;
    XYLayerSettings xy;
    XYMeasurements xyMeasurements;
};

struct VideoRenderSettings {
    int width = 1920;
    int height = 1080;
    int fps = 60;
    int codecIdx = 0; // 0=libx264, 1=h264_nvenc, 2=h264_vaapi, 3=h264_amf
    int crf = 16;
    std::string outputPath = "output.mp4";
};

struct PersistentConfig {
    LayerId nextLayerId = 1;
    std::vector<VisualizerLayer> layers;
    std::string musicFolderPath;

    // Beat detection & background FX (Zen-Kun mode)
    float beatSensitivity = 1.3f;
    bool zenKunModeEnabled = false;
    std::string backgroundImagePath;
    float bgPulseIntensity = 0.05f;
    float shakeIntensity = 0.02f;
    float shakeTiltIntensity = 0.05f;
    float shakeZoomIntensity = 0.02f;

    // Persistence & oscilloscope display settings
    OscilloscopeDisplaySettings oscilloscopeDisplay;

    // Audio processing & mode
    float globalGain = 1.0f;
    AudioMode currentAudioMode = AudioMode::File;
    bool loopTrack = true;
    std::string selectedCaptureDeviceName;
    bool useSpecificCaptureDevice = false;

    // Display pacing
    bool enableVsync = true;
    int targetFps = 60;

    // Media overlay & video export settings
    MediaOverlayLayer mediaOverlay;
    VideoRenderSettings videoSettings;

    LayerId allocateLayerId() { return nextLayerId++; }
};
