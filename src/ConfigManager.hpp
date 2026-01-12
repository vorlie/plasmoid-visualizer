#ifndef CONFIG_MANAGER_HPP
#define CONFIG_MANAGER_HPP

#include <string>
#include <vector>
#include "Visualizer.hpp"

struct ConfigLayer {
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
    int audioChannel = 0; // 0=Mixed, 1=Left, 2=Right
    int barAnchor = 0; // 0=Bottom, 1=Top, 2=Left, 3=Right, 4=Center
};

struct AppConfig {
    std::string musicFolder;
    bool particlesEnabled;
    float beatSensitivity;
    int particleCount;
    float particleSpeed;
    float particleSize;
    float particleColor[4];
    float phosphorDecay;
    int audioMode; // enum cast
    
    std::vector<ConfigLayer> layers;
};

class ConfigManager {
public:
    static bool save(const std::string& filename, const AppConfig& config);
    static bool load(const std::string& filename, AppConfig& config);
    static std::string getConfigPath();
};

#endif // CONFIG_MANAGER_HPP
