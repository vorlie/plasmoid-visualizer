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

void UIManager::renderLayerManager(AppState& state) {
    if (state.showLayerManager) {
        ImGui::Begin("Layer Manager", &state.showLayerManager);
        if (ImGui::Button("Add Layer")) {
            state.layers.push_back({"New Layer", LayerConfig(), {1.0f, 1.0f, 1.0f, 1.0f}, 0.2f, false, VisualizerShape::Bars, 0.0f, {}, true});
            state.selectedLayerIdx = (int)state.layers.size() - 1;
        }
        ImGui::Separator();
        for (int i = 0; i < (int)state.layers.size(); i++) {
            ImGui::PushID(i);
            
            // 1. Visibility toggle (Left) - Always visible and easy to click
            ImGui::Checkbox("##visible", &state.layers[i].visible);
            ImGui::SameLine();

            // 2. Layer name / selection (Center)
            // Reserve space for reordering buttons on the right
            float btnSize = ImGui::GetFrameHeight();
            float spacing = ImGui::GetStyle().ItemSpacing.x;
            float reservedWidth = (btnSize + spacing) * 2 + spacing;
            
            if (ImGui::Selectable(state.layers[i].name.c_str(), state.selectedLayerIdx == i, 0, ImVec2(ImGui::GetContentRegionAvail().x - reservedWidth, 0))) {
                state.selectedLayerIdx = i;
            }
            ImGui::SameLine();

            // 3. Reordering buttons (Right)
            if (ImGui::Button("^", ImVec2(btnSize, 0))) {
                if (i > 0) {
                    std::swap(state.layers[i], state.layers[i-1]);
                    if (state.selectedLayerIdx == i) state.selectedLayerIdx = i - 1;
                    else if (state.selectedLayerIdx == i - 1) state.selectedLayerIdx = i;
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("v", ImVec2(btnSize, 0))) {
                if (i < (int)state.layers.size() - 1) {
                    std::swap(state.layers[i], state.layers[i+1]);
                    if (state.selectedLayerIdx == i) state.selectedLayerIdx = i + 1;
                    else if (state.selectedLayerIdx == i + 1) state.selectedLayerIdx = i;
                }
            }
            
            ImGui::PopID();
        }
        if (state.layers.size() > 1) {
            if (ImGui::Button("Remove Selected")) {
                state.layers.erase(state.layers.begin() + state.selectedLayerIdx);
                if (state.selectedLayerIdx >= (int)state.layers.size()) state.selectedLayerIdx = (int)state.layers.size() - 1;
            }
        }
        ImGui::End();
    }
}

void UIManager::renderLayerEditor(AppState& state) {
    if (state.showLayerEditor && state.selectedLayerIdx >= 0 && state.selectedLayerIdx < (int)state.layers.size()) {
        auto& layer = state.layers[state.selectedLayerIdx];
        ImGui::Begin("Layer Editor", &state.showLayerEditor);
        
        char nameBuf[64];
        strncpy(nameBuf, layer.name.c_str(), sizeof(nameBuf));
        nameBuf[sizeof(nameBuf)-1] = '\0';
        if (ImGui::InputText("Layer Name", nameBuf, sizeof(nameBuf))) layer.name = nameBuf;

        ImGui::ColorEdit4("Color", layer.color);
        ImGui::SliderFloat("Gain", &layer.config.gain, 0.1f, 200.0f);
        ImGui::SliderFloat("Falloff (Decay)", &layer.config.falloff, 0.5f, 0.999f);
        ImGui::SliderFloat("Attack", &layer.config.attack, 0.0f, 1.0f);
        ImGui::SliderInt("Smoothing (Radius)", &layer.config.smoothing, 0, 10);
        ImGui::SliderFloat("Spectrum Power (Log)", &layer.config.spectrumPower, 0.1f, 3.0f);
        ImGui::SliderFloat("Bar Height", &layer.barHeight, 0.01f, 2.0f);
        
        ImGui::Separator(); ImGui::Text("Frequency Range");
        ImGui::SliderFloat("Min Freq", &layer.config.minFreq, 20.0f, 20000.0f);
        ImGui::SliderFloat("Max Freq", &layer.config.maxFreq, 20.0f, 20000.0f);

        ImGui::Separator(); ImGui::Text("Visuals");
        ImGui::Checkbox("Mirror Mode", &layer.mirrored);
        
        const char* shapes[] = { "Bars", "Lines", "Dots", "Waveform", "Oscilloscope XY", "Curve", "Oscilloscope Clean" };
        int shapeIdx = (int)layer.shape;
        if (ImGui::Combo("Shape", &shapeIdx, shapes, IM_ARRAYSIZE(shapes))) layer.shape = (VisualizerShape)shapeIdx;
        
        if (layer.shape == VisualizerShape::Bars) ImGui::SliderFloat("Corner Radius", &layer.cornerRadius, 0.0f, 1.0f);
        if (layer.shape == VisualizerShape::Waveform || layer.shape == VisualizerShape::OscilloscopeXY || layer.shape == VisualizerShape::OscilloscopeXY_Clean) 
            ImGui::SliderFloat("Time Scale", &layer.timeScale, 0.2f, 2.0f);
        
        if (layer.shape == VisualizerShape::OscilloscopeXY || layer.shape == VisualizerShape::OscilloscopeXY_Clean) {
            ImGui::SliderFloat("Bloom Intensity", &layer.bloom, 0.0f, 5.0f);
            
            if (ImGui::TreeNode("Transform")) {
                ImGui::SliderFloat("Rotation", &layer.rotation, 0.0f, 360.0f);
                ImGui::SliderFloat("X Scale", &layer.xScale, 0.1f, 5.0f);
                ImGui::SliderFloat("Y Scale", &layer.yScale, 0.1f, 5.0f);
                ImGui::Checkbox("Flip Horizontal", &layer.flipX);
                ImGui::Checkbox("Flip Vertical", &layer.flipY);
                ImGui::Checkbox("Persistence", &layer.useLayerPersistence);
                
                ImGui::Separator();
                ImGui::Text("Offset Pad");
                ImVec2 padSize = ImVec2(ImGui::GetContentRegionAvail().x, 150);
                ImVec2 cursor = ImGui::GetCursorScreenPos();
                ImGui::InvisibleButton("##offset_pad", padSize);
                if (ImGui::IsItemActive()) {
                    ImVec2 mousePos = ImGui::GetIO().MousePos;
                    layer.xOffset = (mousePos.x - cursor.x) / padSize.x * 2.0f - 1.0f;
                    layer.yOffset = (mousePos.y - cursor.y) / padSize.y * 2.0f - 1.0f; // Note: top-down
                }
                
                ImDrawList* drawList = ImGui::GetWindowDrawList();
                drawList->AddRectFilled(cursor, ImVec2(cursor.x + padSize.x, cursor.y + padSize.y), IM_COL32(30, 30, 30, 255));
                drawList->AddRect(cursor, ImVec2(cursor.x + padSize.x, cursor.y + padSize.y), IM_COL32(100, 100, 100, 255));
                drawList->AddLine(ImVec2(cursor.x + padSize.x/2, cursor.y), ImVec2(cursor.x + padSize.x/2, cursor.y + padSize.y), IM_COL32(60, 60, 60, 255));
                drawList->AddLine(ImVec2(cursor.x, cursor.y + padSize.y/2), ImVec2(cursor.x + padSize.x, cursor.y + padSize.y/2), IM_COL32(60, 60, 60, 255));
                
                ImVec2 dotPos = ImVec2(
                    cursor.x + (layer.xOffset + 1.0f) / 2.0f * padSize.x,
                    cursor.y + (layer.yOffset + 1.0f) / 2.0f * padSize.y
                );
                drawList->AddCircleFilled(dotPos, 6.0f, IM_COL32(230, 230, 230, 255));
                drawList->AddCircle(dotPos, 8.0f, IM_COL32(255, 255, 255, 100));

                ImGui::SliderFloat("X Offset", &layer.xOffset, -1.0f, 1.0f);
                ImGui::SliderFloat("Y Offset", &layer.yOffset, -1.0f, 1.0f);
                
                if (ImGui::Button("Reset Transform")) {
                    layer.xOffset = 0; layer.yOffset = 0;
                    layer.xScale = 1; layer.yScale = 1;
                    layer.rotation = 0;
                }
                ImGui::TreePop();
            }

            ImGui::Separator();
            ImGui::SliderFloat("Trace Thickness", &layer.traceWidth, 1.0f, 10.0f);
            ImGui::SliderFloat("Beam Head Size", &layer.beamHeadSize, 0.0f, 30.0f);
            ImGui::SliderFloat("Velocity Brightness", &layer.velocityModulation, 0.0f, 2.0f);
            ImGui::Checkbox("Automatic XY Gain", &layer.xyAutoGain);
        }
        if (layer.shape == VisualizerShape::Curve) {
             ImGui::SliderFloat("Trace Thickness", &layer.traceWidth, 1.0f, 10.0f);
             ImGui::SliderFloat("Fill Opacity", &layer.fillOpacity, 0.0f, 1.0f);
        }
        if (layer.shape != VisualizerShape::OscilloscopeXY && layer.shape != VisualizerShape::OscilloscopeXY_Clean) {
            ImGui::Separator(); ImGui::Text("Audio Channel");
            const char* channels[] = {"Mixed", "Left", "Right"};
            int chanIdx = (int)layer.channel;
            if (ImGui::Combo("Channel", &chanIdx, channels, 3)) layer.channel = (AudioChannel)chanIdx;
        }
        if (layer.shape == VisualizerShape::Bars || layer.shape == VisualizerShape::Lines || layer.shape == VisualizerShape::Dots) {
            const char* anchors[] = {"Bottom", "Top", "Left", "Right", "Center"};
            int anchorIdx = (int)layer.barAnchor;
            if (ImGui::Combo("Bar Position", &anchorIdx, anchors, 5)) layer.barAnchor = (BarAnchor)anchorIdx;
        }
        ImGui::End();
    }
}

