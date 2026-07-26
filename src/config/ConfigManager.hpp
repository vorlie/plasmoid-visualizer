#ifndef CONFIG_MANAGER_HPP
#define CONFIG_MANAGER_HPP

#include <string>
#include <vector>
#include "XYOscilloscopeTypes.hpp"

struct ConfigLayer {
    LayerId layerId = 0;
    std::string name;
    // LayerConfig fields
    float gain;
    float falloff;
    float minFreq;
    float maxFreq;
    size_t numBars;
    float attack = 0.8f;
    int smoothing = 1;
    float spectrumPower = 1.0f;
    
    float color[4];
    float barHeight;
    bool mirrored;
    int shape; // enum cast
    float cornerRadius;
    bool visible;
    float timeScale;
    float rotation;
    bool flipX;
    bool flipY;
    float bloom = 1.0f;
    bool showGrid = true;
    float traceWidth = 2.0f;
    float fillOpacity = 0.0f;
    float beamHeadSize = 0.0f;
    float velocityModulation = 0.0f;
    bool xyAutoGain = false;
    float xOffset = 0.0f;
    float yOffset = 0.0f;
    float xScale = 1.0f;
    float yScale = 1.0f;
    bool useLayerPersistence = true;
    int audioChannel = 0; // 0=Mixed, 1=Left, 2=Right
    int barAnchor = 0; // 0=Bottom, 1=Top, 2=Left, 3=Right, 4=Center
    bool hasXYSettings = false;
    XYLayerSettings xy;
};

struct AppConfig {
    std::string musicFolder;
    bool particlesEnabled;
    float beatSensitivity;
    int particleCount;
    float particleSpeed;
    float particleSize;
    float particleColor[4];
    float phosphorDecay = 0.1f;
    float globalGain = 1.0f;
    bool enableVsync = true;
    int targetFps = 60;
    bool hasOscilloscopeDisplay = false;
    OscilloscopeDisplaySettings oscilloscopeDisplay;

    // Zen-Kun
    bool zenKunEnabled = false;
    std::string bgPath = "";
    float bgPulse = 0.05f;
    float bgShake = 0.02f;
    float shakeTilt = 0.05f;
    float shakeZoom = 0.02f;
    std::string songTitle = "Song Title";
    std::string artistName = "Artist Name";
    bool showSongInfo = true;

    bool mediaOverlayEnabled = true;
    int mediaBackgroundType = 0;
    int mediaBackgroundFit = 0;
    std::string mediaBackgroundPath;
    float mediaBackgroundOpacity = 1.0f;
    float mediaBackgroundDimming = 0.15f;
    float mediaBackgroundScale = 1.0f;
    float mediaBackgroundOffsetX = 0.0f;
    float mediaBackgroundOffsetY = 0.0f;
    std::string mediaFontPath;
    std::string mediaLyricsFontPath;
    std::string mediaArtist = "Alan Walker ft. K-391, Tungevaag, Mangoo";
    std::string mediaTitle = "Play";
    std::string mediaLyricSource;
    std::string mediaLyricFilePath = "lyrics.lrc";
    bool mediaTopSpectrum = true;
    bool mediaBottomSpectrum = true;
    float mediaOpacity = 1.0f;
    float mediaSpectrumGain = 1.35f;
    float mediaSpectrumAmplitudeCap = 1.0f;
    int mediaSpectrumBarCount = 96;
    float mediaSpectrumBarWidth = 0.78f;
    float mediaSpectrumLineThickness = 0.0022f;
    float mediaTopSpectrumHeight = 0.027f;
    float mediaBottomSpectrumHeight = 0.035f;
    float mediaTopMargin = 0.06f;
    float mediaBottomMargin = 0.07f;
    float mediaArtistTextScale = 0.62f;
    float mediaTitleTextScale = 0.76f;
    float mediaTimestampTextScale = 0.74f;
    float mediaLyricsTextScale = 1.18f;
    bool mediaBlurredBand = true;
    float mediaBandHeight = 0.115f;
    float mediaBandOpacity = 0.72f;
    
    int audioMode; // enum cast
    std::string captureDeviceName;
    bool useSpecificCaptureDevice;
    
    // Video Render Settings
    int vidWidth;
    int vidHeight;
    int vidFps;
    int vidCodecIdx;
    int vidCrf;
    std::string vidOutputPath;

    std::vector<ConfigLayer> layers;
};

class ConfigManager {
public:
    static bool save(const std::string& filename, const AppConfig& config);
    static bool load(const std::string& filename, AppConfig& config);
    static std::string getConfigPath();
};

#endif // CONFIG_MANAGER_HPP
