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

    float gain = 1.0f;
    float falloff = 0.9f;
    float minFreq = 20.0f;
    float maxFreq = 20000.0f;
    float barHeight = 0.2f;
    bool liveMode = false;
    char filePath[256] = "";
    std::string statusMessage = "Ready";
    ImVec4 statusColor = ImVec4(1, 1, 1, 1);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // Audio processing
        std::vector<float> audioBuffer;
        audioEngine.getBuffer(audioBuffer, 8192);
        analysisEngine.setGain(gain);
        analysisEngine.setFalloff(falloff);
        analysisEngine.process(audioBuffer);

        // Rendering
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        visualizer.setHeightScale(barHeight);
        visualizer.render(analysisEngine.getMagnitudes());

        // UI
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Controls");
        
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
            static int selectedDevice = 0;
            
            std::vector<const char*> deviceNames;
            for (const auto& d : devices) deviceNames.push_back(d.name.c_str());

            if (ImGui::Combo("Input Device", &selectedDevice, deviceNames.data(), deviceNames.size())) {
                audioEngine.startCapture(&devices[selectedDevice].id);
            }
        } else {
            // Device Selection (Output)
            static std::vector<AudioEngine::DeviceInfo> devices;
            if (devices.empty()) devices = audioEngine.getAvailableDevices(false); // Get output devices
            
            static int selectedDevice = 0;
            if (ImGui::BeginCombo("Output Device", devices.empty() ? "No devices" : devices[selectedDevice].name.c_str())) {
                for (int i = 0; i < (int)devices.size(); i++) {
                    bool isSelected = (selectedDevice == i);
                    if (ImGui::Selectable(devices[i].name.c_str(), isSelected)) {
                        selectedDevice = i;
                        audioEngine.setDevice(&devices[i].id);
                        if (strlen(filePath) > 0) audioEngine.loadFile(filePath);
                    }
                    if (isSelected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            ImGui::InputText("File Path", filePath, sizeof(filePath));
            if (ImGui::Button("Load File")) {
                std::string path = filePath;
                if (!path.empty() && path.front() == '"') path.erase(0, 1);
                if (!path.empty() && path.back() == '"') path.pop_back();
                
                if (path.empty()) {
                    statusMessage = "Please enter a file path!";
                    statusColor = ImVec4(1, 1, 0, 1);
                } else if (audioEngine.loadFile(path)) {
                    statusMessage = "Loaded: " + path;
                    statusColor = ImVec4(0, 1, 0, 1);
                } else {
                    statusMessage = "Failed to load audio file: " + path;
                    statusColor = ImVec4(1, 0, 0, 1);
                }
            }
            ImGui::TextColored(statusColor, "%s", statusMessage.c_str());

            if (ImGui::Button("Play")) {
                audioEngine.play();
                std::cout << "Play button clicked" << std::endl;
            }
            ImGui::SameLine();
            if (ImGui::Button("Stop")) audioEngine.stop();
        }

        static bool testTone = false;
        if (ImGui::Checkbox("Test Tone (440Hz)", &testTone)) {
            audioEngine.setTestTone(testTone);
            if (testTone) audioEngine.play();
        }

        ImGui::SliderFloat("Gain", &gain, 0.1f, 200.0f);
        ImGui::SliderFloat("Falloff", &falloff, 0.5f, 0.99f);
        ImGui::SliderFloat("Bar Height", &barHeight, 0.01f, 2.0f);
        
        ImGui::Separator();
        ImGui::Text("Frequency Range Selection");
        ImGui::SliderFloat("Min Freq", &minFreq, 20.0f, 20000.0f);
        ImGui::SliderFloat("Max Freq", &maxFreq, 20.0f, 20000.0f);

        float energy = analysisEngine.getEnergyInRange(minFreq, maxFreq, 44100.0f);
        ImGui::ProgressBar(energy * 0.01f, ImVec2(-1, 0), "Energy Output");

        ImGui::Separator();
        ImGui::Text("Debug Info");
        ImGui::Text("Buffer Size: %zu", audioBuffer.size());
        
        float peak = 0;
        for (float f : audioBuffer) if (std::abs(f) > peak) peak = std::abs(f);
        ImGui::Text("Buffer Peak: %.4f", peak);
        
        const auto& mags = analysisEngine.getMagnitudes();
        float magPeak = 0;
        for (float f : mags) if (f > magPeak) magPeak = f;
        ImGui::Text("FFT Peak: %.4f", magPeak);
        
        ImGui::Text("Is Playing: %s", audioEngine.isPlaying() ? "Yes" : "No");
        
        ImGui::Text("Channels: %d", (int)audioEngine.getChannels());
        ImGui::Text("Duration: %.2f s", audioEngine.getDuration());
        ImGui::Text("Position: %.2f s", audioEngine.getPosition());

        ImGui::End();

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
