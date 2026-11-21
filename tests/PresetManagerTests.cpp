#include "ConfigManager.hpp"
#include "Utf8Paths.hpp"

#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

namespace {
bool near(float a, float b, float eps = 1e-4f) {
    return std::abs(a - b) <= eps;
}

fs::path makeTempDir(const std::string& name) {
    const fs::path dir = fs::temp_directory_path() / name;
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir, ec);
    return dir;
}

bool testSanitizePresetFilename() {
    if (ConfigManager::sanitizePresetFilename("../evil/../../path") != "evilpath") {
        std::cerr << "Directory traversal was not stripped cleanly: "
                  << ConfigManager::sanitizePresetFilename("../evil/../../path") << "\n";
        return false;
    }
    if (ConfigManager::sanitizePresetFilename("My Neon: Preset*?") != "My_Neon_Preset") {
        std::cerr << "Illegal filename characters were not stripped cleanly: "
                  << ConfigManager::sanitizePresetFilename("My Neon: Preset*?") << "\n";
        return false;
    }
    if (ConfigManager::sanitizePresetFilename("...") != "preset" ||
        ConfigManager::sanitizePresetFilename("   ") != "preset") {
        std::cerr << "Degenerate preset names should fall back to 'preset'\n";
        return false;
    }
    return true;
}

bool testPresetRoundTripAndIsolation(const fs::path& dir) {
    PersistentConfig src;
    src.musicFolderPath = "D:/Private/Music";
    src.globalGain = 3.5f;
    src.enableVsync = false;
    src.targetFps = 144;
    src.currentAudioMode = AudioMode::Capture;
    src.selectedCaptureDeviceName = "Private Mic";
    src.backgroundImagePath = "D:/Private/bg.png";
    src.videoSettings.outputPath = "D:/Private/out.mp4";
    src.mediaOverlay.artist = "Original Artist";
    src.mediaOverlay.title = "Original Title";
    src.mediaOverlay.lyricSource = "[00:01.00]Original Lyric";
    src.mediaOverlay.fontPath = "D:/Private/font.ttf";
    src.mediaOverlay.backgroundPath = "D:/Private/video.mp4";

    // Visual settings that SHOULD be saved by the preset:
    src.zenKunModeEnabled = true;
    src.bgPulseIntensity = 0.14f;
    src.oscilloscopeDisplay.profile = ScopeProfile::Analog;
    src.oscilloscopeDisplay.graticuleColumns = 14;
    src.oscilloscopeDisplay.phosphorFastDecayMs = 45.0f;
    src.mediaOverlay.enabled = true;
    src.mediaOverlay.style.spectrumBarCount = 144;
    src.mediaOverlay.style.lyricsBandOpacity = 0.42f;

    VisualizerLayer layer;
    layer.id = 3;
    layer.name = "Preset Layer";
    layer.shape = VisualizerShape::OscilloscopeXY;
    layer.xy.bloom = 2.2f;
    layer.xy.traceWidth = 3.0f;
    src.layers = { layer };

    const std::string presetPath = Utf8Paths::toUtf8(dir / "custom_look.toml");
    if (!ConfigManager::savePreset(presetPath, src, "Custom Look")) {
        std::cerr << "savePreset failed\n";
        return false;
    }

    // Load into a target config with distinct machine-local/project-local values.
    PersistentConfig dst;
    dst.musicFolderPath = "E:/KeepThisFolder";
    dst.globalGain = 1.25f;
    dst.enableVsync = true;
    dst.targetFps = 60;
    dst.currentAudioMode = AudioMode::File;
    dst.selectedCaptureDeviceName = "KeepThisDevice";
    dst.backgroundImagePath = "E:/KeepBg.png";
    dst.videoSettings.outputPath = "E:/KeepOut.mp4";
    dst.mediaOverlay.artist = "Keep Artist";
    dst.mediaOverlay.title = "Keep Title";
    dst.mediaOverlay.lyricSource = "[00:02.00]Keep Lyric";
    dst.mediaOverlay.fontPath = "E:/KeepFont.ttf";
    dst.mediaOverlay.backgroundPath = "E:/KeepMedia.mp4";

    if (!ConfigManager::loadPreset(presetPath, dst)) {
        std::cerr << "loadPreset failed\n";
        return false;
    }

    // Verify visual fields were applied.
    if (!dst.zenKunModeEnabled || !near(dst.bgPulseIntensity, 0.14f) ||
        dst.oscilloscopeDisplay.profile != ScopeProfile::Analog ||
        dst.oscilloscopeDisplay.graticuleColumns != 14 ||
        dst.mediaOverlay.style.spectrumBarCount != 144 ||
        !near(dst.mediaOverlay.style.lyricsBandOpacity, 0.42f) ||
        dst.layers.size() != 1 ||
        dst.layers[0].name != "Preset Layer" ||
        !near(dst.layers[0].xy.bloom, 2.2f)) {
        std::cerr << "Visual fields did not survive preset round-trip\n";
        return false;
    }

    // Verify machine-local & track-specific fields were NOT overwritten.
    if (dst.musicFolderPath != "E:/KeepThisFolder" ||
        !near(dst.globalGain, 1.25f) ||
        dst.enableVsync != true ||
        dst.targetFps != 60 ||
        dst.currentAudioMode != AudioMode::File ||
        dst.selectedCaptureDeviceName != "KeepThisDevice" ||
        dst.backgroundImagePath != "E:/KeepBg.png" ||
        dst.videoSettings.outputPath != "E:/KeepOut.mp4" ||
        dst.mediaOverlay.artist != "Keep Artist" ||
        dst.mediaOverlay.title != "Keep Title" ||
        dst.mediaOverlay.lyricSource != "[00:02.00]Keep Lyric" ||
        dst.mediaOverlay.fontPath != "E:/KeepFont.ttf" ||
        dst.mediaOverlay.backgroundPath != "E:/KeepMedia.mp4") {
        std::cerr << "loadPreset overwrote machine-local or track-specific fields\n";
        return false;
    }
    return true;
}

bool testDiscoveryResolutionAndDelete(const fs::path& dir) {
    const fs::path builtInDir = dir / "builtin";
    const fs::path userDir = dir / "user";
    std::error_code ec;
    fs::create_directories(builtInDir, ec);
    fs::create_directories(userDir, ec);

    PersistentConfig cfg;
    VisualizerLayer l;
    l.name = "L1";
    cfg.layers = { l };

    ConfigManager::savePreset(Utf8Paths::toUtf8(builtInDir / "shared_name.toml"), cfg, "Shared Name");
    ConfigManager::savePreset(Utf8Paths::toUtf8(userDir / "shared_name.toml"), cfg, "Shared Name");
    ConfigManager::savePreset(Utf8Paths::toUtf8(userDir / "user_only.toml"), cfg, "User Only");

    const std::string builtInUtf8 = Utf8Paths::toUtf8(builtInDir);
    const std::string userUtf8 = Utf8Paths::toUtf8(userDir);

    const auto list = ConfigManager::listPresets(builtInUtf8, userUtf8);
    if (list.size() != 3 || !list[0].builtIn || list[1].builtIn || list[2].builtIn) {
        std::cerr << "listPresets ordering or count mismatch\n";
        return false;
    }

    // Resolution must prefer user preset over built-in preset when names match.
    const std::string resolved = ConfigManager::resolvePresetPath("Shared Name", builtInUtf8, userUtf8);
    if (resolved != Utf8Paths::toUtf8(userDir / "shared_name.toml")) {
        std::cerr << "resolvePresetPath did not prefer user preset: got " << resolved << "\n";
        return false;
    }

    if (!ConfigManager::deleteUserPreset("User Only", userUtf8)) {
        std::cerr << "deleteUserPreset failed\n";
        return false;
    }
    if (fs::exists(userDir / "user_only.toml", ec)) {
        std::cerr << "Deleted user preset still exists\n";
        return false;
    }
    return true;
}

bool testBundledStarterPresets() {
#ifdef PLASMOID_SOURCE_DIR
    const fs::path bundledDir = fs::path(PLASMOID_SOURCE_DIR) / "assets" / "presets";
    const auto entries = ConfigManager::listPresets(Utf8Paths::toUtf8(bundledDir), "__nonexistent_user_dir__");
    if (entries.size() < 4) {
        std::cerr << "Expected at least 4 bundled starter presets in assets/presets, found " << entries.size() << "\n";
        return false;
    }
    for (const auto& entry : entries) {
        PersistentConfig cfg;
        if (!ConfigManager::loadPreset(entry.path, cfg) || cfg.layers.empty()) {
            std::cerr << "Bundled starter preset failed to load or had zero layers: " << entry.path << "\n";
            return false;
        }
    }
#endif
    return true;
}
} // namespace

int main() {
    const fs::path dir = makeTempDir("plasmoid_preset_tests");
    bool ok = true;
    ok &= testSanitizePresetFilename();
    ok &= testPresetRoundTripAndIsolation(dir);
    ok &= testDiscoveryResolutionAndDelete(dir);
    ok &= testBundledStarterPresets();
    std::error_code ec;
    fs::remove_all(dir, ec);
    return ok ? 0 : 1;
}
