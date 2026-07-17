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

void UIManager::renderAudioSettings(AppState& state, AudioEngine& audioEngine, OscMusicEditor& oscMusicEditor, VideoRenderManager& videoRenderManager) {
    if (state.showAudioSettings) {
        ImGui::Begin("Audio Settings", &state.showAudioSettings);
        
        static int modeInt = (int)state.currentAudioMode;
        const char* modes[] = { "Load File", "Live Capture", "Test Tone", "Oscilloscope Music" };
        if (ImGui::Combo("Input Mode", &modeInt, modes, IM_ARRAYSIZE(modes))) {
            state.currentAudioMode = (AudioMode)modeInt;
            if (state.currentAudioMode == AudioMode::Capture) {
                if (state.useSpecificCaptureDevice) {
                    auto devices = audioEngine.getAvailableDevices(true);
                    ma_device_id* pID = nullptr;
                    ma_device_type type = ma_device_type_capture;
                    for (auto& d : devices) {
                        if (d.name == state.selectedCaptureDeviceName) {
                            pID = &d.id;
                            type = d.type;
                            break;
                        }
                    }
                    audioEngine.startCapture(pID, type);
                } else {
                    audioEngine.startCapture(nullptr);
                }
                audioEngine.play();
            } else if (state.currentAudioMode == AudioMode::TestTone) {
                audioEngine.initTestTone();
            }
        }
        
        ImGui::Separator();
        
        if (state.currentAudioMode == AudioMode::Capture) {
            auto devices = audioEngine.getAvailableDevices(true);
            static int selectedDeviceIdx = -1;
            
            // Try to find current selected device in the list if not already set
            if (selectedDeviceIdx == -1 && state.useSpecificCaptureDevice) {
                for (int i = 0; i < (int)devices.size(); ++i) {
                    if (devices[i].name == state.selectedCaptureDeviceName) {
                        selectedDeviceIdx = i;
                        break;
                    }
                }
            }

            std::vector<const char*> deviceNames;
            deviceNames.push_back("Default Device");
            for (const auto& d : devices) deviceNames.push_back(d.name.c_str());

            int comboIdx = state.useSpecificCaptureDevice ? selectedDeviceIdx + 1 : 0;
            if (ImGui::Combo("Capture Device", &comboIdx, deviceNames.data(), (int)deviceNames.size())) {
                if (comboIdx == 0) {
                    state.useSpecificCaptureDevice = false;
                    selectedDeviceIdx = -1;
                    audioEngine.startCapture(nullptr);
                } else {
                    selectedDeviceIdx = comboIdx - 1;
                    state.useSpecificCaptureDevice = true;
                    strncpy(state.selectedCaptureDeviceName, devices[selectedDeviceIdx].name.c_str(), sizeof(state.selectedCaptureDeviceName));
                    audioEngine.startCapture(&devices[selectedDeviceIdx].id, devices[selectedDeviceIdx].type);
                }
            }

            if (!state.useSpecificCaptureDevice) {
                ImGui::Text("Capturing from default device...");
            } else {
                ImGui::Text("Capturing from: %s", state.selectedCaptureDeviceName);
            }
        } else if (state.currentAudioMode == AudioMode::File) {
            ImGui::InputText("File Path", state.filePath, sizeof(state.filePath));
            if (ImGui::Button("Load & Play")) {
                if (audioEngine.loadFile(state.filePath)) {
                    audioEngine.play();
                    state.statusMessage = "Loaded: " + std::string(state.filePath);
                    state.statusColor = ImVec4(0, 1, 0, 1);
                } else {
                    state.statusMessage = "Failed to load: " + std::string(state.filePath);
                    state.statusColor = ImVec4(1, 0, 0, 1);
                }
            }
            ImGui::TextColored(state.statusColor, "%s", state.statusMessage.c_str());
            if (ImGui::Button("Play")) audioEngine.play();
            ImGui::SameLine();
            if (ImGui::Button("Pause")) audioEngine.pause();
            ImGui::SameLine();
            if (ImGui::Button("Stop")) audioEngine.stop();

            float currentPos = audioEngine.getPosition();
            float duration = audioEngine.getDuration();
            if (duration > 0) {
                float progress = currentPos;
                char formatBuf[64];
                snprintf(formatBuf, sizeof(formatBuf), "%%.1f / %.1f s", duration);
                if (ImGui::SliderFloat("Progress", &progress, 0.0f, duration, formatBuf)) {
                    audioEngine.seekTo(progress);
                }
            }

            ImGui::Separator();
            static bool showRenderDialog = false;
            if (ImGui::Button("Render to Video")) {
                showRenderDialog = true;
            }

            if (showRenderDialog) {
                ImGui::Begin("Render Video", &showRenderDialog);
                auto& s = state.videoSettings;
                auto& status = state.videoStatus;

                if (!status.isRendering) {
                    ImGui::InputInt("Width", &s.width);
                    ImGui::InputInt("Height", &s.height);
                    ImGui::InputInt("FPS", &s.fps);
                    ImGui::InputText("Output File", s.outputPath, sizeof(s.outputPath));

                    const char* codecs[] = { "libx264 (CPU)", "h264_nvenc (NVIDIA)", "h264_vaapi (Intel/AMD)", "h264_amf (AMD)" };
                    ImGui::Combo("Codec", &s.codecIdx, codecs, IM_ARRAYSIZE(codecs));
                    
                    ImGui::SliderInt("Quality (CRF/QP)", &s.crf, 0, 51);
                    ImGui::SameLine(); HelpMarker("Lower is better. 16-23 is good for x264.");

                    if (ImGui::Button("Start Render")) {
                        videoRenderManager.startRender(state, audioEngine);
                    }
                } else {
                    ImGui::Text("Status: Rendering...");
                    ImGui::Text("Frame: %d / %d", status.currentFrame, status.totalFrames);
                    ImGui::ProgressBar(status.progress, ImVec2(-1.0f, 0.0f));
                    
                    if (ImGui::Button("Cancel Render")) {
                        videoRenderManager.cancelRender(state);
                    }
                }
                
                if (!status.errorMessage.empty()) {
                    ImGui::TextColored(ImVec4(1, 0, 0, 1), "Error: %s", status.errorMessage.c_str());
                    if (ImGui::Button("Clear Error")) status.errorMessage = "";
                }

                ImGui::End();
            }
        } else if (state.currentAudioMode == AudioMode::TestTone) {
            static float testFreq = 440.0f;
            static float testFreqRight = 440.0f;
            static bool testStereo = false;
            static float testVol = 0.5f;
            static int testType = 0;
            
            const char* types[] = { "Sine", "Square", "Sawtooth", "Triangle", "White Noise" };
            if (ImGui::Combo("Waveform", &testType, types, IM_ARRAYSIZE(types))) {
                audioEngine.setTestToneType((AudioEngine::TestToneType)testType);
            }
            if (ImGui::Checkbox("Stereo Mode", &testStereo)) audioEngine.setTestToneStereo(testStereo);

            if (testStereo) {
                if (ImGui::SliderFloat("Frequency L (Hz)", &testFreq, 20.0f, 2000.0f)) audioEngine.setTestToneFrequency(testFreq);
                if (ImGui::SliderFloat("Frequency R (Hz)", &testFreqRight, 20.0f, 2000.0f)) audioEngine.setTestToneFrequencyRight(testFreqRight);
            } else {
                if (ImGui::SliderFloat("Frequency (Hz)", &testFreq, 20.0f, 2000.0f)) audioEngine.setTestToneFrequency(testFreq);
            }
            if (ImGui::SliderFloat("Volume", &testVol, 0.0f, 1.0f)) audioEngine.setTestToneVolume(testVol);
            if (ImGui::Button("Start Tone")) { audioEngine.initTestTone(); audioEngine.play(); }
            ImGui::SameLine();
            if (ImGui::Button("Stop Tone")) audioEngine.stop();
        } else if (state.currentAudioMode == AudioMode::OscMusic) {
            ImGui::Text("Oscilloscope Music Editor");
            static char xExpr[256] = "sin(f * t * 2 * 3.14159)";
            static char yExpr[256] = "cos(f * t * 2 * 3.14159)";
            static float duration = 2.0f;
            static float baseFreq = 440.0f;
            
            if (ImGui::InputText("X(t) Expression", xExpr, sizeof(xExpr))) oscMusicEditor.setExpressions(xExpr, yExpr);
            if (ImGui::InputText("Y(t) Expression", yExpr, sizeof(yExpr))) oscMusicEditor.setExpressions(xExpr, yExpr);
            if (ImGui::SliderFloat("Base Frequency (f)", &baseFreq, 20.0f, 2000.0f, "%.1f Hz")) oscMusicEditor.setBaseFrequency(baseFreq);
            ImGui::SliderFloat("Duration (s)", &duration, 0.5f, 10.0f);
            
            if (!oscMusicEditor.isValid()) {
                ImGui::TextColored(ImVec4(1, 0, 0, 1), "Error: %s", oscMusicEditor.getErrorMessage().c_str());
            } else {
                // Real-time Preview Canvas
                ImGui::Text("Live Preview");
                ImVec2 canvasSize = ImVec2(ImGui::GetContentRegionAvail().x, 200);
                ImVec2 canvasPos = ImGui::GetCursorScreenPos();
                ImDrawList* drawList = ImGui::GetWindowDrawList();
                drawList->AddRectFilled(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), IM_COL32(20, 20, 20, 255));
                drawList->AddRect(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), IM_COL32(100, 100, 100, 255));
                
                // Draw a few cycles of the expression
                int samples = 512;
                float timeRange = 1.0f / baseFreq * 2.0f; // Show 2 cycles
                auto previewPoints = oscMusicEditor.getPreviewPoints(samples, timeRange);
                
                if (!previewPoints.empty()) {
                    for (int i = 0; i < samples - 1; ++i) {
                        ImVec2 p1 = ImVec2(
                            canvasPos.x + (previewPoints[i*2] + 1.0f) * 0.5f * canvasSize.x,
                            canvasPos.y + (previewPoints[i*2+1] + 1.0f) * 0.5f * canvasSize.y
                        );
                        ImVec2 p2 = ImVec2(
                            canvasPos.x + (previewPoints[(i+1)*2] + 1.0f) * 0.5f * canvasSize.x,
                            canvasPos.y + (previewPoints[(i+1)*2+1] + 1.0f) * 0.5f * canvasSize.y
                        );
                        drawList->AddLine(p1, p2, IM_COL32(0, 200, 255, 200), 1.5f);
                    }
                    // Current "head" of the trace
                    ImVec2 head = ImVec2(
                        canvasPos.x + (previewPoints[0] + 1.0f) * 0.5f * canvasSize.x,
                        canvasPos.y + (previewPoints[1] + 1.0f) * 0.5f * canvasSize.y
                    );
                    drawList->AddCircleFilled(head, 3.0f, IM_COL32(255, 255, 255, 255));
                }
            }

            if (ImGui::Button("Preview")) {
                if (oscMusicEditor.isValid()) {
                    std::vector<float> buffer = oscMusicEditor.generateStereoBuffer(duration, 192000);
                    audioEngine.setOscMusicBuffer(buffer);
                    audioEngine.initOscMusic(192000);
                    audioEngine.setOscMusicMode(true);
                    audioEngine.play();
                    for (auto& l : state.layers) if (l.visible) l.shape = VisualizerShape::OscilloscopeXY;
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Stop Preview")) {
                audioEngine.stop();
                audioEngine.setOscMusicMode(false);
            }
            
            ImGui::Separator(); ImGui::Text("Presets");
            auto presets = oscMusicEditor.getPresets();
            for (int i = 0; i < (int)presets.size(); i++) {
                if (ImGui::Button(presets[i].name.c_str())) {
                    oscMusicEditor.loadPreset(i);
                    strncpy(xExpr, oscMusicEditor.getXExpression().c_str(), sizeof(xExpr));
                    strncpy(yExpr, oscMusicEditor.getYExpression().c_str(), sizeof(yExpr));
                }
                if ((i + 1) % 4 != 0) ImGui::SameLine();
            }
        }
        ImGui::End();
    }
}

