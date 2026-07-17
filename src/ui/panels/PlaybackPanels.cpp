#include "UIManager.hpp"
#include "UIHelpers.hpp"
#include "ConfigLogic.hpp"
#include "RenderManager.hpp"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <iostream>
#include <cstring>
#include <vector>
#include <filesystem>
#include "VideoRenderManager.hpp"

namespace fs = std::filesystem;

void UIManager::renderPlaylist(AppState& state, AudioEngine& audioEngine) {
    if (state.showPlaylist) {
        ImGui::Begin("Playlist", &state.showPlaylist);
        if (ImGui::InputText("Music Folder", state.musicFolderPath, sizeof(state.musicFolderPath))) ConfigLogic::scanMusicFolder(state, state.musicFolderPath);
        ImGui::SameLine();
        if (ImGui::Button("Refresh")) ConfigLogic::scanMusicFolder(state, state.musicFolderPath);

        ImGui::Separator();
        if (ImGui::BeginChild("FileList")) {
            for (const auto& file : state.musicFiles) {
                bool isSelected = (std::string(state.filePath).find(file) != std::string::npos);
                if (ImGui::Selectable(file.c_str(), isSelected)) {
                    fs::path fullPath = fs::path(state.musicFolderPath) / file;
                    strncpy(state.filePath, fullPath.string().c_str(), sizeof(state.filePath));
                    if (audioEngine.loadFile(state.filePath)) {
                        state.currentAudioMode = AudioMode::File;
                        audioEngine.play();
                        state.statusMessage = "Playing: " + file;
                        state.statusColor = ImVec4(0, 1, 0, 1);
                    } else {
                        state.statusMessage = "Failed to load: " + file;
                        state.statusColor = ImVec4(1, 0, 0, 1);
                    }
                }
            }
        }
        ImGui::EndChild(); ImGui::End();
    }
}

void UIManager::renderParticleSettings(AppState& state, ParticleSystem& particleSystem) {
    if (state.showParticleSettings) {
        ImGui::Begin("Particle Settings", &state.showParticleSettings);
        ImGui::Checkbox("Enable Particles", &state.particlesEnabled);
        if (state.particlesEnabled) {
            ImGui::Separator(); ImGui::Text("Beat Detection");
            ImGui::SliderFloat("Sensitivity", &state.beatSensitivity, 1.0f, 2.0f);
            ImGui::Separator(); ImGui::Text("Particle Physics");
            ImGui::SliderInt("Max Count", &state.particleCount, 100, 2000);
            ImGui::SliderFloat("Speed/Energy", &state.particleSpeed, 0.1f, 5.0f);
            ImGui::SliderFloat("Size", &state.particleSize, 1.0f, 20.0f);
            ImGui::ColorEdit4("Color", state.particleColor);
        }
        ImGui::End();
    }
}

