#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include "AudioEngine.hpp"
#include "AnalysisEngine.hpp"
#include "Visualizer.hpp"
#include "ParticleSystem.hpp"
#include "OscMusicEditor.hpp"
#include "SystemStats.hpp"
#include "AppState.hpp"
#include "RenderManager.hpp"
#include "UIManager.hpp"
#include "VideoRenderManager.hpp"
#include "ConfigLogic.hpp"

#include <iostream>
#include <vector>
#include <string>

int main() {
    if (!glfwInit()) return -1;
    glfwWindowHint(GLFW_SAMPLES, 4);

    GLFWwindow* window = glfwCreateWindow(800, 800, "Plasmoid Visualizer Standalone", NULL, NULL);
    if (!window) {
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    
    glewInit();
    // glfwSwapInterval(1); // Will be set after config load

    // Setup ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // Fullscreen toggle support
    bool g_isFullscreen = false;
    int g_windowedPosX = 0, g_windowedPosY = 0, g_windowedWidth = 800, g_windowedHeight = 800;
    bool f11Prev = false;
    auto toggleFullscreen = [&](GLFWwindow* win) {
        if (!g_isFullscreen) {
            glfwGetWindowPos(win, &g_windowedPosX, &g_windowedPosY);
            glfwGetWindowSize(win, &g_windowedWidth, &g_windowedHeight);
            GLFWmonitor* monitor = glfwGetPrimaryMonitor();
            if (monitor) {
                const GLFWvidmode* mode = glfwGetVideoMode(monitor);
                if (mode) {
                    glfwSetWindowMonitor(win, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
                    g_isFullscreen = true;
                }
            }
        } else {
            glfwSetWindowMonitor(win, NULL, g_windowedPosX, g_windowedPosY, g_windowedWidth, g_windowedHeight, 0);
            g_isFullscreen = false;
        }
    };

    AudioEngine audioEngine;
    AnalysisEngine analysisEngine(8192);
    Visualizer visualizer;
    ParticleSystem particleSystem;
    OscMusicEditor oscMusicEditor;
    SystemStats systemStats;

    AppState state;
    RenderManager renderManager;
    UIManager uiManager;
    VideoRenderManager videoRenderManager;

    // Initial load
    ConfigLogic::loadSettings(state);
    
    // Apply VSync
    glfwSwapInterval(state.enableVsync ? 1 : 0);
    bool prevVsync = state.enableVsync;
    
    // Frame limiting state
    double lastTime = glfwGetTime();

    while (!glfwWindowShouldClose(window)) {
        // Runtime VSync toggle
        if (state.enableVsync != prevVsync) {
            glfwSwapInterval(state.enableVsync ? 1 : 0);
            prevVsync = state.enableVsync;
        }

        glfwPollEvents();

        // Handle F11 fullscreen toggle (edge-triggered)
        bool f11 = (glfwGetKey(window, GLFW_KEY_F11) == GLFW_PRESS);
        if (f11 && !f11Prev) {
            toggleFullscreen(window);
        }
        f11Prev = f11;

        int width, height;
        glfwGetFramebufferSize(window, &width, &height);

        // 1. Render Visualization Frame
        renderManager.renderFrame(
            state, 
            audioEngine, 
            analysisEngine, 
            visualizer, 
            particleSystem, 
            width, 
            height, 
            io.DeltaTime
        );

        // 2. Update Video Rendering (Non-blocking tick)
        if (state.videoStatus.isRendering) {
            videoRenderManager.update(
                state, 
                audioEngine, 
                analysisEngine, 
                visualizer, 
                particleSystem, 
                renderManager
            );
        }

        // 3. Render UI
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        uiManager.renderUI(
            state, 
            audioEngine, 
            particleSystem, 
            oscMusicEditor, 
            systemStats,
            videoRenderManager
        );

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
        
        // Frame limiting
        if (!state.enableVsync && state.targetFps > 0) {
            double targetFrameTime = 1.0 / (double)state.targetFps;
            while (glfwGetTime() < lastTime + targetFrameTime) {
                // Busy wait or sleep (sleep is better for CPU)
                // std::this_thread::sleep_for(std::chrono::duration<double>(targetFrameTime - (glfwGetTime() - lastTime)));
            }
            lastTime = glfwGetTime();
        } else {
             lastTime = glfwGetTime(); // Just update for reference if needed
        }
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
