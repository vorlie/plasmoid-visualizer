#ifndef CONFIG_LOGIC_HPP
#define CONFIG_LOGIC_HPP

#include "AppState.hpp"
#include <string>
#include <vector>

class ConfigLogic {
public:
    static void scanMusicFolder(AppState& state, const char* path);
    static void saveSettings(const AppState& state);
    static void loadSettings(AppState& state);
};

#endif // CONFIG_LOGIC_HPP
