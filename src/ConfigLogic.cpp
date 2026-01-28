#include "ConfigLogic.hpp"
#include "ConfigManager.hpp"
#include <filesystem>
#include <iostream>
#include <cstring>
#include <algorithm>

namespace fs = std::filesystem;

void ConfigLogic::scanMusicFolder(AppState& state, const char* path) {
    state.musicFiles.clear();
    if (!path || strlen(path) == 0) return;
    try {
        if (fs::exists(path) && fs::is_directory(path)) {
            for (const auto& entry : fs::directory_iterator(path)) {
                if (entry.is_regular_file()) {
                    std::string ext = entry.path().extension().string();
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                    if (ext == ".mp3" || ext == ".wav" || ext == ".flac" || ext == ".ogg") {
                        state.musicFiles.push_back(entry.path().filename().string());
                    }
                }
            }
            std::sort(state.musicFiles.begin(), state.musicFiles.end());
        }
    } catch (const std::exception& e) {
        std::cerr << "Error scanning folder: " << e.what() << std::endl;
    }
}

void ConfigLogic::saveSettings(const AppState& state) {
    ::AppConfig config; // Using global/struct AppConfig from ConfigManager.hpp
    config.musicFolder = state.musicFolderPath;
    config.particlesEnabled = state.particlesEnabled;
    config.beatSensitivity = state.beatSensitivity;
    config.particleCount = state.particleCount;
    config.particleSpeed = state.particleSpeed;
    config.particleSize = state.particleSize;
    memcpy(config.particleColor, state.particleColor, sizeof(float)*4);
    config.phosphorDecay = state.phosphorDecay;
    config.audioMode = (int)state.currentAudioMode;
    config.captureDeviceName = state.selectedCaptureDeviceName;
    config.useSpecificCaptureDevice = state.useSpecificCaptureDevice;

    config.zenKunEnabled = state.zenKunModeEnabled;
    config.bgPath = state.backgroundImagePath;
    config.bgPulse = state.bgPulseIntensity;
    config.bgShake = state.shakeIntensity;
    config.shakeTilt = state.shakeTiltIntensity;
    config.shakeZoom = state.shakeZoomIntensity;
    config.songTitle = state.songTitle;
    config.artistName = state.artistName;
    config.showSongInfo = state.showSongInfo;
    
    config.vidWidth = state.videoSettings.width;
    config.vidHeight = state.videoSettings.height;
    config.vidFps = state.videoSettings.fps;
    config.vidCodecIdx = state.videoSettings.codecIdx;
    config.vidCrf = state.videoSettings.crf;
    config.vidOutputPath = state.videoSettings.outputPath;

    for (auto& l : state.layers) {
        ConfigLayer cl;
        cl.name = l.name;
        cl.gain = l.config.gain;
        cl.falloff = l.config.falloff;
        cl.minFreq = l.config.minFreq;
        cl.maxFreq = l.config.maxFreq;
        cl.numBars = l.config.numBars;
        cl.attack = l.config.attack;
        cl.smoothing = l.config.smoothing;
        cl.spectrumPower = l.config.spectrumPower;
        
        memcpy(cl.color, l.color, sizeof(float)*4);
        cl.barHeight = l.barHeight;
        cl.mirrored = l.mirrored;
        cl.shape = (int)l.shape;
        cl.cornerRadius = l.cornerRadius;
        cl.visible = l.visible;
        cl.timeScale = l.timeScale;
        cl.rotation = l.rotation;
        cl.flipX = l.flipX;
        cl.flipY = l.flipY;
        cl.bloom = l.bloom;
        cl.showGrid = l.showGrid;
        cl.traceWidth = l.traceWidth;
        cl.fillOpacity = l.fillOpacity;
        cl.beamHeadSize = l.beamHeadSize;
        cl.velocityModulation = l.velocityModulation;
        cl.audioChannel = (int)l.channel;
        cl.barAnchor = (int)l.barAnchor;
        config.layers.push_back(cl);
    }

    if (ConfigManager::save("", config)) { // empty string uses default path
        std::cout << "Settings saved to " << ConfigManager::getConfigPath() << std::endl;
    }
}

void ConfigLogic::loadSettings(AppState& state) {
    ::AppConfig config;
    if (ConfigManager::load("", config)) {
        strncpy(state.musicFolderPath, config.musicFolder.c_str(), sizeof(state.musicFolderPath));
        state.particlesEnabled = config.particlesEnabled;
        state.beatSensitivity = config.beatSensitivity;
        state.particleCount = config.particleCount;
        state.particleSpeed = config.particleSpeed;
        state.particleSize = config.particleSize;
        memcpy(state.particleColor, config.particleColor, sizeof(float)*4);
        state.phosphorDecay = config.phosphorDecay;
        state.currentAudioMode = (AudioMode)config.audioMode;
        strncpy(state.selectedCaptureDeviceName, config.captureDeviceName.c_str(), sizeof(state.selectedCaptureDeviceName));
        state.useSpecificCaptureDevice = config.useSpecificCaptureDevice;

        state.zenKunModeEnabled = config.zenKunEnabled;
        strncpy(state.backgroundImagePath, config.bgPath.c_str(), sizeof(state.backgroundImagePath) - 1);
        state.backgroundImagePath[sizeof(state.backgroundImagePath) - 1] = '\0';
        state.bgPulseIntensity = config.bgPulse;
        state.shakeIntensity = config.bgShake;
        state.shakeTiltIntensity = config.shakeTilt;
        state.shakeZoomIntensity = config.shakeZoom;
        strncpy(state.songTitle, config.songTitle.c_str(), sizeof(state.songTitle));
        strncpy(state.artistName, config.artistName.c_str(), sizeof(state.artistName));
        state.showSongInfo = config.showSongInfo;

        state.videoSettings.width = config.vidWidth;
        state.videoSettings.height = config.vidHeight;
        state.videoSettings.fps = config.vidFps;
        state.videoSettings.codecIdx = config.vidCodecIdx;
        state.videoSettings.crf = config.vidCrf;
        strncpy(state.videoSettings.outputPath, config.vidOutputPath.c_str(), sizeof(state.videoSettings.outputPath));

        if (!config.layers.empty()) {
            state.layers.clear();
            for (auto& cl : config.layers) {
                VisualizerLayer l;
                l.name = cl.name;
                l.config.gain = cl.gain;
                l.config.falloff = cl.falloff;
                l.config.minFreq = cl.minFreq;
                l.config.maxFreq = cl.maxFreq;
                l.config.numBars = cl.numBars;
                l.config.attack = cl.attack;
                l.config.smoothing = cl.smoothing;
                l.config.spectrumPower = cl.spectrumPower;

                memcpy(l.color, cl.color, sizeof(float)*4);
                l.barHeight = cl.barHeight;
                l.mirrored = cl.mirrored;
                l.shape = (VisualizerShape)cl.shape;
                l.cornerRadius = cl.cornerRadius;
                l.visible = cl.visible;
                l.timeScale = cl.timeScale;
                l.rotation = cl.rotation;
                l.flipX = cl.flipX;
                l.flipY = cl.flipY;
                l.bloom = cl.bloom;
                l.showGrid = cl.showGrid;
                l.traceWidth = cl.traceWidth;
                l.fillOpacity = cl.fillOpacity;
                l.beamHeadSize = cl.beamHeadSize;
                l.velocityModulation = cl.velocityModulation;
                l.channel = (AudioChannel)cl.audioChannel;
                l.barAnchor = (BarAnchor)cl.barAnchor;
                state.layers.push_back(l);
            }
        }
        scanMusicFolder(state, state.musicFolderPath);
    }
}
