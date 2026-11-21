#ifndef CONFIG_LOGIC_HPP
#define CONFIG_LOGIC_HPP

#include "AppState.hpp"
#include <string>

class ConfigLogic {
public:
    static void scanMusicFolder(AppState& state, const std::string& path);
    static void saveSettings(const AppState& state);
    static void loadSettings(AppState& state);
};

#endif // CONFIG_LOGIC_HPP
