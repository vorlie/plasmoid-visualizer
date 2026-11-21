#include "UIManager.hpp"
#include "UIHelpers.hpp"
#include "VideoRenderManager.hpp"

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

void UIManager::refreshCaptureDevices(AppState& state, AudioEngine& audioEngine) {
    m_cachedCaptureDevices = audioEngine.getAvailableDevices(true);
    m_captureDevicesLoaded = true;
    m_selectedCaptureDeviceIdx = -1;
    if (state.config.useSpecificCaptureDevice) {
        for (int i = 0; i < static_cast<int>(m_cachedCaptureDevices.size()); ++i) {
            if (m_cachedCaptureDevices[i].name == state.config.selectedCaptureDeviceName) {
                m_selectedCaptureDeviceIdx = i;
                break;
            }
        }
    }
}

void UIManager::renderAudioSettings(AppState& state, AudioEngine& audioEngine, OscMusicEditor& oscMusicEditor, VideoRenderManager& videoRenderManager) {
    if (state.ui.showAudioSettings) {
        ImGui::Begin("Audio Settings", &state.ui.showAudioSettings);

        int modeInt = static_cast<int>(state.config.currentAudioMode);
        const char* modes[] = { "Load File", "Live Capture", "Test Tone", "Oscilloscope Music" };
        if (ImGui::Combo("Input Mode", &modeInt, modes, IM_ARRAYSIZE(modes))) {
            state.config.currentAudioMode = static_cast<AudioMode>(modeInt);
            if (state.config.currentAudioMode == AudioMode::Capture) {
                refreshCaptureDevices(state, audioEngine);
                if (state.config.useSpecificCaptureDevice && m_selectedCaptureDeviceIdx >= 0 &&
                    m_selectedCaptureDeviceIdx < static_cast<int>(m_cachedCaptureDevices.size())) {
                    audioEngine.startCapture(
                        &m_cachedCaptureDevices[m_selectedCaptureDeviceIdx].id,
                        m_cachedCaptureDevices[m_selectedCaptureDeviceIdx].type);
                } else {
                    audioEngine.startCapture(nullptr);
                }
                audioEngine.play();
            } else if (state.config.currentAudioMode == AudioMode::TestTone) {
                audioEngine.initTestTone();
            }
        }

        ImGui::Separator();

        if (state.config.currentAudioMode == AudioMode::Capture) {
            if (!m_captureDevicesLoaded) {
                refreshCaptureDevices(state, audioEngine);
            }

            std::vector<const char*> deviceNames;
            deviceNames.reserve(m_cachedCaptureDevices.size() + 1);
            deviceNames.push_back("Default Device");
            for (const auto& d : m_cachedCaptureDevices) deviceNames.push_back(d.name.c_str());

            int comboIdx = (state.config.useSpecificCaptureDevice && m_selectedCaptureDeviceIdx >= 0 &&
                            m_selectedCaptureDeviceIdx < static_cast<int>(m_cachedCaptureDevices.size()))
                ? m_selectedCaptureDeviceIdx + 1
                : 0;
            if (ImGui::Combo("Capture Device", &comboIdx, deviceNames.data(), static_cast<int>(deviceNames.size()))) {
                if (comboIdx == 0) {
                    state.config.useSpecificCaptureDevice = false;
                    m_selectedCaptureDeviceIdx = -1;
                    audioEngine.startCapture(nullptr);
                } else {
                    m_selectedCaptureDeviceIdx = comboIdx - 1;
                    state.config.useSpecificCaptureDevice = true;
                    state.config.selectedCaptureDeviceName = m_cachedCaptureDevices[m_selectedCaptureDeviceIdx].name;
                    audioEngine.startCapture(
                        &m_cachedCaptureDevices[m_selectedCaptureDeviceIdx].id,
                        m_cachedCaptureDevices[m_selectedCaptureDeviceIdx].type);
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Refresh##capture_devices")) {
                refreshCaptureDevices(state, audioEngine);
            }

            if (!state.config.useSpecificCaptureDevice) {
                ImGui::Text("Capturing from default device...");
            } else {
                ImGui::Text("Capturing from: %s", state.config.selectedCaptureDeviceName.c_str());
            }
        } else if (state.config.currentAudioMode == AudioMode::File) {
            UIHelpers::InputText("File Path", state.fx.filePath);
            if (ImGui::Button("Load & Play")) {
                if (audioEngine.loadFile(state.fx.filePath)) {
                    audioEngine.play();
                    state.ui.statusMessage = "Loaded: " + state.fx.filePath;
                    state.ui.statusColor = ImVec4(0, 1, 0, 1);
                } else {
                    state.ui.statusMessage = "Failed to load: " + state.fx.filePath;
                    state.ui.statusColor = ImVec4(1, 0, 0, 1);
                }
            }
            ImGui::TextColored(state.ui.statusColor, "%s", state.ui.statusMessage.c_str());
            if (ImGui::Button("Play")) audioEngine.play();
            ImGui::SameLine();
            if (ImGui::Button("Pause")) audioEngine.pause();
            ImGui::SameLine();
            if (ImGui::Button("Stop")) audioEngine.stop();
            ImGui::SameLine();
            if (ImGui::Checkbox("Loop Track", &state.config.loopTrack)) {
                audioEngine.setLoopTrack(state.config.loopTrack);
            }
            ImGui::SameLine();
            HelpMarker("Enabled: loop current track. Disabled: advance to the next track in Playlist at EOF.");

            float currentPos = audioEngine.getPosition();
            float duration = audioEngine.getDuration();
            if (duration > 0) {
                float progress = currentPos;
                char formatBuf[64];
                std::snprintf(formatBuf, sizeof(formatBuf), "%%.1f / %.1f s", duration);
                if (ImGui::SliderFloat("Progress", &progress, 0.0f, duration, formatBuf)) {
                    audioEngine.seekTo(progress);
                }
            }

            ImGui::Separator();
            if (ImGui::Button("Render to Video")) {
                state.ui.showRenderDialog = true;
            }

            if (state.ui.showRenderDialog) {
                ImGui::Begin("Render Video", &state.ui.showRenderDialog);
                auto& s = state.config.videoSettings;
                auto& status = state.fx.videoStatus;

                if (!status.isRendering) {
                    ImGui::InputInt("Width", &s.width);
                    ImGui::InputInt("Height", &s.height);
                    ImGui::InputInt("FPS", &s.fps);
                    UIHelpers::InputText("Output File", s.outputPath);

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
                    if (ImGui::Button("Clear Error")) status.errorMessage.clear();
                }

                ImGui::End();
            }
        } else if (state.config.currentAudioMode == AudioMode::TestTone) {
            auto& ui = state.ui;
            const char* types[] = { "Sine", "Square", "Sawtooth", "Triangle", "White Noise" };
            if (ImGui::Combo("Waveform", &ui.testToneType, types, IM_ARRAYSIZE(types))) {
                audioEngine.setTestToneType(static_cast<AudioEngine::TestToneType>(ui.testToneType));
            }
            if (ImGui::Checkbox("Stereo Mode", &ui.testToneStereo)) {
                audioEngine.setTestToneStereo(ui.testToneStereo);
            }

            if (ui.testToneStereo) {
                if (ImGui::SliderFloat("Frequency L (Hz)", &ui.testToneFreq, 20.0f, 2000.0f)) {
                    audioEngine.setTestToneFrequency(ui.testToneFreq);
                }
                if (ImGui::SliderFloat("Frequency R (Hz)", &ui.testToneFreqRight, 20.0f, 2000.0f)) {
                    audioEngine.setTestToneFrequencyRight(ui.testToneFreqRight);
                }
            } else {
                if (ImGui::SliderFloat("Frequency (Hz)", &ui.testToneFreq, 20.0f, 2000.0f)) {
                    audioEngine.setTestToneFrequency(ui.testToneFreq);
                }
            }
            if (ImGui::SliderFloat("Volume", &ui.testToneVol, 0.0f, 1.0f)) {
                audioEngine.setTestToneVolume(ui.testToneVol);
            }
            if (ImGui::Button("Start Tone")) { audioEngine.initTestTone(); audioEngine.play(); }
            ImGui::SameLine();
            if (ImGui::Button("Stop Tone")) audioEngine.stop();
        } else if (state.config.currentAudioMode == AudioMode::OscMusic) {
            auto& ui = state.ui;
            if (!ui.oscEditorSynced) {
                ui.oscXExpr = oscMusicEditor.getXExpression();
                ui.oscYExpr = oscMusicEditor.getYExpression();
                ui.oscZExpr = oscMusicEditor.getZExpression();
                ui.oscBaseFreq = oscMusicEditor.getBaseFrequency();
                ui.oscPreviewDirty = true;
                ui.oscEditorSynced = true;
            }

            ImGui::Text("Oscilloscope Music Editor");
            if (UIHelpers::InputText("X(t) Expression", ui.oscXExpr)) {
                oscMusicEditor.setExpressions(ui.oscXExpr, ui.oscYExpr, ui.oscZExpr);
                ui.oscPreviewDirty = true;
            }
            if (UIHelpers::InputText("Y(t) Expression", ui.oscYExpr)) {
                oscMusicEditor.setExpressions(ui.oscXExpr, ui.oscYExpr, ui.oscZExpr);
                ui.oscPreviewDirty = true;
            }
            if (UIHelpers::InputText("Z(t) Intensity", ui.oscZExpr)) {
                oscMusicEditor.setExpressions(ui.oscXExpr, ui.oscYExpr, ui.oscZExpr);
                ui.oscPreviewDirty = true;
            }
            if (ImGui::SliderFloat("Base Frequency (f)", &ui.oscBaseFreq, 20.0f, 2000.0f, "%.1f Hz")) {
                oscMusicEditor.setBaseFrequency(ui.oscBaseFreq);
                ui.oscPreviewDirty = true;
            }
            ImGui::SliderFloat("Duration (s)", &ui.oscDuration, 0.5f, 10.0f);

            if (!oscMusicEditor.isValid()) {
                ImGui::TextColored(ImVec4(1, 0, 0, 1), "Error: %s", oscMusicEditor.getErrorMessage().c_str());
            } else {
                ImGui::Text("Live Preview");
                ImVec2 canvasSize = ImVec2(ImGui::GetContentRegionAvail().x, 200);
                ImVec2 canvasPos = ImGui::GetCursorScreenPos();
                ImGui::Dummy(canvasSize);
                ImDrawList* drawList = ImGui::GetWindowDrawList();
                drawList->AddRectFilled(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), IM_COL32(20, 20, 20, 255));
                drawList->AddRect(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), IM_COL32(100, 100, 100, 255));

                constexpr int samples = 512;
                if (ui.oscPreviewDirty) {
                    const float timeRange = 1.0f / std::max(ui.oscBaseFreq, 1.0f) * 2.0f;
                    ui.oscPreviewPoints = oscMusicEditor.getPreviewPoints(samples, timeRange);
                    ui.oscPreviewDirty = false;
                }

                if (ui.oscPreviewPoints.size() >= static_cast<size_t>(samples) * 2) {
                    for (int i = 0; i < samples - 1; ++i) {
                        ImVec2 p1 = ImVec2(
                            canvasPos.x + (ui.oscPreviewPoints[i * 2] + 1.0f) * 0.5f * canvasSize.x,
                            canvasPos.y + (ui.oscPreviewPoints[i * 2 + 1] + 1.0f) * 0.5f * canvasSize.y
                        );
                        ImVec2 p2 = ImVec2(
                            canvasPos.x + (ui.oscPreviewPoints[(i + 1) * 2] + 1.0f) * 0.5f * canvasSize.x,
                            canvasPos.y + (ui.oscPreviewPoints[(i + 1) * 2 + 1] + 1.0f) * 0.5f * canvasSize.y
                        );
                        drawList->AddLine(p1, p2, IM_COL32(0, 200, 255, 200), 1.5f);
                    }
                    ImVec2 head = ImVec2(
                        canvasPos.x + (ui.oscPreviewPoints[0] + 1.0f) * 0.5f * canvasSize.x,
                        canvasPos.y + (ui.oscPreviewPoints[1] + 1.0f) * 0.5f * canvasSize.y
                    );
                    drawList->AddCircleFilled(head, 3.0f, IM_COL32(255, 255, 255, 255));
                }
            }

            if (ImGui::Button("Preview")) {
                if (oscMusicEditor.isValid()) {
                    OscMusicBuffer buffer = oscMusicEditor.generateXYBuffer(ui.oscDuration, 192000);
                    audioEngine.setOscMusicBuffer(std::move(buffer.stereo), std::move(buffer.z));
                    audioEngine.initOscMusic(192000);
                    audioEngine.setOscMusicMode(true);
                    audioEngine.play();
                    for (auto& l : state.config.layers) {
                        if (l.visible) l.shape = VisualizerShape::OscilloscopeXY;
                    }
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Stop Preview")) {
                audioEngine.stop();
                audioEngine.setOscMusicMode(false);
            }

            ImGui::Separator();
            ImGui::Text("Presets");
            const auto& presets = oscMusicEditor.getPresets();
            for (int i = 0; i < static_cast<int>(presets.size()); i++) {
                if (ImGui::Button(presets[i].name.c_str())) {
                    oscMusicEditor.loadPreset(i);
                    ui.oscXExpr = oscMusicEditor.getXExpression();
                    ui.oscYExpr = oscMusicEditor.getYExpression();
                    ui.oscZExpr = oscMusicEditor.getZExpression();
                    ui.oscPreviewDirty = true;
                }
                if ((i + 1) % 4 != 0) ImGui::SameLine();
            }
        }
        ImGui::End();
    }
}
