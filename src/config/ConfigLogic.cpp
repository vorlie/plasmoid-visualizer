#include "ConfigLogic.hpp"
#include "ConfigManager.hpp"
#include "Utf8Paths.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

void ConfigLogic::scanMusicFolder(AppState& state, const std::string& path) {
    state.fx.musicFiles.clear();
    if (path.empty()) return;
    try {
        const fs::path folder = Utf8Paths::fromUtf8(path);
        std::error_code ec;
        if (fs::exists(folder, ec) && !ec && fs::is_directory(folder, ec) && !ec) {
            for (const auto& entry : fs::directory_iterator(
                     folder, fs::directory_options::skip_permission_denied, ec)) {
                if (ec) break;
                if (entry.is_regular_file(ec) && !ec) {
                    std::string ext = Utf8Paths::toUtf8(entry.path().extension());
                    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
                        return static_cast<char>(std::tolower(c));
                    });
                    if (ext == ".mp3" || ext == ".wav" || ext == ".flac" || ext == ".ogg") {
                        state.fx.musicFiles.push_back(Utf8Paths::toUtf8(entry.path().filename()));
                    }
                }
            }
            std::sort(state.fx.musicFiles.begin(), state.fx.musicFiles.end());
        }
    } catch (const std::exception& e) {
        std::cerr << "Error scanning folder: " << e.what() << std::endl;
    }
}

void ConfigLogic::saveSettings(const AppState& state) {
    if (ConfigManager::save("", state.config)) {
        std::cout << "Settings saved to " << ConfigManager::getConfigPath() << std::endl;
    }
}

void ConfigLogic::loadSettings(AppState& state) {
    if (ConfigManager::load("", state.config)) {
        if (!state.config.layers.empty()) {
            state.ui.selectedLayerIdx = std::clamp(
                state.ui.selectedLayerIdx, 0, static_cast<int>(state.config.layers.size()) - 1);
        } else {
            state.ui.selectedLayerIdx = 0;
        }
        scanMusicFolder(state, state.config.musicFolderPath);
    }
}
