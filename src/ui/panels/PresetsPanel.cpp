#include "UIManager.hpp"
#include "AssetPaths.hpp"
#include "UIHelpers.hpp"
#include "Utf8Paths.hpp"

#include <algorithm>
#include <filesystem>
#include <string>

void UIManager::refreshPresets() {
    m_cachedPresets = ConfigManager::listPresets(
        Utf8Paths::toUtf8(AssetPaths::presetsDirectory()),
        ConfigManager::getPresetsDirectory());
    m_presetsLoaded = true;
}

void UIManager::renderPresetsPanel(AppState& state) {
    if (!state.ui.showPresetsPanel) return;
    if (!m_presetsLoaded) {
        refreshPresets();
    }

    ImGui::SetNextWindowSize(ImVec2(420.0f, 460.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Visual Presets", &state.ui.showPresetsPanel)) {
        ImGui::End();
        return;
    }

    ImGui::TextWrapped(
        "Presets save visual layers, oscilloscope styling, beat FX intensities, and overlay layout "
        "while preserving your audio device, playlist, fonts, and lyrics.");
    if (ImGui::Button("Refresh List")) {
        refreshPresets();
    }

    auto applyPreset = [&](const std::string& path, const std::string& label) {
        if (ConfigManager::loadPreset(path, state.config)) {
            if (!state.config.layers.empty()) {
                state.ui.selectedLayerIdx = std::clamp(
                    state.ui.selectedLayerIdx, 0, static_cast<int>(state.config.layers.size()) - 1);
            } else {
                state.ui.selectedLayerIdx = 0;
            }
            state.ui.statusMessage = "Loaded preset: " + label;
            state.ui.statusColor = ImVec4(0.2f, 1.0f, 0.3f, 1.0f);
        } else {
            state.ui.statusMessage = "Failed to load preset: " + label;
            state.ui.statusColor = ImVec4(1.0f, 0.3f, 0.2f, 1.0f);
        }
    };

    ImGui::SeparatorText("Built-in Presets");
    bool hasBuiltIns = false;
    for (size_t i = 0; i < m_cachedPresets.size(); ++i) {
        const auto& entry = m_cachedPresets[i];
        if (!entry.builtIn) continue;
        hasBuiltIns = true;
        ImGui::PushID(static_cast<int>(i));
        if (ImGui::Button("Load")) {
            applyPreset(entry.path, entry.name);
        }
        ImGui::SameLine();
        ImGui::TextUnformatted(entry.name.c_str());
        ImGui::PopID();
    }
    if (!hasBuiltIns) {
        ImGui::TextDisabled("(no built-in presets found in assets/presets)");
    }

    ImGui::SeparatorText("User Presets");
    bool hasUsers = false;
    std::string pendingDeletePath;
    std::string pendingDeleteName;
    for (size_t i = 0; i < m_cachedPresets.size(); ++i) {
        const auto& entry = m_cachedPresets[i];
        if (entry.builtIn) continue;
        hasUsers = true;
        ImGui::PushID(static_cast<int>(i));
        if (ImGui::Button("Load")) {
            applyPreset(entry.path, entry.name);
        }
        ImGui::SameLine();
        if (ImGui::Button("Delete")) {
            pendingDeletePath = entry.path;
            pendingDeleteName = entry.name;
        }
        ImGui::SameLine();
        ImGui::TextUnformatted(entry.name.c_str());
        ImGui::PopID();
    }
    if (!hasUsers) {
        ImGui::TextDisabled("(no user presets saved yet)");
    }

    if (!pendingDeletePath.empty()) {
        std::error_code ec;
        if (std::filesystem::remove(Utf8Paths::fromUtf8(pendingDeletePath), ec) && !ec) {
            state.ui.statusMessage = "Deleted preset: " + pendingDeleteName;
            state.ui.statusColor = ImVec4(1.0f, 0.8f, 0.2f, 1.0f);
            refreshPresets();
        } else {
            state.ui.statusMessage = "Failed to delete preset: " + pendingDeleteName;
            state.ui.statusColor = ImVec4(1.0f, 0.3f, 0.2f, 1.0f);
        }
    }

    ImGui::SeparatorText("Save Current Look");
    UIHelpers::InputText("Preset Name", state.ui.presetSaveName);
    if (ImGui::Button("Save Preset")) {
        const std::string sanitized = ConfigManager::sanitizePresetFilename(state.ui.presetSaveName);
        const auto targetPath =
            Utf8Paths::fromUtf8(ConfigManager::getPresetsDirectory()) /
            Utf8Paths::fromUtf8(sanitized + ".toml");
        if (ConfigManager::savePreset(Utf8Paths::toUtf8(targetPath), state.config, state.ui.presetSaveName)) {
            state.ui.statusMessage = "Saved preset: " + state.ui.presetSaveName;
            state.ui.statusColor = ImVec4(0.2f, 1.0f, 0.3f, 1.0f);
            refreshPresets();
        } else {
            state.ui.statusMessage = "Failed to save preset.";
            state.ui.statusColor = ImVec4(1.0f, 0.3f, 0.2f, 1.0f);
        }
    }

    ImGui::SeparatorText("Import / Export .toml");
    UIHelpers::InputText("Preset File Path", state.ui.presetFilePath);
    if (ImGui::Button("Import File") && !state.ui.presetFilePath.empty()) {
        applyPreset(state.ui.presetFilePath, state.ui.presetFilePath);
    }
    ImGui::SameLine();
    if (ImGui::Button("Export File") && !state.ui.presetFilePath.empty()) {
        if (ConfigManager::savePreset(state.ui.presetFilePath, state.config, state.ui.presetSaveName)) {
            state.ui.statusMessage = "Exported preset to: " + state.ui.presetFilePath;
            state.ui.statusColor = ImVec4(0.2f, 1.0f, 0.3f, 1.0f);
        } else {
            state.ui.statusMessage = "Failed to export preset.";
            state.ui.statusColor = ImVec4(1.0f, 0.3f, 0.2f, 1.0f);
        }
    }

    if (!state.ui.statusMessage.empty()) {
        ImGui::Separator();
        ImGui::TextColored(state.ui.statusColor, "%s", state.ui.statusMessage.c_str());
    }

    ImGui::End();
}
