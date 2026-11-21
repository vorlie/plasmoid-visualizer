#include "UIManager.hpp"
#include "UIHelpers.hpp"

void UIManager::renderZenKunSettings(AppState& state) {
    if (state.ui.showZenKunSettings) {
        ImGui::Begin("Background Settings", &state.ui.showZenKunSettings);
        ImGui::Checkbox("Enable Background ", &state.config.zenKunModeEnabled);
        if (state.config.zenKunModeEnabled) {
            UIHelpers::InputText("Background Path", state.config.backgroundImagePath);
            ImGui::SameLine(); HelpMarker("Path to image file (JPG/PNG). Stretched to fill screen.");

            ImGui::Separator();
            ImGui::Text("Beat Effects");
            ImGui::SliderFloat("Beat Sensitivity", &state.config.beatSensitivity, 1.0f, 2.0f);
            ImGui::SliderFloat("Beat Pulse (Twitch)", &state.config.bgPulseIntensity, 0.0f, 0.5f);
            ImGui::SliderFloat("Shake X/Y", &state.config.shakeIntensity, 0.0f, 0.2f);
            ImGui::SliderFloat("Shake Tilt", &state.config.shakeTiltIntensity, 0.0f, 0.2f);
            ImGui::SliderFloat("Shake Zoom", &state.config.shakeZoomIntensity, 0.0f, 0.2f);
        }
        ImGui::End();
    }
}

void UIManager::renderStatusMessage(AppState& state) {
    (void)state;
}
