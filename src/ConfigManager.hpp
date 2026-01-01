#ifndef CONFIG_MANAGER_HPP
#define CONFIG_MANAGER_HPP

#include <string>
#include <vector>
#include "Visualizer.hpp"

// Forward declaration of VisualizerLayer from main.cpp
// Since VisualizerLayer is defined in main.cpp, we might want to move it to a shared header
// or just define a serializable version here.
// For now, let's assume we move the struct to a header or duplicate structure for config.

struct ConfigLayer {
    std::string name;
    // LayerConfig fields
    float gain;
    float falloff;
    float minFreq;
    float maxFreq;
    size_t numBars;
    
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
    float bloom;
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
