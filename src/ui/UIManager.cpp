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

void UIManager::renderUI(
    AppState& state,
    AudioEngine& audioEngine,
    ParticleSystem& particleSystem,
    OscMusicEditor& oscMusicEditor,
    SystemStats& systemStats,
    VideoRenderManager& videoRenderManager,
    GLFWwindow* window
) {
    renderMainMenu(state, audioEngine);
    if (state.showAudioSettings) renderAudioSettings(state, audioEngine, oscMusicEditor, videoRenderManager);
    if (state.showZenKunSettings) renderZenKunSettings(state);
    renderLayerManager(state);
    renderLayerEditor(state);
    renderPlaylist(state, audioEngine);
    renderParticleSettings(state, particleSystem);
    renderDebugInfo(state, systemStats, audioEngine, window);
    renderGlobalSettings(state);
    renderStatusMessage(state);
}

void UIManager::renderMainMenu(AppState& state, AudioEngine& audioEngine) {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Load Audio File...")) { state.showAudioSettings = true; }
            if (ImGui::MenuItem("Save Config")) { ConfigLogic::saveSettings(state); }
            if (ImGui::MenuItem("Exit", "Alt+F4")) { /* handeled by glfwWindowShouldClose */ }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Audio Settings", nullptr, &state.showAudioSettings);
            ImGui::MenuItem("Layer Manager", nullptr, &state.showLayerManager);
            ImGui::MenuItem("Layer Editor", nullptr, &state.showLayerEditor);
            ImGui::MenuItem("Playlist", nullptr, &state.showPlaylist);
            ImGui::MenuItem("Particle Settings", nullptr, &state.showParticleSettings);
            ImGui::MenuItem("Debug Info", nullptr, &state.showDebugInfo);
            ImGui::MenuItem("Global Settings", nullptr, &state.showGlobalSettings);
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
}

