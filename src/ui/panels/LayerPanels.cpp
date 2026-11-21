#include "UIManager.hpp"
#include "UIHelpers.hpp"

#include <algorithm>
#include <utility>

void UIManager::renderLayerManager(AppState& state) {
    if (state.ui.showLayerManager) {
        auto& layers = state.config.layers;
        ImGui::Begin("Layer Manager", &state.ui.showLayerManager);
        if (ImGui::Button("Add Layer")) {
            VisualizerLayer layer;
            layer.id = state.allocateLayerId();
            layer.name = "New Layer";
            layers.push_back(layer);
            state.ui.selectedLayerIdx = static_cast<int>(layers.size()) - 1;
        }
        ImGui::Separator();
        ImGui::PushID("media_overlay_layer");
        ImGui::Checkbox("##visible", &state.config.mediaOverlay.enabled);
        ImGui::SameLine();
        if (ImGui::Selectable("Media Overlay  (always on top)", state.ui.showLyricsEditor)) {
            state.ui.showLyricsEditor = true;
        }
        ImGui::PopID();
        ImGui::Separator();
        for (int i = 0; i < static_cast<int>(layers.size()); i++) {
            ImGui::PushID(i);

            ImGui::Checkbox("##visible", &layers[i].visible);
            ImGui::SameLine();

            float btnSize = ImGui::GetFrameHeight();
            float spacing = ImGui::GetStyle().ItemSpacing.x;
            float reservedWidth = (btnSize + spacing) * 2 + spacing;

            if (ImGui::Selectable(layers[i].name.c_str(), state.ui.selectedLayerIdx == i, 0, ImVec2(ImGui::GetContentRegionAvail().x - reservedWidth, 0))) {
                state.ui.selectedLayerIdx = i;
            }
            ImGui::SameLine();

            if (ImGui::Button("^", ImVec2(btnSize, 0))) {
                if (i > 0) {
                    std::swap(layers[i], layers[i - 1]);
                    if (state.ui.selectedLayerIdx == i) state.ui.selectedLayerIdx = i - 1;
                    else if (state.ui.selectedLayerIdx == i - 1) state.ui.selectedLayerIdx = i;
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("v", ImVec2(btnSize, 0))) {
                if (i < static_cast<int>(layers.size()) - 1) {
                    std::swap(layers[i], layers[i + 1]);
                    if (state.ui.selectedLayerIdx == i) state.ui.selectedLayerIdx = i + 1;
                    else if (state.ui.selectedLayerIdx == i + 1) state.ui.selectedLayerIdx = i;
                }
            }

            ImGui::PopID();
        }
        if (layers.size() > 1 && state.ui.selectedLayerIdx >= 0 && state.ui.selectedLayerIdx < static_cast<int>(layers.size())) {
            if (ImGui::Button("Remove Selected")) {
                layers.erase(layers.begin() + state.ui.selectedLayerIdx);
                if (state.ui.selectedLayerIdx >= static_cast<int>(layers.size())) {
                    state.ui.selectedLayerIdx = static_cast<int>(layers.size()) - 1;
                }
            }
        }
        ImGui::End();
    }
}

void UIManager::renderLayerEditor(AppState& state) {
    auto& layers = state.config.layers;
    if (state.ui.showLayerEditor && state.ui.selectedLayerIdx >= 0 && state.ui.selectedLayerIdx < static_cast<int>(layers.size())) {
        auto& layer = layers[state.ui.selectedLayerIdx];
        ImGui::Begin("Layer Editor", &state.ui.showLayerEditor);

        UIHelpers::InputText("Layer Name", layer.name);

        ImGui::ColorEdit4("Color", layer.color);
        ImGui::SliderFloat("Gain", &layer.config.gain, 0.1f, 200.0f);
        ImGui::SliderFloat("Falloff (Decay)", &layer.config.falloff, 0.5f, 0.999f);
        ImGui::SliderFloat("Attack", &layer.config.attack, 0.0f, 1.0f);
        ImGui::SliderInt("Smoothing (Radius)", &layer.config.smoothing, 0, 10);
        ImGui::SliderFloat("Spectrum Power (Log)", &layer.config.spectrumPower, 0.1f, 3.0f);
        ImGui::SliderFloat("Bar Height", &layer.barHeight, 0.01f, 2.0f);

        ImGui::Separator();
        ImGui::Text("Frequency Range");
        ImGui::SliderFloat("Min Freq", &layer.config.minFreq, 20.0f, 20000.0f);
        ImGui::SliderFloat("Max Freq", &layer.config.maxFreq, 20.0f, 20000.0f);
        if (layer.config.maxFreq <= layer.config.minFreq) {
            layer.config.maxFreq = std::min(20000.0f, layer.config.minFreq + 10.0f);
        }

        ImGui::Separator();
        ImGui::Text("Visuals");
        ImGui::Checkbox("Mirror Mode", &layer.mirrored);

        const char* shapes[] = { "Bars", "Lines", "Dots", "Waveform", "Oscilloscope XY", "Curve" };
        if (layer.shape == VisualizerShape::OscilloscopeXY_Clean) {
            layer.shape = VisualizerShape::OscilloscopeXY;
        }
        int shapeIdx = std::clamp(static_cast<int>(layer.shape), 0, IM_ARRAYSIZE(shapes) - 1);
        if (ImGui::Combo("Shape", &shapeIdx, shapes, IM_ARRAYSIZE(shapes))) layer.shape = static_cast<VisualizerShape>(shapeIdx);

        if (layer.shape == VisualizerShape::Bars) ImGui::SliderFloat("Corner Radius", &layer.cornerRadius, 0.0f, 1.0f);

        if (layer.shape == VisualizerShape::OscilloscopeXY) {
            auto& xy = layer.xy;
            if (ImGui::CollapsingHeader("Profile", ImGuiTreeNodeFlags_DefaultOpen)) {
                const char* profiles[] = {"Custom", "Analog Scope", "Stylized CRT"};
                int profile = std::clamp(static_cast<int>(xy.profile), 0, IM_ARRAYSIZE(profiles) - 1);
                if (ImGui::Combo("Profile##scope_preset", &profile, profiles, IM_ARRAYSIZE(profiles))) {
                    xy.profile = static_cast<ScopeProfile>(profile);
                    if (xy.profile == ScopeProfile::Analog) {
                        xy.traceWidth = 1.35f;
                        xy.bloom = 0.45f;
                        xy.beamIntensity = 0.85f;
                        xy.dwellEffect = 0.35f;
                        xy.autoGain = false;
                        state.config.oscilloscopeDisplay.profile = ScopeProfile::Analog;
                        state.config.oscilloscopeDisplay.graticuleEnabled = true;
                    } else if (xy.profile == ScopeProfile::Stylized) {
                        xy.traceWidth = 2.5f;
                        xy.bloom = 1.8f;
                        xy.beamIntensity = 1.5f;
                        xy.dwellEffect = 0.15f;
                        xy.autoGain = true;
                        state.config.oscilloscopeDisplay.profile = ScopeProfile::Stylized;
                    }
                }
                ImGui::SliderFloat("Acquisition Window", &xy.windowScale, 0.2f, 4.0f);
            }
            if (ImGui::CollapsingHeader("Acquisition")) {
                ImGui::Checkbox("Persistence", &xy.persistence);
                ImGui::Checkbox("Automatic Gain", &xy.autoGain);
            }
            if (ImGui::CollapsingHeader("Trigger")) {
                const char* modes[] = {"Auto", "Normal"};
                const char* sources[] = {"X", "Y"};
                const char* edges[] = {"Rising", "Falling"};
                int mode = std::clamp(static_cast<int>(xy.triggerMode), 0, 1);
                int source = std::clamp(static_cast<int>(xy.triggerSource), 0, 1);
                int edge = std::clamp(static_cast<int>(xy.triggerEdge), 0, 1);
                if (ImGui::Combo("Mode", &mode, modes, 2)) xy.triggerMode = static_cast<TriggerMode>(mode);
                if (ImGui::Combo("Source", &source, sources, 2)) xy.triggerSource = static_cast<TriggerSource>(source);
                if (ImGui::Combo("Edge", &edge, edges, 2)) xy.triggerEdge = static_cast<TriggerEdge>(edge);
                ImGui::SliderFloat("Level", &xy.triggerLevel, -1.0f, 1.0f);
                ImGui::SliderFloat("Hysteresis", &xy.triggerHysteresis, 0.0f, 0.25f);
            }
            if (ImGui::CollapsingHeader("Input Conditioning")) {
                const char* coupling[] = {"DC", "AC"};
                int cx = std::clamp(static_cast<int>(xy.couplingX), 0, 1);
                int cy = std::clamp(static_cast<int>(xy.couplingY), 0, 1);
                if (ImGui::Combo("X Coupling", &cx, coupling, 2)) xy.couplingX = static_cast<CouplingMode>(cx);
                if (ImGui::Combo("Y Coupling", &cy, coupling, 2)) xy.couplingY = static_cast<CouplingMode>(cy);
                ImGui::SliderFloat("AC Cutoff (Hz)", &xy.acCutoffHz, 0.1f, 100.0f, "%.1f");
                ImGui::SliderFloat("Bandwidth (Hz)", &xy.bandwidthHz, 100.0f, 24000.0f, "%.0f");
            }
            if (ImGui::CollapsingHeader("Beam / Z")) {
                const char* zModes[] = {"Derived", "Explicit", "Derived x Explicit"};
                int zMode = std::clamp(static_cast<int>(xy.zMode), 0, 2);
                ImGui::SliderFloat("Beam Intensity", &xy.beamIntensity, 0.0f, 4.0f);
                ImGui::SliderFloat("Trace Thickness", &xy.traceWidth, 0.5f, 10.0f);
                ImGui::SliderFloat("Bloom", &xy.bloom, 0.0f, 5.0f);
                ImGui::SliderFloat("Dwell Effect", &xy.dwellEffect, 0.0f, 1.0f);
                ImGui::SliderFloat("Max Connected Jump", &xy.jumpBlanking, 0.02f, 1.0f);
                ImGui::SameLine(); HelpMarker("Lower values blank more long beam jumps. Start around 0.08 to remove retrace-like connectors.");
                if (ImGui::Combo("Z Mode", &zMode, zModes, 3)) xy.zMode = static_cast<ZIntensityMode>(zMode);
                ImGui::SliderFloat("Z Gain", &xy.zGain, 0.0f, 2.0f);
            }
            if (ImGui::CollapsingHeader("Transform")) {
                ImGui::SliderFloat("Rotation", &xy.rotationDegrees, -180.0f, 180.0f);
                ImGui::SliderFloat("X Scale", &xy.gainX, 0.1f, 5.0f);
                ImGui::SliderFloat("Y Scale", &xy.gainY, 0.1f, 5.0f);
                ImGui::SliderFloat("X Position", &xy.positionX, -1.0f, 1.0f);
                ImGui::SliderFloat("Y Position", &xy.positionY, -1.0f, 1.0f);
                ImGui::Checkbox("Invert X", &xy.invertX);
                ImGui::Checkbox("Invert Y", &xy.invertY);
            }
            if (ImGui::CollapsingHeader("Measurements")) {
                const auto& m = layer.xyMeasurements;
                ImGui::Text("X: %.2f Hz   Y: %.2f Hz   Ratio: %d:%d", m.frequencyX, m.frequencyY, m.ratioNumerator, m.ratioDenominator);
                ImGui::Text("X peak/RMS/DC: %.3f / %.3f / %.3f", m.peakX, m.rmsX, m.dcX);
                ImGui::Text("Y peak/RMS/DC: %.3f / %.3f / %.3f", m.peakY, m.rmsY, m.dcY);
                ImGui::Text("Phase: %s  %.1f deg   Confidence: %.0f%%", m.phaseValid ? "valid" : "--", m.phaseDegrees, m.confidence * 100.0f);
            }
        }
        if (layer.shape == VisualizerShape::Curve || layer.shape == VisualizerShape::Lines || layer.shape == VisualizerShape::Waveform) {
            ImGui::SliderFloat("Trace Thickness", &layer.traceWidth, 1.0f, 10.0f);
        }
        if (layer.shape == VisualizerShape::Curve) {
            ImGui::SliderFloat("Fill Opacity", &layer.fillOpacity, 0.0f, 1.0f);
        }
        if (layer.shape != VisualizerShape::OscilloscopeXY) {
            ImGui::Separator();
            ImGui::Text("Audio Channel");
            const char* channels[] = {"Mixed", "Left", "Right"};
            int chanIdx = std::clamp(static_cast<int>(layer.channel), 0, 2);
            if (ImGui::Combo("Channel", &chanIdx, channels, 3)) layer.channel = static_cast<AudioChannel>(chanIdx);
        }
        if (layer.shape == VisualizerShape::Bars || layer.shape == VisualizerShape::Lines || layer.shape == VisualizerShape::Dots) {
            const char* anchors[] = {"Bottom", "Top", "Left", "Right", "Center"};
            int anchorIdx = std::clamp(static_cast<int>(layer.barAnchor), 0, 4);
            if (ImGui::Combo("Bar Position", &anchorIdx, anchors, 5)) layer.barAnchor = static_cast<BarAnchor>(anchorIdx);
        }
        ImGui::End();
    }
}
