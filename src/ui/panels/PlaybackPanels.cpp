#include "UIManager.hpp"
#include "UIHelpers.hpp"
#include "ConfigLogic.hpp"
#include "Utf8Paths.hpp"

#include <filesystem>
#include <string>

namespace fs = std::filesystem;

void UIManager::renderPlaylist(AppState& state, AudioEngine& audioEngine) {
    if (state.ui.showPlaylist) {
        ImGui::Begin("Playlist", &state.ui.showPlaylist);
        if (UIHelpers::InputText("Music Folder", state.config.musicFolderPath)) {
            ConfigLogic::scanMusicFolder(state, state.config.musicFolderPath);
        }
        ImGui::SameLine();
        if (ImGui::Button("Refresh")) {
            ConfigLogic::scanMusicFolder(state, state.config.musicFolderPath);
        }
        if (ImGui::Checkbox("Loop Current Track", &state.config.loopTrack)) {
            audioEngine.setLoopTrack(state.config.loopTrack);
        }
        ImGui::SameLine();
        HelpMarker("Uncheck to automatically play the next track in this list when a track finishes.");

        ImGui::Separator();
        if (ImGui::BeginChild("FileList")) {
            const std::string currentFilename = Utf8Paths::toUtf8(Utf8Paths::fromUtf8(state.fx.filePath).filename());
            for (const auto& file : state.fx.musicFiles) {
                const bool isSelected = !currentFilename.empty() && currentFilename == file;
                if (ImGui::Selectable(file.c_str(), isSelected)) {
                    const fs::path fullPath =
                        Utf8Paths::fromUtf8(state.config.musicFolderPath) /
                        Utf8Paths::fromUtf8(file);
                    state.fx.filePath = Utf8Paths::toUtf8(fullPath);
                    if (audioEngine.loadFile(state.fx.filePath)) {
                        state.config.currentAudioMode = AudioMode::File;
                        audioEngine.play();
                        state.ui.statusMessage = "Playing: " + file;
                        state.ui.statusColor = ImVec4(0, 1, 0, 1);
                    } else {
                        state.ui.statusMessage = "Failed to load: " + file;
                        state.ui.statusColor = ImVec4(1, 0, 0, 1);
                    }
                }
            }
        }
        ImGui::EndChild();
        ImGui::End();
    }
}
