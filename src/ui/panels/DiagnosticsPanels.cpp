#include "UIManager.hpp"
#include "UIHelpers.hpp"
#include "AssetPaths.hpp"
#include "Utf8Paths.hpp"

#include <GLFW/glfw3.h>

void UIManager::renderDebugInfo(AppState& state, SystemStats& systemStats, AudioEngine& audioEngine, GLFWwindow* window) {
    if (state.ui.showDebugInfo) {
        systemStats.update(ImGui::GetIO().DeltaTime);
        ImGui::Begin("Debug Info", &state.ui.showDebugInfo);

        // === RENDERING STATS ===
        ImGui::Text("=== Rendering ===");
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
        ImGui::Text("Frame Time: %.2f ms", systemStats.getFrameTimeMs());
        ImGui::Text("  Min/Max/Avg: %.2f / %.2f / %.2f ms",
            systemStats.getMinFrameTimeMs(),
            systemStats.getMaxFrameTimeMs(),
            systemStats.getAvgFrameTimeMs());

        if (window) {
            int width, height;
            glfwGetFramebufferSize(window, &width, &height);
            ImGui::Text("Resolution: %dx%d", width, height);
        }
        ImGui::Text("VSync: %s", state.config.enableVsync ? "ON" : "OFF");
        if (!state.config.enableVsync) {
            ImGui::Text("Target FPS: %d", state.config.targetFps);
        }

        // === AUDIO STATS ===
        ImGui::Separator();
        ImGui::Text("=== Audio ===");
        const char* modeNames[] = { "File", "Live Capture", "Test Tone", "Osc Music" };
        const int modeIdx = static_cast<int>(state.config.currentAudioMode);
        const char* modeLabel = (modeIdx >= 0 && modeIdx < IM_ARRAYSIZE(modeNames)) ? modeNames[modeIdx] : "Unknown";
        ImGui::Text("Mode: %s", modeLabel);
        ImGui::Text("Sample Rate: %u Hz", audioEngine.getSampleRate());
        ImGui::Text("Channels: %zu", audioEngine.getChannels());
        ImGui::Text("Playing: %s", audioEngine.isPlaying() ? "YES" : "NO");

        if (state.config.currentAudioMode == AudioMode::File) {
            float pos = audioEngine.getPosition();
            float dur = audioEngine.getDuration();
            ImGui::Text("Position: %.1f / %.1f s", pos, dur);
            if (dur > 0) {
                float progress = pos / dur;
                ImGui::ProgressBar(progress, ImVec2(-1, 0), "");
            }
        } else if (state.config.currentAudioMode == AudioMode::Capture) {
            if (state.config.useSpecificCaptureDevice) {
                ImGui::Text("Device: %s", state.config.selectedCaptureDeviceName.c_str());
            } else {
                ImGui::Text("Device: Default");
            }
        }

        // === SYSTEM RESOURCES ===
        ImGui::Separator();
        ImGui::Text("=== System Resources ===");
        ImGui::Text("CPU Cores: %d", systemStats.getCpuCoreCount());
        ImGui::Text("CPU (Global):  %.1f%%", systemStats.getGlobalCpuUsage());
        ImGui::Text("CPU (Process): %.1f%%", systemStats.getProcessCpuUsage());
        ImGui::Text("RAM Usage:     %.1f MB", systemStats.getRamUsageMB());
        ImGui::Text("RAM Peak:      %.1f MB", systemStats.getPeakRamUsageMB());

        ImGui::Text("GPU: %s", systemStats.getGpuInfo().c_str());
        if (systemStats.getGpuVramTotalMB() > 0) {
            ImGui::Text("VRAM: %.0f / %.0f MB",
                systemStats.getGpuVramUsedMB(),
                systemStats.getGpuVramTotalMB());
        }

        // === APPLICATION STATE ===
        ImGui::Separator();
        ImGui::Text("=== Application ===");
        ImGui::Text("Active Layers: %zu", state.config.layers.size());
        if (!state.fx.filePath.empty()) {
            ImGui::Text("File: %s", state.fx.filePath.c_str());
        }

        // === SHADERS ===
        ImGui::Separator();
        ImGui::Text("=== Shaders ===");
        const std::string shaderDir = Utf8Paths::toUtf8(AssetPaths::shaderDirectory());
        ImGui::TextWrapped("Dir: %s", shaderDir.c_str());
        ImGui::Checkbox("Auto Hot-Reload Shaders", &state.ui.shaderHotReloadEnabled);
        if (ImGui::Button("Reload Shaders (F5)")) {
            state.ui.requestShaderReload = true;
        }
        ImGui::SameLine();
        ImGui::Text("Reloads: %d", state.ui.shaderReloadCount);
        if (!state.ui.shaderReloadStatus.empty()) {
            const ImVec4 color = state.ui.shaderReloadError
                ? ImVec4(1.0f, 0.35f, 0.35f, 1.0f)
                : ImVec4(0.35f, 1.0f, 0.45f, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_Text, color);
            ImGui::TextWrapped("%s", state.ui.shaderReloadStatus.c_str());
            ImGui::PopStyleColor();
        }

        ImGui::End();
    }
}

void UIManager::renderGlobalSettings(AppState& state) {
    if (state.ui.showGlobalSettings) {
        ImGui::Begin("Global Settings", &state.ui.showGlobalSettings);
        if (ImGui::CollapsingHeader("Oscilloscope Display", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto& scope = state.config.oscilloscopeDisplay;
            ImGui::Checkbox("Graticule", &scope.graticuleEnabled);
            ImGui::SliderFloat("Graticule Opacity", &scope.graticuleOpacity, 0.0f, 1.0f);
            ImGui::SliderInt("Grid Columns", &scope.graticuleColumns, 1, 20);
            ImGui::SliderInt("Grid Rows", &scope.graticuleRows, 1, 20);
            ImGui::SliderFloat("Fast Decay (ms)", &scope.phosphorFastDecayMs, 5.0f, 500.0f);
            ImGui::SliderFloat("Slow Decay (ms)", &scope.phosphorSlowDecayMs, 20.0f, 5000.0f);
            ImGui::SliderFloat("Slow Phosphor Weight", &scope.phosphorSlowWeight, 0.0f, 1.5f);
            ImGui::SliderFloat("Phosphor Saturation", &scope.phosphorSaturation, 0.0f, 2.5f);
            ImGui::SliderFloat("Decay Color Shift", &scope.decayColorShift, -1.0f, 1.0f);
            ImGui::SameLine(); HelpMarker("Shifts the slow phosphor afterglow hue as it decays (classic P31/P7 CRT trail tint).");
            ImGui::Checkbox("Measurement HUD", &scope.measurementOverlay);
            if (scope.measurementOverlay) {
                ImGui::Indent();
                ImGui::Checkbox("Include HUD in Video Export", &scope.overlayInVideo);
                ImGui::Unindent();
            }
        }

        ImGui::Text("Audio Input");
        ImGui::SliderFloat("Global Gain (Pre-Amp)", &state.config.globalGain, 0.1f, 100.0f);
        ImGui::SameLine(); HelpMarker("Boosts input signal level. Useful for Windows WASAPI which captures at lower levels.");

        ImGui::Separator();
        if (ImGui::Button("Background Settings")) state.ui.showZenKunSettings = true;

        ImGui::Separator();
        ImGui::Text("Display Settings");
        ImGui::Checkbox("Enable V-Sync", &state.config.enableVsync);
        if (!state.config.enableVsync) {
            ImGui::SliderInt("Target FPS", &state.config.targetFps, 15, 300);
        }

        ImGui::End();
    }
}
