#include "UIManager.hpp"
#include "ConfigLogic.hpp"
#include "RenderManager.hpp"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <iostream>
#include <cstring>
#include <vector>
#include <filesystem>
#include "VideoRenderManager.hpp"

namespace fs = std::filesystem;

static void HelpMarker(const char* desc)
{
    ImGui::TextDisabled("(?)");
    if (ImGui::BeginItemTooltip())
    {
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

void UIManager::renderUI(
    AppState& state,
    AudioEngine& audioEngine,
    ParticleSystem& particleSystem,
    OscMusicEditor& oscMusicEditor,
    SystemStats& systemStats,
    VideoRenderManager& videoRenderManager
) {
    renderMainMenu(state, audioEngine);
    if (state.showAudioSettings) renderAudioSettings(state, audioEngine, oscMusicEditor, videoRenderManager);
    if (state.showZenKunSettings) renderZenKunSettings(state);
    renderLayerManager(state);
    renderLayerEditor(state);
    renderPlaylist(state, audioEngine);
    renderParticleSettings(state, particleSystem);
    renderDebugInfo(state, systemStats);
    renderGlobalSettings(state);
    renderStatusMessage(state);
}

void UIManager::renderMainMenu(AppState& state, AudioEngine& audioEngine) {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Load Audio File...")) { state.showAudioSettings = true; }
            if (ImGui::MenuItem("Save Config")) { ConfigLogic::saveSettings(state); }
            if (ImGui::MenuItem("Exit", "Alt+F4")) { /* handeled by glfwWindowShouldClose */ }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Audio Settings", nullptr, &state.showAudioSettings);
            ImGui::MenuItem("Layer Manager", nullptr, &state.showLayerManager);
            ImGui::MenuItem("Layer Editor", nullptr, &state.showLayerEditor);
            ImGui::MenuItem("Playlist", nullptr, &state.showPlaylist);
            ImGui::MenuItem("Particle Settings", nullptr, &state.showParticleSettings);
            ImGui::MenuItem("Debug Info", nullptr, &state.showDebugInfo);
            ImGui::MenuItem("Global Settings", nullptr, &state.showGlobalSettings);
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
}

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
                    for (auto& d : devices) {
                        if (d.name == state.selectedCaptureDeviceName) {
                            pID = &d.id;
                            break;
                        }
                    }
                    audioEngine.startCapture(pID);
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
                    audioEngine.startCapture(&devices[selectedDeviceIdx].id);
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
            
            if (!oscMusicEditor.isValid()) ImGui::TextColored(ImVec4(1, 0, 0, 1), "Error: %s", oscMusicEditor.getErrorMessage().c_str());

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
            if (ImGui::Button("Stop Preview")) audioEngine.stop();
            
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
            
            // Reordering buttons
            if (ImGui::Button("^") && i > 0) {
                std::swap(state.layers[i], state.layers[i-1]);
                if (state.selectedLayerIdx == i) state.selectedLayerIdx = i - 1;
                else if (state.selectedLayerIdx == i - 1) state.selectedLayerIdx = i;
            }
            ImGui::SameLine();
            if (ImGui::Button("v") && i < (int)state.layers.size() - 1) {
                std::swap(state.layers[i], state.layers[i+1]);
                if (state.selectedLayerIdx == i) state.selectedLayerIdx = i + 1;
                else if (state.selectedLayerIdx == i + 1) state.selectedLayerIdx = i;
            }
            ImGui::SameLine();

            if (ImGui::Selectable(state.layers[i].name.c_str(), state.selectedLayerIdx == i)) state.selectedLayerIdx = i;
            ImGui::SameLine(ImGui::GetWindowWidth() - 50);
            ImGui::Checkbox("##visible", &state.layers[i].visible);
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
            ImGui::SliderFloat("Rotation", &layer.rotation, 0.0f, 360.0f);
            ImGui::Checkbox("Flip Horizontal", &layer.flipX);
            ImGui::Checkbox("Flip Vertical", &layer.flipY);
            ImGui::SliderFloat("Bloom Intensity", &layer.bloom, 0.0f, 5.0f);
            ImGui::Separator();
            ImGui::SliderFloat("Trace Thickness", &layer.traceWidth, 1.0f, 10.0f);
            ImGui::SliderFloat("Beam Head Size", &layer.beamHeadSize, 0.0f, 30.0f);
            ImGui::SliderFloat("Velocity Brightness", &layer.velocityModulation, 0.0f, 2.0f);
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

void UIManager::renderPlaylist(AppState& state, AudioEngine& audioEngine) {
    if (state.showPlaylist) {
        ImGui::Begin("Playlist", &state.showPlaylist);
        if (ImGui::InputText("Music Folder", state.musicFolderPath, sizeof(state.musicFolderPath))) ConfigLogic::scanMusicFolder(state, state.musicFolderPath);
        ImGui::SameLine();
        if (ImGui::Button("Refresh")) ConfigLogic::scanMusicFolder(state, state.musicFolderPath);

        ImGui::Separator();
        if (ImGui::BeginChild("FileList")) {
            for (const auto& file : state.musicFiles) {
                bool isSelected = (std::string(state.filePath).find(file) != std::string::npos);
                if (ImGui::Selectable(file.c_str(), isSelected)) {
                    fs::path fullPath = fs::path(state.musicFolderPath) / file;
                    strncpy(state.filePath, fullPath.string().c_str(), sizeof(state.filePath));
                    if (audioEngine.loadFile(state.filePath)) {
                        state.currentAudioMode = AudioMode::File;
                        audioEngine.play();
                        state.statusMessage = "Playing: " + file;
                        state.statusColor = ImVec4(0, 1, 0, 1);
                    } else {
                        state.statusMessage = "Failed to load: " + file;
                        state.statusColor = ImVec4(1, 0, 0, 1);
                    }
                }
            }
        }
        ImGui::EndChild(); ImGui::End();
    }
}

void UIManager::renderParticleSettings(AppState& state, ParticleSystem& particleSystem) {
    if (state.showParticleSettings) {
        ImGui::Begin("Particle Settings", &state.showParticleSettings);
        ImGui::Checkbox("Enable Particles", &state.particlesEnabled);
        if (state.particlesEnabled) {
            ImGui::Separator(); ImGui::Text("Beat Detection");
            ImGui::SliderFloat("Sensitivity", &state.beatSensitivity, 1.0f, 2.0f);
            ImGui::Separator(); ImGui::Text("Particle Physics");
            ImGui::SliderInt("Max Count", &state.particleCount, 100, 2000);
            ImGui::SliderFloat("Speed/Energy", &state.particleSpeed, 0.1f, 5.0f);
            ImGui::SliderFloat("Size", &state.particleSize, 1.0f, 20.0f);
            ImGui::ColorEdit4("Color", state.particleColor);
        }
        ImGui::End();
    }
}

void UIManager::renderDebugInfo(AppState& state, SystemStats& systemStats) {
    if (state.showDebugInfo) {
        systemStats.update(ImGui::GetIO().DeltaTime);
        ImGui::Begin("Debug Info", &state.showDebugInfo);
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
        ImGui::Text("Active Layers: %zu", state.layers.size());
        ImGui::Separator(); ImGui::Text("System Resources:");
        ImGui::Text("CPU (Global):  %.1f%%", systemStats.getGlobalCpuUsage());
        ImGui::Text("CPU (Process): %.1f%%", systemStats.getProcessCpuUsage());
        ImGui::Text("RAM Usage:     %.1f MB", systemStats.getRamUsageMB());
        ImGui::Text("GPU Info:      %s", systemStats.getGpuInfo().c_str());
        if (strlen(state.filePath) > 0) { ImGui::Separator(); ImGui::Text("Current File: %s", state.filePath); }
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
        
        ImGui::End();
    }
}

void UIManager::renderZenKunSettings(AppState& state) {
    if (state.showZenKunSettings) {
        ImGui::Begin("Background Settings", &state.showZenKunSettings);
        ImGui::Checkbox("Enable Background ", &state.zenKunModeEnabled);
        if (state.zenKunModeEnabled) {
            ImGui::InputText("Background Path", state.backgroundImagePath, sizeof(state.backgroundImagePath));
            ImGui::SameLine(); HelpMarker("Path to image file (JPG/PNG). Stretched to fill screen.");
            
            ImGui::Separator();
            ImGui::Text("Song Information");
            ImGui::Checkbox("Draw Info Overlay", &state.showSongInfo);
            ImGui::InputText("Title", state.songTitle, sizeof(state.songTitle));
            ImGui::InputText("Artist", state.artistName, sizeof(state.artistName));

            ImGui::Separator();
            ImGui::Text("Beat Effects");
            ImGui::SliderFloat("Beat Sensitivity", &state.beatSensitivity, 1.0f, 2.0f);
            ImGui::SliderFloat("Beat Pulse (Twitch)", &state.bgPulseIntensity, 0.0f, 0.5f);
            ImGui::SliderFloat("Shake X/Y", &state.shakeIntensity, 0.0f, 0.2f);
            ImGui::SliderFloat("Shake Tilt", &state.shakeTiltIntensity, 0.0f, 0.2f);
            ImGui::SliderFloat("Shake Zoom", &state.shakeZoomIntensity, 0.0f, 0.2f);
        }
        ImGui::End();
    }
}

void UIManager::renderStatusMessage(AppState& state) {
    // Optional: Render as an overlay or something
}
