#include "ConfigLogic.hpp"
#include "ConfigManager.hpp"
#include "Utf8Paths.hpp"
#include <filesystem>
#include <iostream>
#include <cstring>
#include <algorithm>
#include <cmath>

namespace fs = std::filesystem;

void ConfigLogic::scanMusicFolder(AppState& state, const char* path) {
    state.musicFiles.clear();
    if (!path || strlen(path) == 0) return;
    try {
        const fs::path folder = Utf8Paths::fromUtf8(path);
        if (fs::exists(folder) && fs::is_directory(folder)) {
            for (const auto& entry : fs::directory_iterator(folder)) {
                if (entry.is_regular_file()) {
                    std::string ext = Utf8Paths::toUtf8(entry.path().extension());
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                    if (ext == ".mp3" || ext == ".wav" || ext == ".flac" || ext == ".ogg") {
                        state.musicFiles.push_back(Utf8Paths::toUtf8(entry.path().filename()));
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
    config.oscilloscopeDisplay = state.oscilloscopeDisplay;
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

    config.mediaOverlayEnabled = state.mediaOverlay.enabled;
    config.mediaBackgroundType = static_cast<int>(state.mediaOverlay.backgroundType);
    config.mediaBackgroundFit = static_cast<int>(state.mediaOverlay.backgroundFit);
    config.mediaBackgroundPath = state.mediaOverlay.backgroundPath;
    config.mediaBackgroundOpacity = state.mediaOverlay.backgroundOpacity;
    config.mediaBackgroundDimming = state.mediaOverlay.backgroundDimming;
    config.mediaBackgroundScale = state.mediaOverlay.backgroundScale;
    config.mediaBackgroundOffsetX = state.mediaOverlay.backgroundOffsetX;
    config.mediaBackgroundOffsetY = state.mediaOverlay.backgroundOffsetY;
    config.mediaVideoDecoder = static_cast<int>(state.mediaOverlay.videoDecoder);
    config.mediaVideoFps = state.mediaOverlay.videoFps;
    config.mediaFontPath = state.mediaOverlay.fontPath;
    config.mediaLyricsFontPath = state.mediaOverlay.lyricsFontPath;
    config.mediaArtist = state.mediaOverlay.artist;
    config.mediaTitle = state.mediaOverlay.title;
    config.mediaLyricSource = state.mediaOverlay.lyricSource;
    config.mediaLyricFilePath = state.mediaOverlay.lyricFilePath;
    config.mediaTopSpectrum = state.mediaOverlay.style.showTopSpectrum;
    config.mediaBottomSpectrum = state.mediaOverlay.style.showBottomSpectrum;
    config.mediaOpacity = state.mediaOverlay.style.opacity;
    config.mediaSpectrumGain = state.mediaOverlay.style.spectrumGain;
    config.mediaSpectrumAmplitudeCap = state.mediaOverlay.style.spectrumAmplitudeCap;
    config.mediaSpectrumBarCount = state.mediaOverlay.style.spectrumBarCount;
    config.mediaSpectrumBarWidth = state.mediaOverlay.style.spectrumBarWidth;
    config.mediaSpectrumLineThickness = state.mediaOverlay.style.spectrumLineThickness;
    config.mediaTopSpectrumHeight = state.mediaOverlay.style.topSpectrumHeight;
    config.mediaBottomSpectrumHeight = state.mediaOverlay.style.bottomSpectrumHeight;
    config.mediaTopMargin = state.mediaOverlay.style.topMargin;
    config.mediaBottomMargin = state.mediaOverlay.style.bottomMargin;
    config.mediaArtistTextScale = state.mediaOverlay.style.artistTextScale;
    config.mediaTitleTextScale = state.mediaOverlay.style.titleTextScale;
    config.mediaTimestampTextScale = state.mediaOverlay.style.timestampTextScale;
    config.mediaLyricsTextScale = state.mediaOverlay.style.lyricsTextScale;
    config.mediaBlurredBand = state.mediaOverlay.style.blurredLyricsBand;
    config.mediaBandHeight = state.mediaOverlay.style.lyricsBandHeight;
    config.mediaBandOpacity = state.mediaOverlay.style.lyricsBandOpacity;
    
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
        cl.xyAutoGain = l.xyAutoGain;
        cl.xOffset = l.xOffset;
        cl.yOffset = l.yOffset;
        cl.xScale = l.xScale;
        cl.yScale = l.yScale;
        cl.useLayerPersistence = l.useLayerPersistence;
        cl.audioChannel = (int)l.channel;
        cl.barAnchor = (int)l.barAnchor;
        cl.layerId = l.id;
        cl.xy = l.xy;
        // Legacy layer controls remain the active controls until their dedicated
        // oscilloscope panel is displayed, so keep both representations aligned.
        cl.xy.persistence = l.useLayerPersistence;
        cl.xy.windowScale = l.timeScale;
        cl.xy.gainX = l.xScale;
        cl.xy.gainY = l.yScale;
        cl.xy.positionX = l.xOffset;
        cl.xy.positionY = l.yOffset;
        cl.xy.invertX = l.flipX;
        cl.xy.invertY = l.flipY;
        cl.xy.rotationDegrees = l.rotation;
        cl.xy.autoGain = l.xyAutoGain;
        cl.xy.traceWidth = l.traceWidth;
        cl.xy.bloom = l.bloom;
        cl.xy.beamHeadSize = l.beamHeadSize;
        cl.xy.dwellEffect = l.velocityModulation;
        cl.hasXYSettings = true;
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
        if (config.hasOscilloscopeDisplay) {
            state.oscilloscopeDisplay = config.oscilloscopeDisplay;
        } else {
            // Convert the former per-frame dimming amount (at 60 FPS) to a
            // slow phosphor lifetime.  Old projects deliberately start Custom.
            const float retained = std::clamp(1.0f - config.phosphorDecay, 0.001f, 0.9999f);
            const float slowMs = -1000.0f / (60.0f * std::log(retained));
            state.oscilloscopeDisplay.profile = ScopeProfile::Custom;
            state.oscilloscopeDisplay.phosphorSlowDecayMs = std::clamp(slowMs, 10.0f, 30000.0f);
            state.oscilloscopeDisplay.phosphorFastDecayMs = state.oscilloscopeDisplay.phosphorSlowDecayMs * 0.15f;
            state.oscilloscopeDisplay.phosphorSlowWeight = 0.25f;
            state.oscilloscopeDisplay.graticuleEnabled = std::any_of(config.layers.begin(), config.layers.end(), [](const ConfigLayer& layer) { return layer.showGrid; });
        }
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

        state.mediaOverlay.enabled = config.mediaOverlayEnabled;
        state.mediaOverlay.backgroundType =
            static_cast<OverlayBackgroundType>(std::clamp(config.mediaBackgroundType, 0, 3));
        state.mediaOverlay.backgroundFit =
            static_cast<OverlayBackgroundFit>(std::clamp(config.mediaBackgroundFit, 0, 2));
        strncpy(state.mediaOverlay.backgroundPath, config.mediaBackgroundPath.c_str(), sizeof(state.mediaOverlay.backgroundPath) - 1);
        state.mediaOverlay.backgroundOpacity = config.mediaBackgroundOpacity;
        state.mediaOverlay.backgroundDimming = config.mediaBackgroundDimming;
        state.mediaOverlay.backgroundScale = config.mediaBackgroundScale;
        state.mediaOverlay.backgroundOffsetX = config.mediaBackgroundOffsetX;
        state.mediaOverlay.backgroundOffsetY = config.mediaBackgroundOffsetY;
        state.mediaOverlay.videoDecoder =
            static_cast<OverlayVideoDecoder>(std::clamp(config.mediaVideoDecoder, 0, 2));
        state.mediaOverlay.videoFps = std::clamp(config.mediaVideoFps, 1, 60);
        strncpy(state.mediaOverlay.fontPath, config.mediaFontPath.c_str(), sizeof(state.mediaOverlay.fontPath) - 1);
        strncpy(state.mediaOverlay.lyricsFontPath, config.mediaLyricsFontPath.c_str(), sizeof(state.mediaOverlay.lyricsFontPath) - 1);
        strncpy(state.mediaOverlay.artist, config.mediaArtist.c_str(), sizeof(state.mediaOverlay.artist) - 1);
        strncpy(state.mediaOverlay.title, config.mediaTitle.c_str(), sizeof(state.mediaOverlay.title) - 1);
        if (!config.mediaLyricSource.empty()) {
            strncpy(state.mediaOverlay.lyricSource, config.mediaLyricSource.c_str(), sizeof(state.mediaOverlay.lyricSource) - 1);
            state.mediaOverlay.lyrics.parseLrc(state.mediaOverlay.lyricSource);
        }
        strncpy(state.mediaOverlay.lyricFilePath, config.mediaLyricFilePath.c_str(), sizeof(state.mediaOverlay.lyricFilePath) - 1);
        state.mediaOverlay.style.showTopSpectrum = config.mediaTopSpectrum;
        state.mediaOverlay.style.showBottomSpectrum = config.mediaBottomSpectrum;
        state.mediaOverlay.style.opacity = config.mediaOpacity;
        state.mediaOverlay.style.spectrumGain = config.mediaSpectrumGain;
        state.mediaOverlay.style.spectrumAmplitudeCap =
            config.mediaSpectrumAmplitudeCap;
        state.mediaOverlay.style.spectrumBarCount = config.mediaSpectrumBarCount;
        state.mediaOverlay.style.spectrumBarWidth = config.mediaSpectrumBarWidth;
        state.mediaOverlay.style.spectrumLineThickness = config.mediaSpectrumLineThickness;
        state.mediaOverlay.style.topSpectrumHeight = config.mediaTopSpectrumHeight;
        state.mediaOverlay.style.bottomSpectrumHeight = config.mediaBottomSpectrumHeight;
        state.mediaOverlay.style.topMargin = config.mediaTopMargin;
        state.mediaOverlay.style.bottomMargin = config.mediaBottomMargin;
        state.mediaOverlay.style.artistTextScale = config.mediaArtistTextScale;
        state.mediaOverlay.style.titleTextScale = config.mediaTitleTextScale;
        state.mediaOverlay.style.timestampTextScale = config.mediaTimestampTextScale;
        state.mediaOverlay.style.lyricsTextScale = config.mediaLyricsTextScale;
        state.mediaOverlay.style.blurredLyricsBand = config.mediaBlurredBand;
        state.mediaOverlay.style.lyricsBandHeight = config.mediaBandHeight;
        state.mediaOverlay.style.lyricsBandOpacity = config.mediaBandOpacity;

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
                l.id = cl.layerId != 0 ? cl.layerId : state.allocateLayerId();
                state.nextLayerId = std::max(state.nextLayerId, l.id + 1);
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
                l.xyAutoGain = cl.xyAutoGain;
                l.xOffset = cl.xOffset;
                l.yOffset = cl.yOffset;
                l.xScale = cl.xScale;
                l.yScale = cl.yScale;
                l.useLayerPersistence = cl.useLayerPersistence;
                l.channel = (AudioChannel)cl.audioChannel;
                l.barAnchor = (BarAnchor)cl.barAnchor;
                if (cl.hasXYSettings) {
                    l.xy = cl.xy;
                } else {
                    l.xy.profile = ScopeProfile::Custom;
                    l.xy.persistence = l.useLayerPersistence;
                    l.xy.windowScale = l.timeScale;
                    l.xy.gainX = l.xScale;
                    l.xy.gainY = l.yScale;
                    l.xy.positionX = l.xOffset;
                    l.xy.positionY = l.yOffset;
                    l.xy.invertX = l.flipX;
                    l.xy.invertY = l.flipY;
                    l.xy.rotationDegrees = l.rotation;
                    l.xy.autoGain = l.xyAutoGain;
                    l.xy.traceWidth = l.traceWidth;
                    l.xy.bloom = l.bloom;
                    l.xy.beamHeadSize = l.beamHeadSize;
                    l.xy.dwellEffect = l.velocityModulation;
                }
                state.layers.push_back(l);
            }
        }
        scanMusicFolder(state, state.musicFolderPath);
    }
}
