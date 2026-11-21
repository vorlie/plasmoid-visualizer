#ifndef CONFIG_MANAGER_HPP
#define CONFIG_MANAGER_HPP

#include "PersistentConfig.hpp"
#include <string>
#include <vector>

struct PresetEntry {
    std::string name;
    std::string path;
    bool builtIn = false;
};

class ConfigManager {
public:
    static bool save(const std::string& filename, const PersistentConfig& config);
    static bool load(const std::string& filename, PersistentConfig& config);

    static bool savePreset(
        const std::string& filename,
        const PersistentConfig& config,
        const std::string& presetName = {});
    static bool loadPreset(const std::string& filename, PersistentConfig& config);

    static std::vector<PresetEntry> listPresets(
        const std::string& builtInDir = {},
        const std::string& userDir = {});
    static std::string resolvePresetPath(
        const std::string& nameOrPath,
        const std::string& builtInDir = {},
        const std::string& userDir = {});
    static bool deleteUserPreset(
        const std::string& presetName,
        const std::string& userDir = {});
    static std::string sanitizePresetFilename(const std::string& name);

    static std::string getConfigDirectory();
    static std::string getPresetsDirectory();
    static std::string getConfigPath();
    static std::string getImGuiIniPath();
};

#endif // CONFIG_MANAGER_HPP
