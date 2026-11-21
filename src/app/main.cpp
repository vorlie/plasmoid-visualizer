#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include "AudioEngine.hpp"
#include "AnalysisEngine.hpp"
#include "Visualizer.hpp"
#include "OscMusicEditor.hpp"
#include "SystemStats.hpp"
#include "AppState.hpp"
#include "RenderManager.hpp"
#include "UIManager.hpp"
#include "VideoRenderManager.hpp"
#include "ConfigLogic.hpp"
#include "ConfigManager.hpp"
#include "AssetPaths.hpp"
#include "CliOptions.hpp"
#include "Utf8Paths.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <sstream>
#include <thread>
#include <tuple>
#include <vector>
#include <string>

#ifdef _WIN32
#include <Windows.h>
#include <timeapi.h>
#endif

namespace {
std::filesystem::path japaneseFallbackFont() {
#ifdef _WIN32
    if (const wchar_t* windows = _wgetenv(L"WINDIR")) {
        const auto fonts = std::filesystem::path(windows) / L"Fonts";
        const wchar_t* candidates[] = {L"YuGothM.ttc", L"meiryo.ttc", L"msgothic.ttc"};
        for (const wchar_t* candidate : candidates) {
            const auto path = fonts / candidate;
            std::error_code ec;
            if (std::filesystem::exists(path, ec) && !ec) return path;
        }
    }
#endif
    return {};
}

void rebuildImGuiFonts(ImGuiIO& io, const std::string& preferredPath) {
    io.Fonts->Clear();
    std::filesystem::path preferred;
    if (!preferredPath.empty()) {
        const auto candidate = Utf8Paths::fromUtf8(preferredPath);
        std::error_code ec;
        if (std::filesystem::exists(candidate, ec) && !ec) {
            preferred = candidate;
        }
    }
    if (preferred.empty()) {
        preferred = AssetPaths::defaultFont();
    }
    ImFontConfig primaryConfig;
    primaryConfig.FontNo = 0;
    ImFont* primary = nullptr;
    if (!preferred.empty()) {
        const std::string preferredUtf8 = Utf8Paths::toUtf8(preferred);
        primary = io.Fonts->AddFontFromFileTTF(
            preferredUtf8.c_str(), 18.0f, &primaryConfig,
            io.Fonts->GetGlyphRangesJapanese());
    }
    if (!primary) primary = io.Fonts->AddFontDefault();

    const auto fallback = japaneseFallbackFont();
    if (!fallback.empty() && fallback != preferred) {
        ImFontConfig mergeConfig;
        mergeConfig.MergeMode = true;
        mergeConfig.FontNo = 0;
        mergeConfig.DstFont = primary;
        const std::string fallbackUtf8 = Utf8Paths::toUtf8(fallback);
        io.Fonts->AddFontFromFileTTF(
            fallbackUtf8.c_str(), 18.0f, &mergeConfig,
            io.Fonts->GetGlyphRangesJapanese());
    }
}
#ifndef NDEBUG
void APIENTRY glDebugCallback(
    GLenum source,
    GLenum type,
    GLuint id,
    GLenum severity,
    GLsizei length,
    const GLchar* message,
    const void* userParam
) {
    (void)source;
    (void)length;
    (void)userParam;
    if (severity == GL_DEBUG_SEVERITY_NOTIFICATION) return;
    std::cerr << "[OpenGL Debug] type=0x" << std::hex << type
              << " id=" << std::dec << id
              << " severity=0x" << std::hex << severity << std::dec
              << ": " << (message ? message : "") << std::endl;
}
#endif
struct HighResTimerScope {
#ifdef _WIN32
    HighResTimerScope() { timeBeginPeriod(1); }
    ~HighResTimerScope() { timeEndPeriod(1); }
#else
    HighResTimerScope() = default;
    ~HighResTimerScope() = default;
#endif
    HighResTimerScope(const HighResTimerScope&) = delete;
    HighResTimerScope& operator=(const HighResTimerScope&) = delete;
};

double processCpuSeconds() {
#ifdef _WIN32
    FILETIME creation{}, exitT{}, kernel{}, user{};
    if (!GetProcessTimes(GetCurrentProcess(), &creation, &exitT, &kernel, &user)) return 0.0;
    auto toSec = [](const FILETIME& ft) {
        const unsigned long long ticks =
            (static_cast<unsigned long long>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
        return static_cast<double>(ticks) * 1e-7;
    };
    return toSec(kernel) + toSec(user);
#else
    return 0.0;
#endif
}

OverlayBackgroundType detectBackgroundType(const std::string& path) {
    std::string ext = Utf8Paths::toUtf8(Utf8Paths::fromUtf8(path).extension());
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    if (ext == ".gif") return OverlayBackgroundType::AnimatedGif;
    if (ext == ".mp4" || ext == ".mkv" || ext == ".mov" || ext == ".webm" || ext == ".avi") {
        return OverlayBackgroundType::LoopedVideo;
    }
    return OverlayBackgroundType::StillImage;
}

bool applyExportCliOptions(AppState& state, const ExportCliOptions& job, std::string& error) {
    // Load user config.toml as baseline defaults when available.
    ConfigManager::load("", state.config);

    if (!job.presetNameOrPath.empty()) {
        const std::string resolved = ConfigManager::resolvePresetPath(
            job.presetNameOrPath,
            Utf8Paths::toUtf8(AssetPaths::presetsDirectory()),
            ConfigManager::getPresetsDirectory());
        if (resolved.empty()) {
            error = "Preset not found: " + job.presetNameOrPath;
            return false;
        }
        if (!ConfigManager::loadPreset(resolved, state.config)) {
            error = "Failed to parse preset: " + resolved;
            return false;
        }
        std::cout << "Loaded preset: " << resolved << std::endl;
    }

    state.fx.filePath = job.inputAudioPath;
    state.config.videoSettings.outputPath = job.outputVideoPath;
    if (job.width.has_value()) state.config.videoSettings.width = *job.width;
    if (job.height.has_value()) state.config.videoSettings.height = *job.height;
    if (job.fps.has_value()) state.config.videoSettings.fps = *job.fps;
    if (job.codecIdx.has_value()) state.config.videoSettings.codecIdx = *job.codecIdx;
    if (job.crf.has_value()) state.config.videoSettings.crf = *job.crf;

    if (job.disableOverlay) {
        state.config.mediaOverlay.enabled = false;
    }
    if (job.artist.has_value()) {
        state.config.mediaOverlay.artist = *job.artist;
    }
    if (job.title.has_value()) {
        state.config.mediaOverlay.title = *job.title;
    }
    if (!job.backgroundPath.empty()) {
        state.config.mediaOverlay.backgroundType = detectBackgroundType(job.backgroundPath);
        state.config.mediaOverlay.backgroundPath = job.backgroundPath;
    }
    if (!job.lyricsPath.empty()) {
        std::ifstream input(Utf8Paths::fromUtf8(job.lyricsPath), std::ios::binary);
        if (!input) {
            error = "Could not open lyrics file: " + job.lyricsPath;
            return false;
        }
        std::ostringstream contents;
        contents << input.rdbuf();
        state.config.mediaOverlay.lyricFilePath = job.lyricsPath;
        state.config.mediaOverlay.lyricSource = contents.str();
        std::string parseError;
        if (!state.config.mediaOverlay.lyrics.parseLrc(state.config.mediaOverlay.lyricSource, &parseError)) {
            error = "Failed to parse lyrics file '" + job.lyricsPath + "': " + parseError;
            return false;
        }
    }

    if (state.config.layers.empty()) {
        VisualizerLayer defaultLayer;
        defaultLayer.id = state.allocateLayerId();
        defaultLayer.name = "Default Bars";
        state.config.layers.push_back(std::move(defaultLayer));
    }
    return true;
}

int runHeadlessExport(const ExportCliOptions& job) try {
    AudioEngine audioEngine;
    AnalysisEngine analysisEngine(8192);
    Visualizer visualizer;
    AppState state;
    RenderManager renderManager;
    VideoRenderManager videoRenderManager;

    std::string setupError;
    if (!applyExportCliOptions(state, job, setupError)) {
        std::cerr << "Export setup error: " << setupError << std::endl;
        return 1;
    }

    videoRenderManager.startRender(state, audioEngine);
    if (!state.fx.videoStatus.isRendering) {
        std::cerr << "Export failed to start: "
                  << (state.fx.videoStatus.errorMessage.empty()
                          ? "Unknown error"
                          : state.fx.videoStatus.errorMessage)
                  << std::endl;
        return 1;
    }

    const auto startWall = std::chrono::steady_clock::now();
    auto lastLogWall = startWall;

    while (state.fx.videoStatus.isRendering) {
        glfwPollEvents();
        videoRenderManager.update(
            state,
            audioEngine,
            analysisEngine,
            visualizer,
            renderManager);

        const auto now = std::chrono::steady_clock::now();
        if (state.fx.videoStatus.isRendering &&
            std::chrono::duration<double>(now - lastLogWall).count() >= 0.5) {
            const double elapsed = std::max(std::chrono::duration<double>(now - startWall).count(), 1e-6);
            const int cur = state.fx.videoStatus.currentFrame;
            const int total = std::max(state.fx.videoStatus.totalFrames, 1);
            const double pct = 100.0 * static_cast<double>(cur) / static_cast<double>(total);
            const double fps = static_cast<double>(cur) / elapsed;
            const double eta = fps > 0.01 ? static_cast<double>(std::max(total - cur, 0)) / fps : 0.0;
            std::cout << std::fixed << std::setprecision(1)
                      << "Export: " << cur << "/" << total
                      << " (" << pct << "%) | " << fps << " fps | elapsed "
                      << elapsed << "s | ETA " << eta << "s" << std::endl;
            lastLogWall = now;
        }
    }

    if (!state.fx.videoStatus.errorMessage.empty()) {
        std::cerr << "Export failed: " << state.fx.videoStatus.errorMessage << std::endl;
        return 1;
    }

    const double totalElapsed = std::max(
        std::chrono::duration<double>(std::chrono::steady_clock::now() - startWall).count(), 1e-6);
    const int totalFrames = state.fx.videoStatus.totalFrames;
    std::cout << std::fixed << std::setprecision(2)
              << "Export complete: " << totalFrames << " frames in " << totalElapsed
              << "s (" << (static_cast<double>(totalFrames) / totalElapsed) << " fps) -> "
              << state.config.videoSettings.outputPath << std::endl;
    return 0;
} catch (const std::exception& e) {
    std::cerr << "Export error: " << e.what() << std::endl;
    return 1;
}

void configureBenchmarkState(AppState& state, const BenchmarkOptions& cfg) {
    state.config.layers.clear();
    state.config.enableVsync = cfg.vsync;
    state.config.targetFps = cfg.targetFps;
    state.ui.showLayerManager = false;
    state.ui.showGlobalSettings = false;
    state.ui.showAudioSettings = false;
    state.config.mediaOverlay.enabled = true;

    for (int i = 0; i < cfg.layers; ++i) {
        VisualizerLayer layer;
        layer.id = state.allocateLayerId();
        layer.name = "Bench Layer " + std::to_string(i + 1);
        layer.visible = true;
        layer.config.numBars = 256;
        layer.config.smoothing = 1;
        if (cfg.layers >= 5 && i == cfg.layers - 1) {
            layer.shape = VisualizerShape::OscilloscopeXY;
            layer.xy.persistence = true;
            layer.xy.bloom = 1.2f;
        } else if (i == 1) {
            layer.shape = VisualizerShape::Curve;
            layer.fillOpacity = 0.35f;
        } else {
            layer.shape = VisualizerShape::Bars;
            layer.cornerRadius = 0.2f;
        }
        state.config.layers.push_back(std::move(layer));
    }
}
}

int main(int argc, char** argv) {
#ifdef _WIN32
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
#endif
    const CliOptions cli = CliParser::parse(argc, argv);
    if (cli.showHelp) {
        std::cout << CliParser::usageText();
        return 0;
    }
    if (!cli.errorMessage.empty()) {
        std::cerr << "Error: " << cli.errorMessage << "\n\n" << CliParser::usageText();
        return 2;
    }
    if (!cli.shaderDirOverride.empty()) {
        const auto shaderDir = Utf8Paths::fromUtf8(cli.shaderDirOverride);
        std::error_code ec;
        if (!std::filesystem::is_directory(shaderDir, ec) || ec) {
            std::cerr << "Error: Shader directory does not exist: " << cli.shaderDirOverride << std::endl;
            return 2;
        }
        AssetPaths::setShaderDirectoryOverride(shaderDir);
    }

    const BenchmarkOptions& bench = cli.benchmark;
    HighResTimerScope highResTimer;

    if (!glfwInit()) return -1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifndef NDEBUG
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
#endif
    glfwWindowHint(GLFW_SAMPLES, 4);
    if (cli.exportJob.enabled) {
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    }

    const int initialWindowW = cli.exportJob.enabled ? 64 : 800;
    const int initialWindowH = cli.exportJob.enabled ? 64 : 800;
    GLFWwindow* window = glfwCreateWindow(
        initialWindowW, initialWindowH, "Plasmoid Visualizer Standalone", NULL, NULL);
    if (!window) {
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);

    glewExperimental = GL_TRUE;
    const GLenum glewStatus = glewInit();
    if (glewStatus != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW: "
                  << reinterpret_cast<const char*>(glewGetErrorString(glewStatus)) << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }
    // Clear benign GL_INVALID_ENUM that glewExperimental may emit on Core Profile.
    while (glGetError() != GL_NO_ERROR) {}

#ifndef NDEBUG
    if (GLEW_KHR_debug || GLEW_VERSION_4_3) {
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageCallback(glDebugCallback, nullptr);
        glDebugMessageControl(
            GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION,
            0, nullptr, GL_FALSE);
    }
#endif

    if (cli.exportJob.enabled) {
        const int exitCode = runHeadlessExport(cli.exportJob);
        glfwDestroyWindow(window);
        glfwTerminate();
        return exitCode;
    }
    // glfwSwapInterval(1); // Will be set after config load

    // Setup ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    std::string imguiIniPath;
    if (bench.enabled) {
        io.IniFilename = nullptr;
    } else {
        imguiIniPath = ConfigManager::getImGuiIniPath();
        io.IniFilename = imguiIniPath.c_str();
    }
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // Fullscreen toggle & shader reload hotkey support
    bool g_isFullscreen = false;
    int g_windowedPosX = 0, g_windowedPosY = 0, g_windowedWidth = 800, g_windowedHeight = 800;
    bool f11Prev = false;
    bool f5Prev = false;
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

    // Scope runtime and OpenGL-owning objects so they are destroyed while the
    // OpenGL context is still current, before ImGui/GLFW teardown.
    int exitCode = 0;
    try {
        AudioEngine audioEngine;
        AnalysisEngine analysisEngine(8192);
        Visualizer visualizer;
        OscMusicEditor oscMusicEditor;
        SystemStats systemStats;

        AppState state;
        RenderManager renderManager;
        UIManager uiManager;
        VideoRenderManager videoRenderManager;

        // Initial load
        if (bench.enabled) {
            configureBenchmarkState(state, bench);
        } else {
            ConfigLogic::loadSettings(state);
            if (cli.watchShaders) {
                state.ui.shaderHotReloadEnabled = true;
            }
        }
        audioEngine.setLoopTrack(state.config.loopTrack);
        std::string loadedImGuiFontPath = state.config.mediaOverlay.fontPath;
        rebuildImGuiFonts(io, loadedImGuiFontPath);
        
        // Apply VSync
        glfwSwapInterval(state.config.enableVsync ? 1 : 0);
        bool prevVsync = state.config.enableVsync;
        
        // Frame limiting state
        double lastTime = glfwGetTime();

        // Benchmark tracking
        double benchPhase = 0.0;
        const double benchStartWall = glfwGetTime();
        bool benchMeasuring = false;
        double benchMeasureStartWall = 0.0;
        double benchMeasureStartCpu = 0.0;
        std::vector<double> benchFrameMs;
        std::vector<double> benchWorkMs;
        if (bench.enabled) {
            audioEngine.pushSyntheticFrames(8192, benchPhase, 48000);
            benchFrameMs.reserve(1024);
            benchWorkMs.reserve(1024);
        }

        while (!glfwWindowShouldClose(window)) {
            const double frameStart = glfwGetTime();
            if (bench.enabled) {
                audioEngine.pushSyntheticFrames(800, benchPhase, 48000);
                if (!benchMeasuring && (frameStart - benchStartWall) >= bench.warmupSeconds) {
                    benchMeasuring = true;
                    benchMeasureStartWall = frameStart;
                    benchMeasureStartCpu = processCpuSeconds();
                } else if (benchMeasuring && (frameStart - benchMeasureStartWall) >= bench.durationSeconds) {
                    break;
                }
            }
            // Runtime VSync toggle
            if (state.config.enableVsync != prevVsync) {
                glfwSwapInterval(state.config.enableVsync ? 1 : 0);
                prevVsync = state.config.enableVsync;
            }

            glfwPollEvents();

            // Surface RT decoder errors on main thread without logging on the audio callback.
            if (const auto decoderErrors = audioEngine.consumeDecoderErrorCount(); decoderErrors > 0) {
                std::cerr << "Audio decoder reported " << decoderErrors << " read error(s)." << std::endl;
            }

            // Handle end-of-track when Loop Track is disabled: advance playlist or stop cleanly.
            audioEngine.setLoopTrack(state.config.loopTrack);
            if (state.config.currentAudioMode == AudioMode::File &&
                !state.fx.videoStatus.isRendering &&
                audioEngine.consumeTrackEnded()) {
                if (!state.fx.musicFiles.empty() && !state.config.musicFolderPath.empty()) {
                    const std::string currentFile = Utf8Paths::toUtf8(Utf8Paths::fromUtf8(state.fx.filePath).filename());
                    size_t nextIdx = 0;
                    for (size_t i = 0; i < state.fx.musicFiles.size(); ++i) {
                        if (state.fx.musicFiles[i] == currentFile) {
                            nextIdx = (i + 1) % state.fx.musicFiles.size();
                            break;
                        }
                    }
                    const auto nextPath =
                        Utf8Paths::fromUtf8(state.config.musicFolderPath) /
                        Utf8Paths::fromUtf8(state.fx.musicFiles[nextIdx]);
                    state.fx.filePath = Utf8Paths::toUtf8(nextPath);
                    if (audioEngine.loadFile(state.fx.filePath)) {
                        audioEngine.play();
                        state.ui.statusMessage = "Playing: " + state.fx.musicFiles[nextIdx];
                        state.ui.statusColor = ImVec4(0, 1, 0, 1);
                    } else {
                        audioEngine.stop();
                        state.ui.statusMessage = "Failed to load: " + state.fx.musicFiles[nextIdx];
                        state.ui.statusColor = ImVec4(1, 0, 0, 1);
                    }
                } else {
                    audioEngine.stop();
                }
            }

            const std::string& requestedImGuiFont = state.config.mediaOverlay.fontPath;
            if (requestedImGuiFont != loadedImGuiFontPath) {
                rebuildImGuiFonts(io, requestedImGuiFont);
                loadedImGuiFontPath = requestedImGuiFont;
            }

            // Handle F11 fullscreen toggle (edge-triggered)
            bool f11 = (glfwGetKey(window, GLFW_KEY_F11) == GLFW_PRESS);
            if (f11 && !f11Prev) {
                toggleFullscreen(window);
            }
            f11Prev = f11;

            // Handle F5 manual shader reload & optional live hot-reload polling
            const bool f5 = (glfwGetKey(window, GLFW_KEY_F5) == GLFW_PRESS);
            const bool f5Pressed = f5 && !f5Prev;
            f5Prev = f5;
            if (!bench.enabled) {
                renderManager.pollShaderHotReload(visualizer, state, io.DeltaTime, f5Pressed);
            }

            int width, height;
            glfwGetFramebufferSize(window, &width, &height);
            if (width <= 0 || height <= 0) {
                if (state.fx.videoStatus.isRendering) {
                    videoRenderManager.update(
                        state,
                        audioEngine,
                        analysisEngine,
                        visualizer,
                        renderManager
                    );
                } else {
                    glfwWaitEventsTimeout(0.05);
                }
                lastTime = glfwGetTime();
                continue;
            }

            // Record frame time for statistics
            systemStats.recordFrameTime(io.DeltaTime);

            // 1. Render Visualization Frame OR Offline Video Export Tick
            // Pausing the live pass during offline export keeps persistence/HDR FBOs
            // at export resolution (fixing broken CRT persistence & FBO thrashing)
            // while presenting the rendered export frame as a live preview.
            if (state.fx.videoStatus.isRendering) {
                videoRenderManager.update(
                    state,
                    audioEngine,
                    analysisEngine,
                    visualizer,
                    renderManager
                );
                renderManager.presentOfflinePreview(visualizer, width, height);
            } else {
                renderManager.renderFrame(
                    state,
                    audioEngine,
                    analysisEngine,
                    visualizer,
                    width,
                    height,
                    io.DeltaTime
                );
            }

            // 3. Render UI
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            uiManager.renderUI(
                state, 
                audioEngine, 
                oscMusicEditor, 
                systemStats,
                videoRenderManager,
                window
            );

            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

            const double workEnd = glfwGetTime();
            glfwSwapBuffers(window);
            
            // Frame limiting: sleep until ~1.5 ms before deadline, then yield-spin
            // the final sub-millisecond window for accurate pacing at low CPU usage.
            if (!state.config.enableVsync && state.config.targetFps > 0) {
                const double targetFrameTime = 1.0 / static_cast<double>(state.config.targetFps);
                const double deadline = lastTime + targetFrameTime;
                constexpr double kSpinMarginSeconds = 0.0015;
                double now = glfwGetTime();
                if (now + kSpinMarginSeconds < deadline) {
                    const double sleepSeconds = (deadline - now) - kSpinMarginSeconds;
                    std::this_thread::sleep_for(std::chrono::duration<double>(sleepSeconds));
                }
                while ((now = glfwGetTime()) < deadline) {
                    std::this_thread::yield();
                }
                lastTime = (now - deadline > targetFrameTime) ? now : deadline;
            } else {
                lastTime = glfwGetTime();
            }

            if (bench.enabled && benchMeasuring) {
                const double frameEnd = glfwGetTime();
                benchWorkMs.push_back((workEnd - frameStart) * 1000.0);
                benchFrameMs.push_back((frameEnd - frameStart) * 1000.0);
            }
        }

        if (bench.enabled && !benchFrameMs.empty()) {
            const double elapsedWall = std::max(glfwGetTime() - benchMeasureStartWall, 1e-6);
            const double elapsedCpu = std::max(processCpuSeconds() - benchMeasureStartCpu, 0.0);
            const double cpuPct = (elapsedCpu / elapsedWall) * 100.0;
            auto stats = [](std::vector<double> v) {
                std::sort(v.begin(), v.end());
                const double avg = std::accumulate(v.begin(), v.end(), 0.0) / static_cast<double>(v.size());
                const double p50 = v[v.size() / 2];
                const double p95 = v[std::min(v.size() - 1, static_cast<size_t>(v.size() * 0.95))];
                return std::tuple<double, double, double>{avg, p50, p95};
            };
            const auto [fAvg, fP50, fP95] = stats(benchFrameMs);
            const auto [wAvg, wP50, wP95] = stats(benchWorkMs);
            std::cout << std::fixed << std::setprecision(2)
                      << "BENCH layers=" << bench.layers
                      << " vsync=" << (bench.vsync ? 1 : 0)
                      << " target_fps=" << bench.targetFps
                      << " frames=" << benchFrameMs.size()
                      << " fps=" << (static_cast<double>(benchFrameMs.size()) / elapsedWall)
                      << " work_avg_ms=" << wAvg << " work_p50_ms=" << wP50 << " work_p95_ms=" << wP95
                      << " frame_avg_ms=" << fAvg << " frame_p95_ms=" << fP95
                      << " cpu_core_pct=" << cpuPct
                      << std::endl;
        }

        // Auto-save configuration on clean exit
        if (!bench.enabled) {
            ConfigLogic::saveSettings(state);
        }
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        exitCode = 1;
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return exitCode;
}
