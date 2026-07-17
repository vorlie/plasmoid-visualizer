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

void UIManager::renderZenKunSettings(AppState& state) {
    if (state.showZenKunSettings) {
        ImGui::Begin("Background Settings", &state.showZenKunSettings);
        ImGui::Checkbox("Enable Background ", &state.zenKunModeEnabled);
        if (state.zenKunModeEnabled) {
            ImGui::InputText("Background Path", state.backgroundImagePath, sizeof(state.backgroundImagePath));
            ImGui::SameLine(); HelpMarker("Path to image file (JPG/PNG). Stretched to fill screen.");
            
            ImGui::Separator();
            ImGui::Text("Song Information");
            ImGui::Checkbox("Draw Info Overlay", &state.showSongInfo);
            ImGui::InputText("Title", state.songTitle, sizeof(state.songTitle));
            ImGui::InputText("Artist", state.artistName, sizeof(state.artistName));

            ImGui::Separator();
            ImGui::Text("Beat Effects");
            ImGui::SliderFloat("Beat Sensitivity", &state.beatSensitivity, 1.0f, 2.0f);
            ImGui::SliderFloat("Beat Pulse (Twitch)", &state.bgPulseIntensity, 0.0f, 0.5f);
            ImGui::SliderFloat("Shake X/Y", &state.shakeIntensity, 0.0f, 0.2f);
            ImGui::SliderFloat("Shake Tilt", &state.shakeTiltIntensity, 0.0f, 0.2f);
            ImGui::SliderFloat("Shake Zoom", &state.shakeZoomIntensity, 0.0f, 0.2f);
        }
        ImGui::End();
    }
}

void UIManager::renderStatusMessage(AppState& state) {
    // Optional: Render as an overlay or something
}
