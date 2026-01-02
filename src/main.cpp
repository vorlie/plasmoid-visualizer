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
#include "ConfigManager.hpp"
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

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
    bool mirrored = false;
    VisualizerShape shape = VisualizerShape::Bars;
    float cornerRadius = 0.0f;
    std::vector<float> prevMagnitudes;
    bool visible = true;
    float timeScale = 1.0f; // For Waveform/Oscilloscope
    float rotation = 0.0f; // For Oscilloscope XY rotation (degrees for UI)
    bool flipX = false; // Horizontal flip for XY
    bool flipY = false; // Vertical flip for XY
    float bloom = 1.0f; // Bloom intensity for XY
    bool showGrid = true; // For Oscilloscope XY
    float traceWidth = 2.0f; // Thickness for all line-based shapes
};

int main() {
    if (!glfwInit()) return -1;

    GLFWwindow* window = glfwCreateWindow(800, 800, "Plasmoid Visualizer Standalone", NULL, NULL);
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
    ParticleSystem particleSystem;
    OscMusicEditor oscMusicEditor;

    std::vector<VisualizerLayer> layers;
    // Add default layer
    layers.push_back({"Default Layer", LayerConfig(), {0.0f, 0.8f, 1.0f, 1.0f}, 0.2f, false, VisualizerShape::Bars, 0.0f, {}, true});

    // Audio Modes
    enum AudioMode { ModeFile, ModeLive, ModeTest, ModeOscMusic };
    int currentAudioMode = ModeFile;
    
    char filePath[256] = "";
    std::string statusMessage = "Ready";
    ImVec4 statusColor = ImVec4(1, 1, 1, 1);

    int selectedLayerIdx = 0;

    // Window visibility
    bool showAudioSettings = true;
    bool showLayerManager = true;
    bool showLayerEditor = true;
    bool showDebugInfo = false;
    bool showParticleSettings = true;

    // Particle Settings
    bool particlesEnabled = false;
    float beatSensitivity = 1.3f;
    int particleCount = 500;
    float particleSpeed = 1.0f;
    float particleSize = 5.0f;
    float particleColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

    char musicFolderPath[256] = "";
    std::vector<std::string> playlistFiles;
    bool showPlaylist = true;
    float phosphorDecay = 0.1f;

    auto scanMusicFolder = [&](const std::string& path) {
        playlistFiles.clear();
        if (path.empty() || !fs::exists(path) || !fs::is_directory(path)) return;
        try {
            for (const auto& entry : fs::directory_iterator(path)) {
                if (entry.is_regular_file()) {
                    std::string ext = entry.path().extension().string();
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                    if (ext == ".mp3" || ext == ".wav" || ext == ".flac" || ext == ".ogg") {
                        playlistFiles.push_back(entry.path().filename().string());
                    }
                }
            }
            std::sort(playlistFiles.begin(), playlistFiles.end());
        } catch (...) {}
    };

    auto saveSettings = [&]() {
        AppConfig config;
        config.musicFolder = musicFolderPath;
        config.particlesEnabled = particlesEnabled;
        config.beatSensitivity = beatSensitivity;
        config.particleCount = particleCount;
        config.particleSpeed = particleSpeed;
        config.particleSize = particleSize;
        std::copy(std::begin(particleColor), std::end(particleColor), std::begin(config.particleColor));
        config.phosphorDecay = phosphorDecay;
        config.audioMode = currentAudioMode;

        for (auto& l : layers) {
            ConfigLayer cl;
            cl.name = l.name;
            cl.gain = l.config.gain;
            cl.falloff = l.config.falloff;
            cl.minFreq = l.config.minFreq;
            cl.maxFreq = l.config.maxFreq;
            cl.numBars = l.config.numBars;
            std::copy(std::begin(l.color), std::end(l.color), std::begin(cl.color));
            cl.barHeight = l.barHeight;
            cl.mirrored = l.mirrored;
            cl.shape = (int)l.shape;
            cl.cornerRadius = l.cornerRadius;
            cl.visible = l.visible;
            cl.timeScale = l.timeScale;
            cl.rotation = l.rotation;
            cl.flipX = l.flipX;
            cl.flipY = l.flipY;
            cl.bloom = l.bloom;
            cl.showGrid = l.showGrid;
            cl.traceWidth = l.traceWidth;
            config.layers.push_back(cl);
        }
        ConfigManager::save("", config); // empty means default path
        statusMessage = "Config saved to " + ConfigManager::getConfigPath();
        statusColor = ImVec4(0, 1, 1, 1);
    };

    auto loadSettings = [&]() {
        AppConfig config;
        if (ConfigManager::load("", config)) { // empty means default path
            strncpy(musicFolderPath, config.musicFolder.c_str(), sizeof(musicFolderPath));
            particlesEnabled = config.particlesEnabled;
            beatSensitivity = config.beatSensitivity;
            particleCount = config.particleCount;
            particleSpeed = config.particleSpeed;
            particleSize = config.particleSize;
            std::copy(std::begin(config.particleColor), std::end(config.particleColor), std::begin(particleColor));
            phosphorDecay = config.phosphorDecay;
            currentAudioMode = (AudioMode)config.audioMode;

            if (!config.layers.empty()) {
                layers.clear();
                for (auto& cl : config.layers) {
                    VisualizerLayer l;
                    l.name = cl.name;
                    l.config.gain = cl.gain;
                    l.config.falloff = cl.falloff;
                    l.config.minFreq = cl.minFreq;
                    l.config.maxFreq = cl.maxFreq;
                    l.config.numBars = cl.numBars;
                    std::copy(std::begin(cl.color), std::end(cl.color), std::begin(l.color));
                    l.barHeight = cl.barHeight;
                    l.mirrored = cl.mirrored;
                    l.shape = (VisualizerShape)cl.shape;
                    l.cornerRadius = cl.cornerRadius;
                    l.visible = cl.visible;
                    l.timeScale = cl.timeScale;
                    l.rotation = cl.rotation;
                    l.flipX = cl.flipX;
                    l.flipY = cl.flipY;
                    l.bloom = cl.bloom;
                    l.showGrid = cl.showGrid;
                    l.traceWidth = cl.traceWidth;
                    layers.push_back(l);
                }
            }
            scanMusicFolder(musicFolderPath);
            statusMessage = "Config loaded from " + ConfigManager::getConfigPath();
            statusColor = ImVec4(0, 1, 0, 1);
        }
    };

    // Initial load
    loadSettings();

    float smoothTriggerOffset = 0.0f;
    float triggerLevel = 0.05f;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        float deltaTime = 1.0f / ImGui::GetIO().Framerate;

        std::vector<float> audioBuffer;
        bool isBeat = false;
        
        try {
            audioEngine.getBuffer(audioBuffer, 8192);
            analysisEngine.computeFFT(audioBuffer);
            
            if (particlesEnabled) {
                analysisEngine.setBeatSensitivity(beatSensitivity);
                isBeat = analysisEngine.detectBeat(deltaTime);
                particleSystem.update(deltaTime, isBeat);
            }

            // 1. PERSISTENCE PASS (Render to FBO)
            int display_w, display_h;
            glfwGetFramebufferSize(window, &display_w, &display_h);
            
            visualizer.setupPersistence(display_w, display_h);
            visualizer.beginPersistence();

            bool usePersistence = false;
            for (auto& l : layers) {
                if (l.visible && l.shape == VisualizerShape::OscilloscopeXY) {
                    usePersistence = true; break;
                }
            }

            if (usePersistence) {
                visualizer.drawFullscreenDimmer(phosphorDecay); 
            } else {
                glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
                glClear(GL_COLOR_BUFFER_BIT);
            }

            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            for (auto& layer : layers) {
                if (!layer.visible) continue;
                
                std::vector<float> renderData;
                if (layer.shape == VisualizerShape::Waveform) {
                    size_t triggerOffset = 0;
                    for (size_t i = 0; i < 512 && i + 1 < audioBuffer.size(); ++i) {
                        if (audioBuffer[i] < 0.0f && audioBuffer[i+1] >= 0.0f) {
                            triggerOffset = i; break;
                        }
                    }
                    size_t copyLen = std::min((size_t)2048, audioBuffer.size() - triggerOffset);
                    renderData.assign(audioBuffer.begin() + triggerOffset, audioBuffer.begin() + triggerOffset + copyLen);
                } else if (layer.shape == VisualizerShape::OscilloscopeXY) {
                    size_t requestFrames = (size_t)(2048 * layer.timeScale);
                    if (audioEngine.getChannels() == 2) {
                        std::vector<float> stereo;
                        audioEngine.getStereoBuffer(stereo, 8192);
                        size_t triggerOffset = 0;
                        for (size_t i = 0; i < 512 && (i + 1) * 2 < stereo.size(); ++i) {
                            if (stereo[i * 2] < 0.0f && stereo[(i + 1) * 2] >= 0.05f) {
                                triggerOffset = i * 2; break;
                            }
                        }
                        size_t copySize = std::min(stereo.size() - triggerOffset, requestFrames * 2);
                        renderData.assign(stereo.begin() + triggerOffset, stereo.begin() + triggerOffset + copySize);
                    }
                } else {
                    renderData = analysisEngine.computeLayerMagnitudes(layer.config, layer.prevMagnitudes);
                }

                visualizer.setColor(layer.color[0], layer.color[1], layer.color[2], layer.color[3]);
                visualizer.setHeightScale(layer.barHeight);
                visualizer.setMirrored(layer.mirrored);
                visualizer.setShape(layer.shape);
                visualizer.setCornerRadius(layer.cornerRadius);
                visualizer.setRotation(layer.rotation * 3.14159f / 180.0f);
                visualizer.setFlip(layer.flipX, layer.flipY);
                visualizer.setBloomIntensity(layer.bloom);
                visualizer.setGridEnabled(layer.showGrid);
                visualizer.setTraceWidth(layer.traceWidth);
                visualizer.render(renderData);
            }

            if (particlesEnabled) {
                particleSystem.render();
            }
            
            visualizer.endPersistence();

            // 2. MAIN PASS (Clear UI and Draw visuals)
            glViewport(0, 0, display_w, display_h);
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            visualizer.drawPersistenceBuffer();
        } catch (const std::exception& e) {
            std::cerr << "EXCEPTION in Main Loop: " << e.what() << std::endl;
        } catch (...) {
            std::cerr << "UNKNOWN EXCEPTION in Main Loop" << std::endl;
        }


        // UI
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Main Menu Bar
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Save Config")) saveSettings();
                if (ImGui::MenuItem("Load Config")) loadSettings();
                ImGui::Separator();
                if (ImGui::MenuItem("Exit")) glfwSetWindowShouldClose(window, true);
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Windows")) {
                ImGui::MenuItem("Audio Settings", NULL, &showAudioSettings);
                ImGui::MenuItem("Layer Manager", NULL, &showLayerManager);
                ImGui::MenuItem("Layer Editor", NULL, &showLayerEditor);
                ImGui::MenuItem("Particle Settings", NULL, &showParticleSettings);
                ImGui::MenuItem("Playlist", NULL, &showPlaylist);
                ImGui::MenuItem("Debug Info", NULL, &showDebugInfo);
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

        // Audio Settings Window
        if (showAudioSettings) {
            ImGui::Begin("Audio Settings", &showAudioSettings);
            
            const char* modes[] = { "File Playback", "Live Playback", "Test Tone", "Oscilloscope Music" };
            if (ImGui::Combo("Audio Mode", &currentAudioMode, modes, IM_ARRAYSIZE(modes))) {
                // Stop everything on mode switch
                audioEngine.stop();
                audioEngine.stopCapture();
                audioEngine.setTestTone(false);
                audioEngine.setOscMusicMode(false);

                if (currentAudioMode == ModeLive) {
                    audioEngine.startCapture();
                } else if (currentAudioMode == ModeTest) {
                    audioEngine.setTestTone(true);
                    audioEngine.initTestTone();
                    audioEngine.play();
                } else if (currentAudioMode == ModeOscMusic) {
                    audioEngine.setOscMusicMode(true);
                    audioEngine.initOscMusic(192000);
                    audioEngine.play();
                }
            }
            ImGui::Separator();

            if (currentAudioMode == ModeLive) {
                ImGui::Text("Live Mode Active");
                
                static std::vector<AudioEngine::DeviceInfo> captureDevices;
                // Fetch devices only once or on demand
                if (captureDevices.empty() || ImGui::Button("Refresh Devices")) {
                    captureDevices = audioEngine.getAvailableDevices(true);
                }
                ImGui::SameLine(); // align refresh button if it wasn't clicked
                
                static int selectedCaptureDevice = 0;
                
                std::vector<const char*> deviceNames;
                for (const auto& d : captureDevices) deviceNames.push_back(d.name.c_str());
                
                // Safety check for index
                if (selectedCaptureDevice >= (int)captureDevices.size()) selectedCaptureDevice = 0;

                if (!captureDevices.empty()) {
                    if (ImGui::Combo("Input Device", &selectedCaptureDevice, deviceNames.data(), deviceNames.size())) {
                        audioEngine.startCapture(&captureDevices[selectedCaptureDevice].id);
                    }
                } else {
                    ImGui::TextDisabled("No capture devices found");
                }
                
            } else {
                // Shared Output Device Selection for File and Test modes
                static std::vector<AudioEngine::DeviceInfo> outputDevices;
                // Refresh device list only once or on demand
                if (outputDevices.empty() || ImGui::Button("Refresh Output Devices")) {
                    outputDevices = audioEngine.getAvailableDevices(false);
                }
                
                static int selectedOutputDevice = 0;
                // Safety check for index
                if (selectedOutputDevice >= (int)outputDevices.size()) selectedOutputDevice = 0;

                if (ImGui::BeginCombo("Output Device", outputDevices.empty() ? "No devices" : outputDevices[selectedOutputDevice].name.c_str())) {
                    for (int i = 0; i < (int)outputDevices.size(); i++) {
                        bool isSelected = (selectedOutputDevice == i);
                        if (ImGui::Selectable(outputDevices[i].name.c_str(), isSelected)) {
                            selectedOutputDevice = i;
                            audioEngine.setDevice(&outputDevices[i].id);
                            
                            // Restart audio subsystem if we change device
                            if (currentAudioMode == ModeTest) {
                                audioEngine.initTestTone();
                                audioEngine.play();
                            } else if (currentAudioMode == ModeFile && strlen(filePath) > 0) {
                                audioEngine.loadFile(filePath); // Reload to switch device
                            }
                        }
                    }
                    ImGui::EndCombo();
                }

                ImGui::Separator();

                if (currentAudioMode == ModeFile) {
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

                    ImGui::Separator();
                    static bool showRenderDialog = false;
                    if (ImGui::Button("Render to Video")) {
                        showRenderDialog = true;
                    }

                    if (showRenderDialog) {
                        ImGui::Begin("Render Video", &showRenderDialog);
                        static int vidWidth = 1920;
                        static int vidHeight = 1080;
                        static int vidFps = 60;
                        static char outPath[256] = "output.mp4";
                        static float renderProgress = 0.0f;
                        static bool isRendering = false;

                        ImGui::InputInt("Width", &vidWidth);
                        ImGui::InputInt("Height", &vidHeight);
                        ImGui::InputInt("FPS", &vidFps);
                        ImGui::InputText("Output File", outPath, sizeof(outPath));

                        if (!isRendering && ImGui::Button("Start Render")) {
                            isRendering = true;
                            // RENDER LOGIC
                            // 1. Pause Playback
                            audioEngine.pause();
                            
                            // 2. Open FFMPEG Pipe
                            // Added -vf vflip because OpenGL reads bottom-to-top
                            std::string cmd = "ffmpeg -y -f rawvideo -vcodec rawvideo -s " + 
                                              std::to_string(vidWidth) + "x" + std::to_string(vidHeight) + 
                                              " -pix_fmt rgb24 -r " + std::to_string(vidFps) + 
                                              " -i - -i \"" + std::string(filePath) + 
                                              "\" -vf \"vflip,colorspace=all=bt709:iall=bt709\" -c:v libx264 -crf 16 -preset veryslow -pix_fmt yuv444p -c:a copy -shortest \"" + std::string(outPath) + "\"";
                            
                            FILE* pipe = popen(cmd.c_str(), "w");
                            if (pipe) {
                                float duration = audioEngine.getDuration();
                                ma_uint32 sampleRate = audioEngine.getSampleRate();
                                int totalFrames = (int)(duration * vidFps);
                                double dt = 1.0 / (double)vidFps;
                                // We'll just ask audioEngine for chunks.
                                
                                std::vector<float> frameAudio; // Stereo interleaved from file
                                std::vector<float> monoAudio; // Downmixed
                                std::vector<uint8_t> pixels(vidWidth * vidHeight * 3);

                                for (int f = 0; f < totalFrames; f++) {
                                    renderProgress = (float)f / totalFrames;
                                    
                                    if (f % 60 == 0) std::cout << "Rendering: " << (int)(renderProgress * 100) << "%" << std::endl;

                                    // 1. Read Audio
                                    size_t offset = (size_t)((double)f * dt * (double)sampleRate); 
                                    // Let's grab 8192 samples around the point
                                    audioEngine.readAudioFrames(offset, 8192, frameAudio); 
                                    
                                    // Downmix to Mono for FFT
                                    monoAudio.resize(frameAudio.size() / 2); // Assume stereo
                                    for(size_t i=0; i<monoAudio.size(); ++i) {
                                        monoAudio[i] = (frameAudio[i*2] + frameAudio[i*2+1]) * 0.5f;
                                    }

                                    // 2. Update Physics
                                    analysisEngine.computeFFT(monoAudio);
                                    if (particlesEnabled) {
                                        analysisEngine.setBeatSensitivity(beatSensitivity);
                                        bool isBeat = analysisEngine.detectBeat((float)dt);
                                        particleSystem.setParticleCount(particleCount);
                                        particleSystem.setSpeed(particleSpeed);
                                        particleSystem.setSize(particleSize);
                                        particleSystem.setColor(particleColor[0], particleColor[1], particleColor[2], particleColor[3]);
                                        particleSystem.update((float)dt, isBeat);
                                    }

                                    // 3. Render
                                    static GLuint fbo = 0, tex = 0, rbo = 0;
                                    if (fbo == 0) {
                                        glGenFramebuffers(1, &fbo);
                                        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
                                        glGenTextures(1, &tex);
                                        glBindTexture(GL_TEXTURE_2D, tex);
                                        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, vidWidth, vidHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
                                        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
                                        glGenRenderbuffers(1, &rbo);
                                        glBindRenderbuffer(GL_RENDERBUFFER, rbo);
                                        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, vidWidth, vidHeight);
                                        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);
                                        glBindFramebuffer(GL_FRAMEBUFFER, 0);
                                    }
                                    
                                    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
                                    glViewport(0, 0, vidWidth, vidHeight);
                                    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
                                    glClear(GL_COLOR_BUFFER_BIT);

                                    glEnable(GL_BLEND);
                                    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                                    
                                    visualizer.setViewportSize(vidWidth, vidHeight);
                                    for (auto& layer : layers) {
                                        if (!layer.visible) continue;
                                        
                                        std::vector<float> renderData;
                                        if (layer.shape == VisualizerShape::Waveform) {
                                            renderData.assign(frameAudio.begin(), frameAudio.begin() + std::min((size_t)2048, frameAudio.size()));
                                        } else if (layer.shape == VisualizerShape::OscilloscopeXY) {
                                            renderData = frameAudio; // frameAudio is already stereo
                                        } else {
                                            renderData = analysisEngine.computeLayerMagnitudes(layer.config, layer.prevMagnitudes);
                                        }

                                        visualizer.setColor(layer.color[0], layer.color[1], layer.color[2], layer.color[3]);
                                        visualizer.setHeightScale(layer.barHeight);
                                        visualizer.setMirrored(layer.mirrored);
                                        visualizer.setShape(layer.shape);
                                        visualizer.setCornerRadius(layer.cornerRadius);
                                        visualizer.setRotation(layer.rotation * 3.14159f / 180.0f);
                                        visualizer.setFlip(layer.flipX, layer.flipY);
                                        visualizer.setBloomIntensity(layer.bloom);
                                        visualizer.setGridEnabled(layer.showGrid);
                                        visualizer.render(renderData);
                                    }

                                    if (particlesEnabled) {
                                        particleSystem.render();
                                    }
                                    
                                    // 4. Read Pixels
                                    glReadPixels(0, 0, vidWidth, vidHeight, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
                                    glBindFramebuffer(GL_FRAMEBUFFER, 0);
                                    
                                    // Write to Pipe
                                    fwrite(pixels.data(), 1, pixels.size(), pipe);
                                }
                                pclose(pipe);
                                std::cout << "Rendering Complete!" << std::endl;
                            } else {
                                std::cerr << "Failed to open FFMPEG pipe" << std::endl;
                            }
                            isRendering = false;
                            showRenderDialog = false;
                        }
                        
                        if (isRendering) {
                             ImGui::ProgressBar(renderProgress, ImVec2(0.0f, 0.0f));
                             ImGui::SameLine();
                             ImGui::Text("Rendering...");
                        }

                        ImGui::End();
                    }

                } else if (currentAudioMode == ModeTest) {
                    static float testFreq = 440.0f;
                    static float testFreqRight = 440.0f;
                    static bool testStereo = false;
                    static float testVol = 0.5f;
                    static int testType = 0; // Sine default
                    
                    const char* types[] = { "Sine", "Square", "Sawtooth", "Triangle", "White Noise" };
                    if (ImGui::Combo("Waveform", &testType, types, IM_ARRAYSIZE(types))) {
                        audioEngine.setTestToneType((AudioEngine::TestToneType)testType);
                    }

                    if (ImGui::Checkbox("Stereo Mode", &testStereo)) {
                        audioEngine.setTestToneStereo(testStereo);
                    }

                    if (testStereo) {
                        if (ImGui::SliderFloat("Frequency L (Hz)", &testFreq, 20.0f, 2000.0f)) {
                            audioEngine.setTestToneFrequency(testFreq);
                        }
                        if (ImGui::SliderFloat("Frequency R (Hz)", &testFreqRight, 20.0f, 2000.0f)) {
                            audioEngine.setTestToneFrequencyRight(testFreqRight);
                        }
                    } else {
                        if (ImGui::SliderFloat("Frequency (Hz)", &testFreq, 20.0f, 2000.0f)) {
                            audioEngine.setTestToneFrequency(testFreq);
                        }
                    }
                    
                    if (ImGui::SliderFloat("Volume", &testVol, 0.0f, 1.0f)) {
                        audioEngine.setTestToneVolume(testVol);
                    }

                    if (ImGui::Button("Start Tone")) {
                        audioEngine.initTestTone();
                        audioEngine.play();
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Stop Tone")) {
                        audioEngine.stop();
                    }
                } else if (currentAudioMode == ModeOscMusic) {
                    ImGui::Text("Oscilloscope Music Editor");
                    
                    static char xExpr[256] = "sin(f * t * 2 * 3.14159)";
                    static char yExpr[256] = "cos(f * t * 2 * 3.14159)";
                    static float duration = 2.0f;
                    static float baseFreq = 440.0f;
                    
                    if (ImGui::InputText("X(t) Expression", xExpr, sizeof(xExpr))) {
                        oscMusicEditor.setExpressions(xExpr, yExpr);
                    }
                    if (ImGui::InputText("Y(t) Expression", yExpr, sizeof(yExpr))) {
                        oscMusicEditor.setExpressions(xExpr, yExpr);
                    }
                    if (ImGui::SliderFloat("Base Frequency (f)", &baseFreq, 20.0f, 2000.0f, "%.1f Hz")) {
                        oscMusicEditor.setBaseFrequency(baseFreq);
                    }
                    ImGui::SliderFloat("Duration (s)", &duration, 0.5f, 10.0f);
                    
                    if (!oscMusicEditor.isValid()) {
                        ImGui::TextColored(ImVec4(1, 0, 0, 1), "Error: %s", oscMusicEditor.getErrorMessage().c_str());
                    }

                    if (ImGui::Button("Preview")) {
                        if (oscMusicEditor.isValid()) {
                            std::vector<float> buffer = oscMusicEditor.generateStereoBuffer(duration, 192000);
                            audioEngine.setOscMusicBuffer(buffer);
                            audioEngine.initOscMusic(192000);
                            audioEngine.setOscMusicMode(true);
                            audioEngine.play();
                            
                            // Auto-set visualization to XY
                            for (auto& l : layers) {
                                if (l.visible) {
                                    l.shape = VisualizerShape::OscilloscopeXY;
                                }
                            }
                        }
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Stop Preview")) {
                        audioEngine.stop();
                    }
                    
                    ImGui::Separator();
                    ImGui::Text("Presets");
                    auto presets = oscMusicEditor.getPresets();
                    for (int i = 0; i < (int)presets.size(); i++) {
                        if (ImGui::Button(presets[i].name.c_str())) {
                            oscMusicEditor.loadPreset(i);
                            strncpy(xExpr, oscMusicEditor.getXExpression().c_str(), sizeof(xExpr));
                            strncpy(yExpr, oscMusicEditor.getYExpression().c_str(), sizeof(yExpr));
                        }
                        if ((i + 1) % 4 != 0) ImGui::SameLine();
                    }
                    ImGui::NewLine();
                }
            }
            ImGui::End();
        }

        // Layer Manager Window
        if (showLayerManager) {
            ImGui::Begin("Layer Manager", &showLayerManager);
            if (ImGui::Button("Add Layer")) {
                layers.push_back({"New Layer", LayerConfig(), {1.0f, 1.0f, 1.0f, 1.0f}, 0.2f, false, VisualizerShape::Bars, 0.0f, {}, true});
                selectedLayerIdx = (int)layers.size() - 1;
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

            ImGui::Separator();
            ImGui::Text("Visuals");
            ImGui::Checkbox("Mirror Mode", &layer.mirrored);
            
            const char* shapes[] = { "Bars", "Lines", "Dots", "Waveform", "Oscilloscope XY" };
            int currentShape = (int)layer.shape;
            if (ImGui::Combo("Shape", &currentShape, shapes, IM_ARRAYSIZE(shapes))) {
                layer.shape = (VisualizerShape)currentShape;
            }
            
            if (layer.shape == VisualizerShape::Bars) {
                ImGui::SliderFloat("Corner Radius", &layer.cornerRadius, 0.0f, 1.0f);
            }
            
            if (layer.shape == VisualizerShape::Waveform || layer.shape == VisualizerShape::OscilloscopeXY) {
                 ImGui::SliderFloat("Time Scale", &layer.timeScale, 0.2f, 2.0f);
            }
            
            if (layer.shape == VisualizerShape::OscilloscopeXY) {
                ImGui::SliderFloat("Rotation", &layer.rotation, 0.0f, 360.0f);
                ImGui::Checkbox("Flip Horizontal", &layer.flipX);
                ImGui::Checkbox("Flip Vertical", &layer.flipY);
                ImGui::SliderFloat("Bloom Intensity", &layer.bloom, 0.0f, 5.0f);
                ImGui::Separator();
                ImGui::SliderFloat("Trace Thickness", &layer.traceWidth, 1.0f, 10.0f);
            }

            ImGui::End();
        }

        // Playlist Window
        if (showPlaylist) {
            ImGui::Begin("Playlist", &showPlaylist);
            if (ImGui::InputText("Music Folder", musicFolderPath, sizeof(musicFolderPath))) {
                scanMusicFolder(musicFolderPath);
            }
            ImGui::SameLine();
            if (ImGui::Button("Refresh")) scanMusicFolder(musicFolderPath);

            ImGui::Separator();
            if (ImGui::BeginChild("FileList")) {
                for (const auto& file : playlistFiles) {
                    bool isSelected = (std::string(filePath).find(file) != std::string::npos);
                    if (ImGui::Selectable(file.c_str(), isSelected)) {
                        fs::path fullPath = fs::path(musicFolderPath) / file;
                        strncpy(filePath, fullPath.string().c_str(), sizeof(filePath));
                        if (audioEngine.loadFile(filePath)) {
                            currentAudioMode = ModeFile;
                            audioEngine.play();
                            statusMessage = "Playing: " + file;
                            statusColor = ImVec4(0, 1, 0, 1);
                        } else {
                            statusMessage = "Failed to load: " + file;
                            statusColor = ImVec4(1, 0, 0, 1);
                        }
                    }
                }
            }
            ImGui::EndChild();
            ImGui::End();
        }

        // Particle Settings Window
        if (showParticleSettings) {
            ImGui::Begin("Particle Settings", &showParticleSettings);
            ImGui::Checkbox("Enable Particles", &particlesEnabled);
            
            if (particlesEnabled) {
                ImGui::Separator();
                ImGui::Text("Beat Detection");
                ImGui::SliderFloat("Sensitivity", &beatSensitivity, 1.0f, 2.0f);
                if (isBeat) ImGui::TextColored(ImVec4(0,1,0,1), "BEAT!");
                else ImGui::Text("Listening...");

                ImGui::Separator();
                ImGui::Text("Particle Physics");
                ImGui::SliderInt("Max Count", &particleCount, 100, 2000);
                ImGui::SliderFloat("Speed/Energy", &particleSpeed, 0.1f, 5.0f);
                ImGui::SliderFloat("Size", &particleSize, 1.0f, 20.0f);
                ImGui::ColorEdit4("Color", particleColor);
            }
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
