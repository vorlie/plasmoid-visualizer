#include "ConfigManager.hpp"
#include "Utf8Paths.hpp"

#include <cmath>
#include <filesystem>
#include <fstream>
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

bool testRoundTrip(const fs::path& dir) {
    PersistentConfig saved;
    saved.musicFolderPath = "D:/Music/\xE3\x83\x84\xE3\x83\xA6";
    saved.beatSensitivity = 1.65f;
    saved.globalGain = 2.5f;
    saved.enableVsync = false;
    saved.targetFps = 144;
    saved.currentAudioMode = AudioMode::Capture;
    saved.loopTrack = false;
    saved.selectedCaptureDeviceName = "Loopback Speakers";
    saved.useSpecificCaptureDevice = true;
    saved.zenKunModeEnabled = true;
    saved.backgroundImagePath = "bg/test.png";
    saved.bgPulseIntensity = 0.12f;
    saved.shakeIntensity = 0.04f;
    saved.shakeTiltIntensity = 0.08f;
    saved.shakeZoomIntensity = 0.03f;

    saved.oscilloscopeDisplay.profile = ScopeProfile::Analog;
    saved.oscilloscopeDisplay.graticuleEnabled = false;
    saved.oscilloscopeDisplay.graticuleColumns = 12;
    saved.oscilloscopeDisplay.graticuleRows = 10;
    saved.oscilloscopeDisplay.phosphorFastDecayMs = 42.0f;
    saved.oscilloscopeDisplay.phosphorSlowDecayMs = 750.0f;
    saved.oscilloscopeDisplay.phosphorSlowWeight = 0.4f;

    saved.mediaOverlay.enabled = true;
    saved.mediaOverlay.artist = "Artist \xE6\x97\xA5\xE6\x9C\xAC";
    saved.mediaOverlay.title = "Track Title";
    saved.mediaOverlay.lyricSource = "[00:01.00]Hello\n[00:04.50]World";
    saved.mediaOverlay.style.spectrumBarCount = 128;
    saved.mediaOverlay.style.lyricsBandOpacity = 0.55f;

    saved.videoSettings.width = 2560;
    saved.videoSettings.height = 1440;
    saved.videoSettings.fps = 120;
    saved.videoSettings.codecIdx = 1;
    saved.videoSettings.crf = 18;
    saved.videoSettings.outputPath = "renders/demo.mp4";

    VisualizerLayer layer;
    layer.id = 7;
    layer.name = "Bass Bars";
    layer.config.gain = 4.2f;
    layer.config.falloff = 0.85f;
    layer.config.minFreq = 30.0f;
    layer.config.maxFreq = 4000.0f;
    layer.config.numBars = 64;
    layer.config.attack = 0.7f;
    layer.config.smoothing = 2;
    layer.config.spectrumPower = 0.6f;
    layer.color[0] = 0.1f; layer.color[1] = 0.5f; layer.color[2] = 0.9f; layer.color[3] = 0.8f;
    layer.barHeight = 0.65f;
    layer.mirrored = true;
    layer.shape = VisualizerShape::Curve;
    layer.cornerRadius = 0.3f;
    layer.visible = true;
    layer.traceWidth = 3.5f;
    layer.fillOpacity = 0.45f;
    layer.channel = AudioChannel::Left;
    layer.barAnchor = BarAnchor::Center;
    layer.xy.persistence = false;
    layer.xy.bloom = 1.75f;
    layer.xy.gainX = 1.5f;
    layer.xy.rotationDegrees = 45.0f;
    layer.xy.oversampling = 4;
    saved.layers.push_back(layer);

    const std::string path = Utf8Paths::toUtf8(dir / "roundtrip.toml");
    if (!ConfigManager::save(path, saved)) {
        std::cerr << "ConfigManager::save failed\n";
        return false;
    }

    PersistentConfig loaded;
    if (!ConfigManager::load(path, loaded)) {
        std::cerr << "ConfigManager::load failed\n";
        return false;
    }

    if (loaded.musicFolderPath != saved.musicFolderPath ||
        !near(loaded.globalGain, 2.5f) ||
        loaded.enableVsync != false ||
        loaded.targetFps != 144 ||
        loaded.currentAudioMode != AudioMode::Capture ||
        loaded.loopTrack != false ||
        loaded.selectedCaptureDeviceName != "Loopback Speakers" ||
        !loaded.useSpecificCaptureDevice ||
        loaded.mediaOverlay.artist != saved.mediaOverlay.artist ||
        loaded.mediaOverlay.lyrics.events.size() != 2 ||
        loaded.videoSettings.width != 2560 ||
        loaded.videoSettings.outputPath != "renders/demo.mp4" ||
        loaded.layers.size() != 1) {
        std::cerr << "ConfigManager round-trip mismatch in top-level fields\n";
        return false;
    }

    const auto& l = loaded.layers[0];
    if (l.id != 7 || l.name != "Bass Bars" || l.config.numBars != 64 ||
        l.shape != VisualizerShape::Curve || l.channel != AudioChannel::Left ||
        l.barAnchor != BarAnchor::Center || l.xy.persistence != false ||
        !near(l.xy.bloom, 1.75f) || !near(l.xy.rotationDegrees, 45.0f) ||
        l.xy.oversampling != 4) {
        std::cerr << "ConfigManager round-trip mismatch in layer fields\n";
        return false;
    }
    return true;
}

bool testMalformedAndClamping(const fs::path& dir) {
    const fs::path badPath = dir / "malformed.toml";
    {
        std::ofstream out(badPath, std::ios::binary);
        out << "[application\n broken = ???\n";
    }
    PersistentConfig badCfg;
    if (ConfigManager::load(Utf8Paths::toUtf8(badPath), badCfg)) {
        std::cerr << "Malformed TOML should return false\n";
        return false;
    }

    // Out-of-range numeric/enum fields & legacy OscilloscopeXY_Clean (shape = 6) + legacy flat XY keys.
    const fs::path legacyPath = dir / "legacy_clamped.toml";
    {
        std::ofstream out(legacyPath, std::ios::binary);
        out << "[application]\n"
            << "audio_mode = 99\n"
            << "target_fps = -10\n"
            << "global_gain = -5.0\n"
            << "phosphor_decay = 0.05\n"
            << "[[layers]]\n"
            << "name = \"Legacy XY\"\n"
            << "num_bars = -50\n"
            << "shape = 6\n"
            << "audio_channel = 42\n"
            << "bar_anchor = -3\n"
            << "x_scale = 2.5\n"
            << "y_scale = 1.8\n"
            << "bloom = 2.0\n"
            << "use_layer_persistence = false\n"
            << "show_grid = false\n";
    }

    PersistentConfig cfg;
    if (!ConfigManager::load(Utf8Paths::toUtf8(legacyPath), cfg)) {
        std::cerr << "Legacy/clamped TOML failed to load\n";
        return false;
    }
    if (cfg.currentAudioMode != AudioMode::OscMusic ||
        cfg.targetFps != 0 ||
        cfg.globalGain < 0.01f ||
        cfg.oscilloscopeDisplay.graticuleEnabled != false ||
        cfg.layers.size() != 1) {
        std::cerr << "Top-level clamping or legacy phosphor migration failed\n";
        return false;
    }
    const auto& l = cfg.layers[0];
    if (l.config.numBars != 1 ||
        l.shape != VisualizerShape::OscilloscopeXY ||
        l.channel != AudioChannel::Right ||
        l.barAnchor != BarAnchor::Bottom ||
        !near(l.xy.gainX, 2.5f) ||
        !near(l.xy.gainY, 1.8f) ||
        !near(l.xy.bloom, 2.0f) ||
        l.xy.persistence != false) {
        std::cerr << "Layer clamping or legacy XY migration failed\n";
        return false;
    }
    return true;
}
} // namespace

int main() {
    const fs::path dir = makeTempDir("plasmoid_config_tests");
    bool ok = true;
    ok &= testRoundTrip(dir);
    ok &= testMalformedAndClamping(dir);
    std::error_code ec;
    fs::remove_all(dir, ec);
    return ok ? 0 : 1;
}
