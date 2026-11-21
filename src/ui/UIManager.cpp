#include "UIManager.hpp"
#include "ConfigLogic.hpp"

#include <GLFW/glfw3.h>

void UIManager::renderUI(
    AppState& state,
    AudioEngine& audioEngine,
    OscMusicEditor& oscMusicEditor,
    SystemStats& systemStats,
    VideoRenderManager& videoRenderManager,
    GLFWwindow* window
) {
    renderMainMenu(state, audioEngine, window);
    if (state.ui.showAudioSettings) renderAudioSettings(state, audioEngine, oscMusicEditor, videoRenderManager);
    if (state.ui.showZenKunSettings) renderZenKunSettings(state);
    renderLayerManager(state);
    renderLayerEditor(state);
    renderPlaylist(state, audioEngine);
    renderDebugInfo(state, systemStats, audioEngine, window);
    renderGlobalSettings(state);
    renderLyricsEditor(state, audioEngine);
    renderPresetsPanel(state);
    renderStatusMessage(state);
}

void UIManager::renderMainMenu(AppState& state, AudioEngine& audioEngine, GLFWwindow* window) {
    (void)audioEngine;
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Load Audio File...")) { state.ui.showAudioSettings = true; }
            if (ImGui::MenuItem("Presets...")) { state.ui.showPresetsPanel = true; }
            if (ImGui::MenuItem("Save Config")) { ConfigLogic::saveSettings(state); }
            if (ImGui::MenuItem("Exit", "Alt+F4")) {
                if (window) glfwSetWindowShouldClose(window, GLFW_TRUE);
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Audio Settings", nullptr, &state.ui.showAudioSettings);
            ImGui::MenuItem("Layer Manager", nullptr, &state.ui.showLayerManager);
            ImGui::MenuItem("Layer Editor", nullptr, &state.ui.showLayerEditor);
            ImGui::MenuItem("Playlist", nullptr, &state.ui.showPlaylist);
            ImGui::MenuItem("Presets", nullptr, &state.ui.showPresetsPanel);
            ImGui::MenuItem("Debug Info", nullptr, &state.ui.showDebugInfo);
            ImGui::MenuItem("Global Settings", nullptr, &state.ui.showGlobalSettings);
            ImGui::MenuItem("Media Overlay", nullptr, &state.ui.showLyricsEditor);
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
}
