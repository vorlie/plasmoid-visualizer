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

void UIManager::renderDebugInfo(AppState& state, SystemStats& systemStats, AudioEngine& audioEngine, GLFWwindow* window) {
    if (state.showDebugInfo) {
        systemStats.update(ImGui::GetIO().DeltaTime);
        ImGui::Begin("Debug Info", &state.showDebugInfo);
        
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
        ImGui::Text("VSync: %s", state.enableVsync ? "ON" : "OFF");
        if (!state.enableVsync) {
            ImGui::Text("Target FPS: %d", state.targetFps);
        }
        
        // === AUDIO STATS ===
        ImGui::Separator();
        ImGui::Text("=== Audio ===");
        const char* modeNames[] = { "File", "Live Capture", "Test Tone", "Osc Music" };
        ImGui::Text("Mode: %s", modeNames[(int)state.currentAudioMode]);
        ImGui::Text("Sample Rate: %u Hz", audioEngine.getSampleRate());
        ImGui::Text("Channels: %zu", audioEngine.getChannels());
        ImGui::Text("Playing: %s", audioEngine.isPlaying() ? "YES" : "NO");
        
        if (state.currentAudioMode == AudioMode::File) {
            float pos = audioEngine.getPosition();
            float dur = audioEngine.getDuration();
            ImGui::Text("Position: %.1f / %.1f s", pos, dur);
            if (dur > 0) {
                float progress = pos / dur;
                ImGui::ProgressBar(progress, ImVec2(-1, 0), "");
            }
        } else if (state.currentAudioMode == AudioMode::Capture) {
            if (state.useSpecificCaptureDevice) {
                ImGui::Text("Device: %s", state.selectedCaptureDeviceName);
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
        ImGui::Text("Active Layers: %zu", state.layers.size());
        if (strlen(state.filePath) > 0) {
            ImGui::Text("File: %s", state.filePath);
        }
        
        ImGui::End();
    }
}

void UIManager::renderGlobalSettings(AppState& state) {
    if (state.showGlobalSettings) {
        ImGui::Begin("Global Settings", &state.showGlobalSettings);
        ImGui::Text("Persistence / CRT Effects");
        ImGui::SliderFloat("Phosphor Decay", &state.phosphorDecay, 0.0f, 1.0f);
        ImGui::SameLine(); HelpMarker("1.0 = Instant clear (No trails). 0.01 = Long trails.");
        if (ImGui::Button("Reset Decay")) state.phosphorDecay = 0.1f;
        
        ImGui::Text("Audio Input");
        ImGui::SliderFloat("Global Gain (Pre-Amp)", &state.globalGain, 0.1f, 100.0f);
        ImGui::SameLine(); HelpMarker("Boosts input signal level. Useful for Windows WASAPI which captures at lower levels.");
        
        ImGui::Separator();
        if (ImGui::Button("Background Settings")) state.showZenKunSettings = true;

        ImGui::Separator();
        ImGui::Text("Display Settings");
        ImGui::Checkbox("Enable V-Sync", &state.enableVsync);
        if (!state.enableVsync) {
            ImGui::SliderInt("Target FPS", &state.targetFps, 15, 300);
        }
        
        ImGui::End();
    }
}

