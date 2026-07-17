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
            VisualizerLayer layer;
            layer.id = state.allocateLayerId();
            layer.name = "New Layer";
            state.layers.push_back(layer);
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
            auto& xy = layer.xy;
            if (ImGui::CollapsingHeader("Profile", ImGuiTreeNodeFlags_DefaultOpen)) {
                const char* profiles[] = {"Custom", "Analog Scope", "Stylized CRT"};
                int profile = static_cast<int>(xy.profile);
                if (ImGui::Combo("Profile##scope_preset", &profile, profiles, IM_ARRAYSIZE(profiles))) {
                    xy.profile = static_cast<ScopeProfile>(profile);
                    if (xy.profile == ScopeProfile::Analog) {
                        xy.traceWidth = 1.35f; xy.bloom = 0.45f; xy.beamIntensity = 0.85f;
                        xy.dwellEffect = 0.35f; xy.autoGain = false;
                        state.oscilloscopeDisplay.profile = ScopeProfile::Analog;
                        state.oscilloscopeDisplay.graticuleEnabled = true;
                    } else if (xy.profile == ScopeProfile::Stylized) {
                        xy.traceWidth = 2.5f; xy.bloom = 1.8f; xy.beamIntensity = 1.5f;
                        xy.dwellEffect = 0.15f; xy.autoGain = true;
                        state.oscilloscopeDisplay.profile = ScopeProfile::Stylized;
                    }
                }
                ImGui::SliderFloat("Acquisition Window", &xy.windowScale, 0.2f, 4.0f);
                layer.timeScale = xy.windowScale;
            }
            if (ImGui::CollapsingHeader("Acquisition")) {
                ImGui::Checkbox("Persistence", &layer.useLayerPersistence);
                xy.persistence = layer.useLayerPersistence;
                ImGui::Checkbox("Automatic Gain", &xy.autoGain);
            }
            if (ImGui::CollapsingHeader("Trigger")) {
                const char* modes[] = {"Auto", "Normal"}; const char* sources[] = {"X", "Y"}; const char* edges[] = {"Rising", "Falling"};
                int mode = static_cast<int>(xy.triggerMode), source = static_cast<int>(xy.triggerSource), edge = static_cast<int>(xy.triggerEdge);
                if (ImGui::Combo("Mode", &mode, modes, 2)) xy.triggerMode = static_cast<TriggerMode>(mode);
                if (ImGui::Combo("Source", &source, sources, 2)) xy.triggerSource = static_cast<TriggerSource>(source);
                if (ImGui::Combo("Edge", &edge, edges, 2)) xy.triggerEdge = static_cast<TriggerEdge>(edge);
                ImGui::SliderFloat("Level", &xy.triggerLevel, -1.0f, 1.0f);
                ImGui::SliderFloat("Hysteresis", &xy.triggerHysteresis, 0.0f, 0.25f);
                ImGui::SliderFloat("Holdoff (ms)", &xy.triggerHoldoffMs, 0.0f, 100.0f);
            }
            if (ImGui::CollapsingHeader("Input Conditioning")) {
                const char* coupling[] = {"DC", "AC"}; int cx = static_cast<int>(xy.couplingX), cy = static_cast<int>(xy.couplingY);
                if (ImGui::Combo("X Coupling", &cx, coupling, 2)) xy.couplingX = static_cast<CouplingMode>(cx);
                if (ImGui::Combo("Y Coupling", &cy, coupling, 2)) xy.couplingY = static_cast<CouplingMode>(cy);
                ImGui::SliderFloat("AC Cutoff (Hz)", &xy.acCutoffHz, 0.1f, 100.0f, "%.1f");
                ImGui::SliderFloat("Bandwidth (Hz)", &xy.bandwidthHz, 100.0f, 24000.0f, "%.0f");
            }
            if (ImGui::CollapsingHeader("Beam / Z")) {
                const char* zModes[] = {"Derived", "Explicit", "Derived x Explicit"}; int zMode = static_cast<int>(xy.zMode);
                ImGui::SliderFloat("Beam Intensity", &xy.beamIntensity, 0.0f, 4.0f);
                ImGui::SliderFloat("Trace Thickness", &xy.traceWidth, 0.5f, 10.0f);
                ImGui::SliderFloat("Bloom", &xy.bloom, 0.0f, 5.0f);
                ImGui::SliderFloat("Dwell Effect", &xy.dwellEffect, 0.0f, 1.0f);
                ImGui::SliderFloat("Max Connected Jump", &xy.jumpBlanking, 0.02f, 1.0f);
                ImGui::SameLine(); HelpMarker("Lower values blank more long beam jumps. Start around 0.08 to remove retrace-like connectors.");
                if (ImGui::Combo("Z Mode", &zMode, zModes, 3)) xy.zMode = static_cast<ZIntensityMode>(zMode);
                ImGui::SliderFloat("Z Gain", &xy.zGain, 0.0f, 2.0f);
                layer.traceWidth = xy.traceWidth; layer.bloom = xy.bloom; layer.velocityModulation = xy.dwellEffect; layer.xyAutoGain = xy.autoGain;
            }
            if (ImGui::CollapsingHeader("Transform")) {
                ImGui::SliderFloat("Rotation", &xy.rotationDegrees, -180.0f, 180.0f);
                ImGui::SliderFloat("X Scale", &xy.gainX, 0.1f, 5.0f);
                ImGui::SliderFloat("Y Scale", &xy.gainY, 0.1f, 5.0f);
                ImGui::SliderFloat("X Position", &xy.positionX, -1.0f, 1.0f);
                ImGui::SliderFloat("Y Position", &xy.positionY, -1.0f, 1.0f);
                ImGui::Checkbox("Invert X", &xy.invertX); ImGui::Checkbox("Invert Y", &xy.invertY);
                layer.rotation = xy.rotationDegrees; layer.xScale = xy.gainX; layer.yScale = xy.gainY; layer.xOffset = xy.positionX; layer.yOffset = xy.positionY; layer.flipX = xy.invertX; layer.flipY = xy.invertY;
            }
            if (ImGui::CollapsingHeader("Measurements")) {
                const auto& m = layer.xyMeasurements;
                ImGui::Text("X: %.2f Hz   Y: %.2f Hz   Ratio: %d:%d", m.frequencyX, m.frequencyY, m.ratioNumerator, m.ratioDenominator);
                ImGui::Text("X peak/RMS/DC: %.3f / %.3f / %.3f", m.peakX, m.rmsX, m.dcX);
                ImGui::Text("Y peak/RMS/DC: %.3f / %.3f / %.3f", m.peakY, m.rmsY, m.dcY);
                ImGui::Text("Phase: %s  %.1f deg   Confidence: %.0f%%", m.phaseValid ? "valid" : "--", m.phaseDegrees, m.confidence * 100.0f);
            }
        }

        if (false && (layer.shape == VisualizerShape::OscilloscopeXY || layer.shape == VisualizerShape::OscilloscopeXY_Clean)) {
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
            ImGui::SliderFloat("Beam Dwell Effect", &layer.velocityModulation, 0.0f, 1.0f);
            ImGui::SameLine(); HelpMarker("Dims fast beam movement slightly while keeping the trace continuous.");
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

