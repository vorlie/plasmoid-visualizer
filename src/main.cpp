#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include "AudioEngine.hpp"
#include "AnalysisEngine.hpp"
#include "Visualizer.hpp"

#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <cstring>

struct VisualizerLayer {
    std::string name;
    LayerConfig config;
    float color[4] = { 0.0f, 0.8f, 1.0f, 1.0f };
    float barHeight = 0.2f;
    std::vector<float> prevMagnitudes;
    bool visible = true;
};

int main() {
    if (!glfwInit()) return -1;

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Plasmoid Visualizer Standalone", NULL, NULL);
    if (!window) {
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glewInit();
    glfwSwapInterval(1);

    // Setup ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    AudioEngine audioEngine;
    AnalysisEngine analysisEngine(8192);
    Visualizer visualizer;

    std::vector<VisualizerLayer> layers;
    // Add default layer
    layers.push_back({"Default Layer", LayerConfig(), {0.0f, 0.8f, 1.0f, 1.0f}, 0.2f, {}, true});

    bool liveMode = false;
    char filePath[256] = "";
    std::string statusMessage = "Ready";
    ImVec4 statusColor = ImVec4(1, 1, 1, 1);

    int selectedLayerIdx = 0;

    // Window visibility
    bool showAudioSettings = true;
    bool showLayerManager = true;
    bool showLayerEditor = true;
    bool showDebugInfo = false;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // Audio processing
        std::vector<float> audioBuffer;
        audioEngine.getBuffer(audioBuffer, 8192);
        analysisEngine.computeFFT(audioBuffer);

        // Rendering
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // Render layers
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        for (auto& layer : layers) {
            if (!layer.visible) continue;
            auto magnitudes = analysisEngine.computeLayerMagnitudes(layer.config, layer.prevMagnitudes);
            visualizer.setColor(layer.color[0], layer.color[1], layer.color[2], layer.color[3]);
            visualizer.setHeightScale(layer.barHeight);
            visualizer.render(magnitudes);
        }

        // UI
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Main Menu Bar
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("Windows")) {
                ImGui::MenuItem("Audio Settings", NULL, &showAudioSettings);
                ImGui::MenuItem("Layer Manager", NULL, &showLayerManager);
                ImGui::MenuItem("Layer Editor", NULL, &showLayerEditor);
                ImGui::MenuItem("Debug Info", NULL, &showDebugInfo);
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

        // Audio Settings Window
        if (showAudioSettings) {
            ImGui::Begin("Audio Settings", &showAudioSettings);
            if (ImGui::Checkbox("Live Mode (System Audio/Mic)", &liveMode)) {
                if (liveMode) {
                    audioEngine.stop();
                    audioEngine.startCapture();
                } else {
                    audioEngine.stopCapture();
                }
            }
            ImGui::Separator();

            if (liveMode) {
                ImGui::Text("Live Mode Active");
                auto devices = audioEngine.getAvailableDevices(true);
                static int selectedCaptureDevice = 0;
                
                std::vector<const char*> deviceNames;
                for (const auto& d : devices) deviceNames.push_back(d.name.c_str());

                if (ImGui::Combo("Input Device", &selectedCaptureDevice, deviceNames.data(), deviceNames.size())) {
                    audioEngine.startCapture(&devices[selectedCaptureDevice].id);
                }
            } else {
                static std::vector<AudioEngine::DeviceInfo> outputDevices;
                if (outputDevices.empty()) outputDevices = audioEngine.getAvailableDevices(false);
                
                static int selectedOutputDevice = 0;
                if (ImGui::BeginCombo("Output Device", outputDevices.empty() ? "No devices" : outputDevices[selectedOutputDevice].name.c_str())) {
                    for (int i = 0; i < (int)outputDevices.size(); i++) {
                        bool isSelected = (selectedOutputDevice == i);
                        if (ImGui::Selectable(outputDevices[i].name.c_str(), isSelected)) {
                            selectedOutputDevice = i;
                            audioEngine.setDevice(&outputDevices[i].id);
                            if (strlen(filePath) > 0) audioEngine.loadFile(filePath);
                        }
                    }
                    ImGui::EndCombo();
                }

                ImGui::InputText("File Path", filePath, sizeof(filePath));
                if (ImGui::Button("Load File")) {
                    std::string path = filePath;
                    if (!path.empty() && path.front() == '"') path.erase(0, 1);
                    if (!path.empty() && path.back() == '"') path.pop_back();
                    
                    if (audioEngine.loadFile(path)) {
                        statusMessage = "Loaded: " + path;
                        statusColor = ImVec4(0, 1, 0, 1);
                    } else {
                        statusMessage = "Failed to load: " + path;
                        statusColor = ImVec4(1, 0, 0, 1);
                    }
                }
                ImGui::TextColored(statusColor, "%s", statusMessage.c_str());

                if (ImGui::Button("Play")) audioEngine.play();
                ImGui::SameLine();
                if (ImGui::Button("Stop")) audioEngine.stop();
            }

            static bool testTone = false;
            if (ImGui::Checkbox("Test Tone (440Hz)", &testTone)) {
                audioEngine.setTestTone(testTone);
                if (testTone) audioEngine.play();
            }
            ImGui::End();
        }

        // Layer Manager Window
        if (showLayerManager) {
            ImGui::Begin("Layer Manager", &showLayerManager);
            if (ImGui::Button("Add Layer")) {
                layers.push_back({"New Layer", LayerConfig(), {1.0f, 1.0f, 1.0f, 1.0f}, 0.2f, {}, true});
                selectedLayerIdx = layers.size() - 1;
            }
            ImGui::Separator();

            for (int i = 0; i < (int)layers.size(); i++) {
                ImGui::PushID(i);
                if (ImGui::Selectable(layers[i].name.c_str(), selectedLayerIdx == i)) {
                    selectedLayerIdx = i;
                }
                ImGui::SameLine(ImGui::GetWindowWidth() - 50);
                ImGui::Checkbox("##visible", &layers[i].visible);
                ImGui::PopID();
            }

            if (layers.size() > 1) {
                if (ImGui::Button("Remove Selected")) {
                    layers.erase(layers.begin() + selectedLayerIdx);
                    if (selectedLayerIdx >= (int)layers.size()) selectedLayerIdx = (int)layers.size() - 1;
                }
            }
            ImGui::End();
        }

        // Layer Editor Window
        if (showLayerEditor && selectedLayerIdx >= 0 && selectedLayerIdx < (int)layers.size()) {
            auto& layer = layers[selectedLayerIdx];
            ImGui::Begin("Layer Editor", &showLayerEditor);
            
            char nameBuf[64];
            strncpy(nameBuf, layer.name.c_str(), sizeof(nameBuf));
            nameBuf[sizeof(nameBuf)-1] = '\0';
            if (ImGui::InputText("Layer Name", nameBuf, sizeof(nameBuf))) {
                layer.name = nameBuf;
            }

            ImGui::ColorEdit4("Color", layer.color);
            ImGui::SliderFloat("Gain", &layer.config.gain, 0.1f, 200.0f);
            ImGui::SliderFloat("Falloff", &layer.config.falloff, 0.5f, 0.99f);
            ImGui::SliderFloat("Bar Height", &layer.barHeight, 0.01f, 2.0f);
            
            ImGui::Separator();
            ImGui::Text("Frequency Range");
            ImGui::SliderFloat("Min Freq", &layer.config.minFreq, 20.0f, 20000.0f);
            ImGui::SliderFloat("Max Freq", &layer.config.maxFreq, 20.0f, 20000.0f);

            ImGui::End();
        }

        // Debug Info Window
        if (showDebugInfo) {
            ImGui::Begin("Debug Info", &showDebugInfo);
            ImGui::Text("FPS: %.1f", io.Framerate);
            ImGui::Text("Buffer Size: %zu", audioBuffer.size());
            ImGui::Text("Is Playing: %s", audioEngine.isPlaying() ? "Yes" : "No");
            ImGui::Text("Channels: %d", (int)audioEngine.getChannels());
            ImGui::Text("Position: %.2f s", audioEngine.getPosition());
            ImGui::End();
        }

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
