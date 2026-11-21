#include "ConfigManager.hpp"
#include "Utf8Paths.hpp"
#include "toml.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

namespace {
template <typename EnumT>
EnumT readClampedEnum(const toml::table& table, const char* key, int minVal, int maxVal, int defaultVal = 0) {
    const int raw = table[key].value_or(defaultVal);
    return static_cast<EnumT>(std::clamp(raw, minVal, maxVal));
}

toml::table writeXYSettings(const XYLayerSettings& xy) {
    return toml::table{
        {"profile", static_cast<int>(xy.profile)}, {"persistence", xy.persistence},
        {"window_scale", xy.windowScale}, {"coupling_x", static_cast<int>(xy.couplingX)},
        {"coupling_y", static_cast<int>(xy.couplingY)}, {"ac_cutoff_hz", xy.acCutoffHz},
        {"bandwidth_hz", xy.bandwidthHz}, {"gain_x", xy.gainX}, {"gain_y", xy.gainY},
        {"position_x", xy.positionX}, {"position_y", xy.positionY},
        {"invert_x", xy.invertX}, {"invert_y", xy.invertY}, {"rotation", xy.rotationDegrees},
        {"auto_gain", xy.autoGain}, {"trigger_mode", static_cast<int>(xy.triggerMode)},
        {"trigger_source", static_cast<int>(xy.triggerSource)}, {"trigger_edge", static_cast<int>(xy.triggerEdge)},
        {"trigger_level", xy.triggerLevel}, {"trigger_hysteresis", xy.triggerHysteresis},
        {"trigger_holdoff_ms", xy.triggerHoldoffMs}, {"jump_blanking", xy.jumpBlanking},
        {"trace_width", xy.traceWidth}, {"bloom", xy.bloom}, {"beam_head_size", xy.beamHeadSize},
        {"beam_intensity", xy.beamIntensity}, {"dwell_effect", xy.dwellEffect},
        {"density_effect", xy.densityEffect}, {"z_mode", static_cast<int>(xy.zMode)},
        {"z_gain", xy.zGain}, {"z_offset", xy.zOffset}
    };
}

void readXYSettings(const toml::table& table, XYLayerSettings& xy) {
    xy.profile = readClampedEnum<ScopeProfile>(table, "profile", 0, 2);
    xy.persistence = table["persistence"].value_or(true);
    xy.windowScale = std::clamp(table["window_scale"].value_or(1.0f), 0.1f, 8.0f);
    xy.couplingX = readClampedEnum<CouplingMode>(table, "coupling_x", 0, 1);
    xy.couplingY = readClampedEnum<CouplingMode>(table, "coupling_y", 0, 1);
    xy.acCutoffHz = std::clamp(table["ac_cutoff_hz"].value_or(5.0f), 0.1f, 1000.0f);
    xy.bandwidthHz = std::clamp(table["bandwidth_hz"].value_or(20000.0f), 10.0f, 96000.0f);
    xy.gainX = std::clamp(table["gain_x"].value_or(1.0f), 0.01f, 100.0f);
    xy.gainY = std::clamp(table["gain_y"].value_or(1.0f), 0.01f, 100.0f);
    xy.positionX = std::clamp(table["position_x"].value_or(0.0f), -2.0f, 2.0f);
    xy.positionY = std::clamp(table["position_y"].value_or(0.0f), -2.0f, 2.0f);
    xy.invertX = table["invert_x"].value_or(false);
    xy.invertY = table["invert_y"].value_or(false);
    xy.rotationDegrees = std::clamp(table["rotation"].value_or(0.0f), -360.0f, 360.0f);
    xy.autoGain = table["auto_gain"].value_or(false);
    xy.triggerMode = readClampedEnum<TriggerMode>(table, "trigger_mode", 0, 1);
    xy.triggerSource = readClampedEnum<TriggerSource>(table, "trigger_source", 0, 1);
    xy.triggerEdge = readClampedEnum<TriggerEdge>(table, "trigger_edge", 0, 1);
    xy.triggerLevel = std::clamp(table["trigger_level"].value_or(0.0f), -2.0f, 2.0f);
    xy.triggerHysteresis = std::clamp(table["trigger_hysteresis"].value_or(0.02f), 0.0f, 1.0f);
    xy.triggerHoldoffMs = std::clamp(table["trigger_holdoff_ms"].value_or(0.0f), 0.0f, 1000.0f);
    xy.jumpBlanking = std::clamp(table["jump_blanking"].value_or(0.35f), 0.01f, 2.0f);
    xy.traceWidth = std::clamp(table["trace_width"].value_or(2.0f), 0.5f, 20.0f);
    xy.bloom = std::clamp(table["bloom"].value_or(1.0f), 0.0f, 5.0f);
    xy.beamHeadSize = std::clamp(table["beam_head_size"].value_or(0.0f), 0.0f, 50.0f);
    xy.beamIntensity = std::clamp(table["beam_intensity"].value_or(1.0f), 0.0f, 10.0f);
    xy.dwellEffect = std::clamp(table["dwell_effect"].value_or(0.0f), 0.0f, 1.0f);
    xy.densityEffect = std::clamp(table["density_effect"].value_or(0.0f), 0.0f, 1.0f);
    xy.zMode = readClampedEnum<ZIntensityMode>(table, "z_mode", 0, 2);
    xy.zGain = std::clamp(table["z_gain"].value_or(1.0f), 0.0f, 10.0f);
    xy.zOffset = std::clamp(table["z_offset"].value_or(0.0f), -2.0f, 2.0f);
}

toml::table writeOscilloscopeTable(const OscilloscopeDisplaySettings& scope) {
    return toml::table{
        {"profile", static_cast<int>(scope.profile)},
        {"graticule_enabled", scope.graticuleEnabled},
        {"graticule_opacity", scope.graticuleOpacity},
        {"graticule_columns", scope.graticuleColumns},
        {"graticule_rows", scope.graticuleRows},
        {"fast_decay_ms", scope.phosphorFastDecayMs},
        {"slow_decay_ms", scope.phosphorSlowDecayMs},
        {"slow_weight", scope.phosphorSlowWeight},
        {"saturation", scope.phosphorSaturation},
        {"decay_color_shift", scope.decayColorShift},
        {"measurement_overlay", scope.measurementOverlay},
        {"overlay_in_video", scope.overlayInVideo}
    };
}

void readOscilloscopeTable(const toml::table& scope, OscilloscopeDisplaySettings& dst) {
    dst.profile = readClampedEnum<ScopeProfile>(scope, "profile", 0, 2);
    dst.graticuleEnabled = scope["graticule_enabled"].value_or(true);
    dst.graticuleOpacity = std::clamp(scope["graticule_opacity"].value_or(0.18f), 0.0f, 1.0f);
    dst.graticuleColumns = std::clamp(scope["graticule_columns"].value_or(10), 1, 64);
    dst.graticuleRows = std::clamp(scope["graticule_rows"].value_or(8), 1, 64);
    dst.phosphorFastDecayMs = std::clamp(scope["fast_decay_ms"].value_or(65.0f), 1.0f, 30000.0f);
    dst.phosphorSlowDecayMs = std::clamp(scope["slow_decay_ms"].value_or(500.0f), 1.0f, 30000.0f);
    dst.phosphorSlowWeight = std::clamp(scope["slow_weight"].value_or(0.25f), 0.0f, 2.0f);
    dst.phosphorSaturation = scope["saturation"].value_or(1.0f);
    dst.decayColorShift = scope["decay_color_shift"].value_or(0.08f);
    dst.measurementOverlay = scope["measurement_overlay"].value_or(false);
    dst.overlayInVideo = scope["overlay_in_video"].value_or(false);
}

toml::table writeOverlayVisualTable(const MediaOverlayLayer& overlay) {
    return toml::table{
        {"enabled", overlay.enabled},
        {"background_fit", static_cast<int>(overlay.backgroundFit)},
        {"background_opacity", overlay.backgroundOpacity},
        {"background_dimming", overlay.backgroundDimming},
        {"background_scale", overlay.backgroundScale},
        {"background_offset_x", overlay.backgroundOffsetX},
        {"background_offset_y", overlay.backgroundOffsetY},
        {"top_spectrum", overlay.style.showTopSpectrum},
        {"bottom_spectrum", overlay.style.showBottomSpectrum},
        {"opacity", overlay.style.opacity},
        {"spectrum_gain", overlay.style.spectrumGain},
        {"spectrum_amplitude_cap", overlay.style.spectrumAmplitudeCap},
        {"spectrum_bar_count", overlay.style.spectrumBarCount},
        {"spectrum_bar_width", overlay.style.spectrumBarWidth},
        {"spectrum_line_thickness", overlay.style.spectrumLineThickness},
        {"top_spectrum_height", overlay.style.topSpectrumHeight},
        {"bottom_spectrum_height", overlay.style.bottomSpectrumHeight},
        {"top_margin", overlay.style.topMargin},
        {"bottom_margin", overlay.style.bottomMargin},
        {"artist_text_scale", overlay.style.artistTextScale},
        {"title_text_scale", overlay.style.titleTextScale},
        {"timestamp_text_scale", overlay.style.timestampTextScale},
        {"lyrics_text_scale", overlay.style.lyricsTextScale},
        {"blurred_band", overlay.style.blurredLyricsBand},
        {"band_height", overlay.style.lyricsBandHeight},
        {"band_opacity", overlay.style.lyricsBandOpacity}
    };
}

void readOverlayVisualTable(const toml::table& overlay, MediaOverlayLayer& dst) {
    dst.enabled = overlay["enabled"].value_or(true);
    dst.backgroundFit = readClampedEnum<OverlayBackgroundFit>(overlay, "background_fit", 0, 2);
    dst.backgroundOpacity = std::clamp(overlay["background_opacity"].value_or(1.0f), 0.0f, 1.0f);
    dst.backgroundDimming = std::clamp(overlay["background_dimming"].value_or(0.15f), 0.0f, 1.0f);
    dst.backgroundScale = std::clamp(overlay["background_scale"].value_or(1.0f), 0.1f, 4.0f);
    dst.backgroundOffsetX = std::clamp(overlay["background_offset_x"].value_or(0.0f), -2.0f, 2.0f);
    dst.backgroundOffsetY = std::clamp(overlay["background_offset_y"].value_or(0.0f), -2.0f, 2.0f);
    dst.style.showTopSpectrum = overlay["top_spectrum"].value_or(true);
    dst.style.showBottomSpectrum = overlay["bottom_spectrum"].value_or(true);
    dst.style.opacity = std::clamp(overlay["opacity"].value_or(1.0f), 0.0f, 1.0f);
    dst.style.spectrumGain = std::clamp(overlay["spectrum_gain"].value_or(1.35f), 0.01f, 10.0f);
    dst.style.spectrumAmplitudeCap = std::clamp(overlay["spectrum_amplitude_cap"].value_or(1.0f), 0.05f, 8.0f);
    dst.style.spectrumBarCount = std::clamp(overlay["spectrum_bar_count"].value_or(96), 8, 512);
    dst.style.spectrumBarWidth = std::clamp(overlay["spectrum_bar_width"].value_or(0.78f), 0.05f, 1.0f);
    dst.style.spectrumLineThickness = std::clamp(overlay["spectrum_line_thickness"].value_or(0.0022f), 0.0005f, 0.02f);
    dst.style.topSpectrumHeight = std::clamp(overlay["top_spectrum_height"].value_or(0.027f), 0.005f, 0.5f);
    dst.style.bottomSpectrumHeight = std::clamp(overlay["bottom_spectrum_height"].value_or(0.035f), 0.005f, 0.5f);
    dst.style.topMargin = std::clamp(overlay["top_margin"].value_or(0.06f), 0.0f, 0.5f);
    dst.style.bottomMargin = std::clamp(overlay["bottom_margin"].value_or(0.07f), 0.0f, 0.5f);
    dst.style.artistTextScale = std::clamp(overlay["artist_text_scale"].value_or(0.62f), 0.1f, 5.0f);
    dst.style.titleTextScale = std::clamp(overlay["title_text_scale"].value_or(0.76f), 0.1f, 5.0f);
    dst.style.timestampTextScale = std::clamp(overlay["timestamp_text_scale"].value_or(0.74f), 0.1f, 5.0f);
    dst.style.lyricsTextScale = std::clamp(overlay["lyrics_text_scale"].value_or(1.18f), 0.1f, 5.0f);
    dst.style.blurredLyricsBand = overlay["blurred_band"].value_or(true);
    dst.style.lyricsBandHeight = std::clamp(overlay["band_height"].value_or(0.115f), 0.02f, 0.45f);
    dst.style.lyricsBandOpacity = std::clamp(overlay["band_opacity"].value_or(0.72f), 0.0f, 1.0f);
}

toml::array writeLayersArray(const std::vector<VisualizerLayer>& layers) {
    toml::array layersArray;
    for (const auto& layer : layers) {
        const VisualizerShape savedShape = layer.shape == VisualizerShape::OscilloscopeXY_Clean
            ? VisualizerShape::OscilloscopeXY
            : layer.shape;
        toml::table layerTable{
            {"layer_id", static_cast<int64_t>(layer.id)},
            {"name", layer.name},
            {"gain", layer.config.gain},
            {"falloff", layer.config.falloff},
            {"min_freq", layer.config.minFreq},
            {"max_freq", layer.config.maxFreq},
            {"num_bars", static_cast<int64_t>(layer.config.numBars)},
            {"attack", layer.config.attack},
            {"smoothing", layer.config.smoothing},
            {"spectrum_power", layer.config.spectrumPower},
            {"shape", static_cast<int>(savedShape)},
            {"color", toml::array{layer.color[0], layer.color[1], layer.color[2], layer.color[3]}},
            {"bar_height", layer.barHeight},
            {"mirrored", layer.mirrored},
            {"corner_radius", layer.cornerRadius},
            {"visible", layer.visible},
            {"trace_width", layer.traceWidth},
            {"fill_opacity", layer.fillOpacity},
            {"audio_channel", static_cast<int>(layer.channel)},
            {"bar_anchor", static_cast<int>(layer.barAnchor)}
        };
        layerTable.insert_or_assign("xy", writeXYSettings(layer.xy));
        layersArray.push_back(std::move(layerTable));
    }
    return layersArray;
}

bool readLayersArray(const toml::array& layers, PersistentConfig& config, bool& anyLegacyGrid) {
    std::vector<VisualizerLayer> loadedLayers;
    for (const auto& item : layers) {
        if (auto layerTbl = item.as_table()) {
            VisualizerLayer l;
            const int64_t rawLayerId = (*layerTbl)["layer_id"].value_or(int64_t{0});
            l.id = rawLayerId > 0 ? static_cast<LayerId>(rawLayerId) : config.allocateLayerId();
            config.nextLayerId = std::max(config.nextLayerId, l.id + 1);
            l.name = (*layerTbl)["name"].value_or(std::string("Unnamed Layer"));
            l.config.gain = std::clamp((*layerTbl)["gain"].value_or(1.0f), 0.01f, 500.0f);
            l.config.falloff = std::clamp((*layerTbl)["falloff"].value_or(0.9f), 0.0f, 0.9999f);
            l.config.minFreq = std::clamp((*layerTbl)["min_freq"].value_or(20.0f), 1.0f, 20000.0f);
            l.config.maxFreq = std::clamp((*layerTbl)["max_freq"].value_or(20000.0f), l.config.minFreq + 1.0f, 96000.0f);
            const int64_t rawNumBars = (*layerTbl)["num_bars"].value_or(int64_t{256});
            l.config.numBars = static_cast<size_t>(std::clamp<int64_t>(rawNumBars, 1, 4096));
            l.config.attack = std::clamp((*layerTbl)["attack"].value_or(0.8f), 0.0f, 1.0f);
            l.config.smoothing = std::clamp((*layerTbl)["smoothing"].value_or(1), 0, 32);
            l.config.spectrumPower = std::clamp((*layerTbl)["spectrum_power"].value_or(1.0f), 0.05f, 5.0f);

            const int rawShape = std::clamp(
                (*layerTbl)["shape"].value_or(0),
                0, static_cast<int>(VisualizerShape::OscilloscopeXY_Clean));
            l.shape = static_cast<VisualizerShape>(rawShape);
            if (l.shape == VisualizerShape::OscilloscopeXY_Clean) {
                l.shape = VisualizerShape::OscilloscopeXY;
            }

            if (auto lColor = (*layerTbl)["color"].as_array()) {
                for (int i = 0; i < 4 && i < static_cast<int>(lColor->size()); ++i) {
                    l.color[i] = (*lColor)[i].value_or(1.0f);
                }
            }

            l.barHeight = std::clamp((*layerTbl)["bar_height"].value_or(0.2f), 0.01f, 5.0f);
            l.mirrored = (*layerTbl)["mirrored"].value_or(false);
            l.cornerRadius = std::clamp((*layerTbl)["corner_radius"].value_or(0.0f), 0.0f, 1.0f);
            l.visible = (*layerTbl)["visible"].value_or(true);
            l.traceWidth = std::clamp((*layerTbl)["trace_width"].value_or(2.0f), 0.5f, 20.0f);
            l.fillOpacity = std::clamp((*layerTbl)["fill_opacity"].value_or(0.0f), 0.0f, 1.0f);
            l.channel = readClampedEnum<AudioChannel>(*layerTbl, "audio_channel", 0, 2);
            l.barAnchor = readClampedEnum<BarAnchor>(*layerTbl, "bar_anchor", 0, 4);

            if (auto xy = (*layerTbl)["xy"].as_table()) {
                readXYSettings(*xy, l.xy);
            } else {
                l.xy.profile = ScopeProfile::Custom;
                l.xy.persistence = (*layerTbl)["use_layer_persistence"].value_or(true);
                l.xy.windowScale = std::clamp((*layerTbl)["time_scale"].value_or(1.0f), 0.1f, 8.0f);
                l.xy.gainX = std::clamp((*layerTbl)["x_scale"].value_or(1.0f), 0.01f, 100.0f);
                l.xy.gainY = std::clamp((*layerTbl)["y_scale"].value_or(1.0f), 0.01f, 100.0f);
                l.xy.positionX = std::clamp((*layerTbl)["x_offset"].value_or(0.0f), -2.0f, 2.0f);
                l.xy.positionY = std::clamp((*layerTbl)["y_offset"].value_or(0.0f), -2.0f, 2.0f);
                l.xy.invertX = (*layerTbl)["flip_x"].value_or(false);
                l.xy.invertY = (*layerTbl)["flip_y"].value_or(false);
                l.xy.rotationDegrees = std::clamp((*layerTbl)["rotation"].value_or(0.0f), -360.0f, 360.0f);
                l.xy.autoGain = (*layerTbl)["xy_auto_gain"].value_or(false);
                l.xy.traceWidth = l.traceWidth;
                l.xy.bloom = std::clamp((*layerTbl)["bloom"].value_or(1.0f), 0.0f, 5.0f);
                l.xy.beamHeadSize = std::clamp((*layerTbl)["beam_head_size"].value_or(0.0f), 0.0f, 50.0f);
                l.xy.dwellEffect = std::clamp((*layerTbl)["velocity_modulation"].value_or(0.0f), 0.0f, 1.0f);
            }
            anyLegacyGrid |= (*layerTbl)["show_grid"].value_or(true);
            loadedLayers.push_back(std::move(l));
        }
    }
    if (!loadedLayers.empty()) {
        config.layers = std::move(loadedLayers);
        return true;
    }
    return false;
}

bool writeAtomicToml(const std::string& fullPath, const toml::table& tbl) {
    const fs::path p = Utf8Paths::fromUtf8(fullPath);

    std::error_code ec;
    if (p.has_parent_path() && !fs::exists(p.parent_path(), ec)) {
        fs::create_directories(p.parent_path(), ec);
        if (ec) {
            std::cerr << "Failed to create directory for " << fullPath
                      << ": " << ec.message() << std::endl;
            return false;
        }
    }

    fs::path tmpPath = p;
    tmpPath += ".tmp";
    {
        std::ofstream file(tmpPath, std::ios::binary | std::ios::trunc);
        if (!file) {
            std::cerr << "Failed to open temporary file for writing: "
                      << Utf8Paths::toUtf8(tmpPath) << std::endl;
            return false;
        }
        file << tbl;
        file.flush();
        if (!file.good()) {
            std::cerr << "Failed to flush temporary file: "
                      << Utf8Paths::toUtf8(tmpPath) << std::endl;
            return false;
        }
    }

    fs::rename(tmpPath, p, ec);
    if (ec) {
        std::error_code rmEc;
        fs::remove(p, rmEc);
        ec.clear();
        fs::rename(tmpPath, p, ec);
        if (ec) {
            std::cerr << "Failed to replace file " << fullPath
                      << ": " << ec.message() << std::endl;
            fs::remove(tmpPath, rmEc);
            return false;
        }
    }
    return true;
}

toml::table parseTomlFile(const fs::path& filePath, const std::string& utf8Path) {
#ifdef _WIN32
    (void)utf8Path;
    return toml::parse_file(filePath.wstring());
#else
    (void)filePath;
    return toml::parse_file(utf8Path);
#endif
}

std::string lowercaseCopy(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return s;
}

std::string readPresetDisplayName(const fs::path& filePath, const std::string& utf8Path) {
    try {
        const toml::table tbl = parseTomlFile(filePath, utf8Path);
        if (auto meta = tbl["preset"].as_table()) {
            const std::string name = (*meta)["name"].value_or(std::string{});
            if (!name.empty()) return name;
        }
    } catch (const std::exception&) {
        // Fall back to filename stem if metadata cannot be read.
    }
    return Utf8Paths::toUtf8(filePath.stem());
}
} // namespace

bool ConfigManager::save(const std::string& filename, const PersistentConfig& config) {
    const std::string fullPath = filename.empty() ? getConfigPath() : filename;

    toml::table tbl;
    tbl.insert_or_assign("application", toml::table{
        {"music_folder", config.musicFolderPath},
        {"beat_sensitivity", config.beatSensitivity},
        {"global_gain", config.globalGain},
        {"enable_vsync", config.enableVsync},
        {"target_fps", config.targetFps},
        {"audio_mode", static_cast<int>(config.currentAudioMode)},
        {"loop_track", config.loopTrack},
        {"capture_device_name", config.selectedCaptureDeviceName},
        {"use_specific_capture_device", config.useSpecificCaptureDevice},
        {"vid_width", config.videoSettings.width},
        {"vid_height", config.videoSettings.height},
        {"vid_fps", config.videoSettings.fps},
        {"vid_codec_idx", config.videoSettings.codecIdx},
        {"vid_crf", config.videoSettings.crf},
        {"vid_output_path", config.videoSettings.outputPath},
        {"zen_kun_enabled", config.zenKunModeEnabled},
        {"bg_path", config.backgroundImagePath},
        {"bg_pulse", config.bgPulseIntensity},
        {"bg_shake", config.shakeIntensity},
        {"bg_shake_tilt", config.shakeTiltIntensity},
        {"bg_shake_zoom", config.shakeZoomIntensity}
    });

    tbl.insert_or_assign("oscilloscope", writeOscilloscopeTable(config.oscilloscopeDisplay));

    const auto& overlay = config.mediaOverlay;
    toml::table overlayTbl = writeOverlayVisualTable(overlay);
    overlayTbl.insert_or_assign("background_type", static_cast<int>(overlay.backgroundType));
    overlayTbl.insert_or_assign("background_path", overlay.backgroundPath);
    overlayTbl.insert_or_assign("video_decoder", static_cast<int>(overlay.videoDecoder));
    overlayTbl.insert_or_assign("video_fps", overlay.videoFps);
    overlayTbl.insert_or_assign("font_path", overlay.fontPath);
    overlayTbl.insert_or_assign("lyrics_font_path", overlay.lyricsFontPath);
    overlayTbl.insert_or_assign("artist", overlay.artist);
    overlayTbl.insert_or_assign("title", overlay.title);
    overlayTbl.insert_or_assign("lyric_source", overlay.lyricSource);
    overlayTbl.insert_or_assign("lyric_file_path", overlay.lyricFilePath);
    tbl.insert_or_assign("media_overlay", std::move(overlayTbl));

    tbl.insert_or_assign("layers", writeLayersArray(config.layers));
    return writeAtomicToml(fullPath, tbl);
}

bool ConfigManager::load(const std::string& filename, PersistentConfig& config) {
    const std::string fullPath = filename.empty() ? getConfigPath() : filename;
    const fs::path configPath = Utf8Paths::fromUtf8(fullPath);

    std::error_code ec;
    if (!fs::exists(configPath, ec) || ec) return false;

    try {
        const toml::table tbl = parseTomlFile(configPath, fullPath);

        float legacyPhosphorDecay = 0.1f;
        if (auto app = tbl["application"].as_table()) {
            config.musicFolderPath = (*app)["music_folder"].value_or(std::string{});
            config.beatSensitivity = std::clamp((*app)["beat_sensitivity"].value_or(1.3f), 0.1f, 10.0f);
            legacyPhosphorDecay = (*app)["phosphor_decay"].value_or(0.1f);
            config.globalGain = std::clamp((*app)["global_gain"].value_or(1.0f), 0.01f, 100.0f);
            config.enableVsync = (*app)["enable_vsync"].value_or(true);
            config.targetFps = std::clamp((*app)["target_fps"].value_or(60), 0, 1000);
            config.currentAudioMode = readClampedEnum<AudioMode>(*app, "audio_mode", 0, 3);
            config.loopTrack = (*app)["loop_track"].value_or(true);
            config.selectedCaptureDeviceName = (*app)["capture_device_name"].value_or(std::string{});
            config.useSpecificCaptureDevice = (*app)["use_specific_capture_device"].value_or(false);

            config.videoSettings.width = std::clamp((*app)["vid_width"].value_or(1920), 64, 7680);
            config.videoSettings.height = std::clamp((*app)["vid_height"].value_or(1080), 64, 4320);
            config.videoSettings.fps = std::clamp((*app)["vid_fps"].value_or(60), 1, 240);
            config.videoSettings.codecIdx = std::clamp((*app)["vid_codec_idx"].value_or(0), 0, 3);
            config.videoSettings.crf = std::clamp((*app)["vid_crf"].value_or(16), 0, 51);
            config.videoSettings.outputPath = (*app)["vid_output_path"].value_or(std::string("output.mp4"));

            config.zenKunModeEnabled = (*app)["zen_kun_enabled"].value_or(false);
            config.backgroundImagePath = (*app)["bg_path"].value_or(std::string{});
            config.bgPulseIntensity = std::clamp((*app)["bg_pulse"].value_or(0.05f), 0.0f, 2.0f);
            config.shakeIntensity = std::clamp((*app)["bg_shake"].value_or(0.02f), 0.0f, 2.0f);
            config.shakeTiltIntensity = std::clamp((*app)["bg_shake_tilt"].value_or(0.05f), 0.0f, 2.0f);
            config.shakeZoomIntensity = std::clamp((*app)["bg_shake_zoom"].value_or(0.02f), 0.0f, 2.0f);
        }

        const bool hasOscilloscopeTable = tbl["oscilloscope"].is_table();
        if (auto scope = tbl["oscilloscope"].as_table()) {
            readOscilloscopeTable(*scope, config.oscilloscopeDisplay);
        }

        if (auto overlay = tbl["media_overlay"].as_table()) {
            auto& dst = config.mediaOverlay;
            readOverlayVisualTable(*overlay, dst);
            dst.backgroundType = readClampedEnum<OverlayBackgroundType>(*overlay, "background_type", 0, 3);
            dst.backgroundPath = (*overlay)["background_path"].value_or(std::string{});
            dst.videoDecoder = readClampedEnum<OverlayVideoDecoder>(*overlay, "video_decoder", 0, 2);
            dst.videoFps = std::clamp((*overlay)["video_fps"].value_or(30), 1, 60);
            dst.fontPath = (*overlay)["font_path"].value_or(std::string{});
            dst.lyricsFontPath = (*overlay)["lyrics_font_path"].value_or(std::string{});
            dst.artist = (*overlay)["artist"].value_or(dst.artist);
            dst.title = (*overlay)["title"].value_or(dst.title);
            const std::string loadedLyrics = (*overlay)["lyric_source"].value_or(std::string{});
            if (!loadedLyrics.empty()) {
                dst.lyricSource = loadedLyrics;
                dst.lyrics.parseLrc(dst.lyricSource);
            }
            dst.lyricFilePath = (*overlay)["lyric_file_path"].value_or(std::string("lyrics.lrc"));
        }

        bool anyLegacyGrid = false;
        if (auto layers = tbl["layers"].as_array()) {
            readLayersArray(*layers, config, anyLegacyGrid);
        }

        if (!hasOscilloscopeTable) {
            const float retained = std::clamp(1.0f - legacyPhosphorDecay, 0.001f, 0.9999f);
            const float slowMs = -1000.0f / (60.0f * std::log(retained));
            config.oscilloscopeDisplay.profile = ScopeProfile::Custom;
            config.oscilloscopeDisplay.phosphorSlowDecayMs = std::clamp(slowMs, 10.0f, 30000.0f);
            config.oscilloscopeDisplay.phosphorFastDecayMs = config.oscilloscopeDisplay.phosphorSlowDecayMs * 0.15f;
            config.oscilloscopeDisplay.phosphorSlowWeight = 0.25f;
            config.oscilloscopeDisplay.graticuleEnabled = anyLegacyGrid;
        }
        return true;
    } catch (const toml::parse_error& err) {
        std::cerr << "Config TOML parse error in " << fullPath << ":\n" << err << "\n";
        return false;
    } catch (const std::exception& err) {
        std::cerr << "Failed to load config " << fullPath << ": " << err.what() << "\n";
        return false;
    }
}

bool ConfigManager::savePreset(
    const std::string& filename,
    const PersistentConfig& config,
    const std::string& presetName
) {
    if (filename.empty()) return false;
    const fs::path p = Utf8Paths::fromUtf8(filename);
    const std::string effectiveName = !presetName.empty()
        ? presetName
        : Utf8Paths::toUtf8(p.stem());

    toml::table tbl;
    tbl.insert_or_assign("preset", toml::table{
        {"name", effectiveName},
        {"schema_version", int64_t{1}}
    });
    tbl.insert_or_assign("oscilloscope", writeOscilloscopeTable(config.oscilloscopeDisplay));
    tbl.insert_or_assign("zen_kun", toml::table{
        {"enabled", config.zenKunModeEnabled},
        {"beat_sensitivity", config.beatSensitivity},
        {"bg_pulse", config.bgPulseIntensity},
        {"bg_shake", config.shakeIntensity},
        {"bg_shake_tilt", config.shakeTiltIntensity},
        {"bg_shake_zoom", config.shakeZoomIntensity}
    });
    tbl.insert_or_assign("media_overlay", writeOverlayVisualTable(config.mediaOverlay));
    tbl.insert_or_assign("layers", writeLayersArray(config.layers));

    return writeAtomicToml(filename, tbl);
}

bool ConfigManager::loadPreset(const std::string& filename, PersistentConfig& config) {
    if (filename.empty()) return false;
    const fs::path presetPath = Utf8Paths::fromUtf8(filename);

    std::error_code ec;
    if (!fs::exists(presetPath, ec) || ec) return false;

    try {
        const toml::table tbl = parseTomlFile(presetPath, filename);

        if (auto scope = tbl["oscilloscope"].as_table()) {
            readOscilloscopeTable(*scope, config.oscilloscopeDisplay);
        }
        if (auto zen = tbl["zen_kun"].as_table()) {
            config.zenKunModeEnabled = (*zen)["enabled"].value_or(config.zenKunModeEnabled);
            config.beatSensitivity = std::clamp((*zen)["beat_sensitivity"].value_or(config.beatSensitivity), 0.1f, 10.0f);
            config.bgPulseIntensity = std::clamp((*zen)["bg_pulse"].value_or(config.bgPulseIntensity), 0.0f, 2.0f);
            config.shakeIntensity = std::clamp((*zen)["bg_shake"].value_or(config.shakeIntensity), 0.0f, 2.0f);
            config.shakeTiltIntensity = std::clamp((*zen)["bg_shake_tilt"].value_or(config.shakeTiltIntensity), 0.0f, 2.0f);
            config.shakeZoomIntensity = std::clamp((*zen)["bg_shake_zoom"].value_or(config.shakeZoomIntensity), 0.0f, 2.0f);
        }
        if (auto overlay = tbl["media_overlay"].as_table()) {
            readOverlayVisualTable(*overlay, config.mediaOverlay);
        }

        bool anyLegacyGrid = false;
        if (auto layers = tbl["layers"].as_array()) {
            readLayersArray(*layers, config, anyLegacyGrid);
        }
        return true;
    } catch (const toml::parse_error& err) {
        std::cerr << "Preset TOML parse error in " << filename << ":\n" << err << "\n";
        return false;
    } catch (const std::exception& err) {
        std::cerr << "Failed to load preset " << filename << ": " << err.what() << "\n";
        return false;
    }
}

std::string ConfigManager::sanitizePresetFilename(const std::string& name) {
    std::string out;
    out.reserve(name.size());
    bool lastWasSeparator = false;
    for (unsigned char ch : name) {
        if (ch < 32 || ch == '.' || ch == '/' || ch == '\\' || ch == ':' || ch == '*' ||
            ch == '?' || ch == '"' || ch == '<' || ch == '>' || ch == '|') {
            continue;
        }
        if (std::isspace(ch) || ch == '-' || ch == '_') {
            if (!out.empty() && !lastWasSeparator) {
                out.push_back('_');
                lastWasSeparator = true;
            }
            continue;
        }
        out.push_back(static_cast<char>(ch));
        lastWasSeparator = false;
    }
    while (!out.empty() && (out.back() == '_' || out.back() == '.')) {
        out.pop_back();
    }
    if (out.empty()) return "preset";
    if (out.size() > 64) out.resize(64);
    return out;
}

std::vector<PresetEntry> ConfigManager::listPresets(
    const std::string& builtInDir,
    const std::string& userDir
) {
    std::vector<PresetEntry> entries;
    auto scanDir = [&](const std::string& dirUtf8, bool builtIn) {
        if (dirUtf8.empty()) return;
        const fs::path dir = Utf8Paths::fromUtf8(dirUtf8);
        std::error_code ec;
        if (!fs::exists(dir, ec) || ec || !fs::is_directory(dir, ec) || ec) return;
        for (const auto& item : fs::directory_iterator(
                 dir, fs::directory_options::skip_permission_denied, ec)) {
            if (ec) break;
            if (!item.is_regular_file(ec) || ec) continue;
            const std::string ext = lowercaseCopy(Utf8Paths::toUtf8(item.path().extension()));
            if (ext != ".toml") continue;
            const std::string utf8Path = Utf8Paths::toUtf8(item.path());
            PresetEntry entry;
            entry.name = readPresetDisplayName(item.path(), utf8Path);
            entry.path = utf8Path;
            entry.builtIn = builtIn;
            entries.push_back(std::move(entry));
        }
    };

    scanDir(builtInDir, true);
    scanDir(userDir.empty() ? getPresetsDirectory() : userDir, false);

    std::sort(entries.begin(), entries.end(), [](const PresetEntry& a, const PresetEntry& b) {
        if (a.builtIn != b.builtIn) return a.builtIn > b.builtIn;
        return lowercaseCopy(a.name) < lowercaseCopy(b.name);
    });
    return entries;
}

std::string ConfigManager::resolvePresetPath(
    const std::string& nameOrPath,
    const std::string& builtInDir,
    const std::string& userDir
) {
    if (nameOrPath.empty()) return {};

    // 1. Direct file path (absolute or relative).
    const fs::path direct = Utf8Paths::fromUtf8(nameOrPath);
    std::error_code ec;
    if (fs::exists(direct, ec) && !ec && fs::is_regular_file(direct, ec) && !ec) {
        return nameOrPath;
    }

    const std::string effectiveUserDir = userDir.empty() ? getPresetsDirectory() : userDir;
    const std::string targetLower = lowercaseCopy(nameOrPath);
    const std::string sanitizedLower = lowercaseCopy(sanitizePresetFilename(nameOrPath));

    auto searchDir = [&](const std::string& dirUtf8) -> std::string {
        if (dirUtf8.empty()) return {};
        const fs::path dir = Utf8Paths::fromUtf8(dirUtf8);
        std::error_code dirEc;
        if (!fs::exists(dir, dirEc) || dirEc || !fs::is_directory(dir, dirEc) || dirEc) return {};

        for (const auto& item : fs::directory_iterator(
                 dir, fs::directory_options::skip_permission_denied, dirEc)) {
            if (dirEc) break;
            if (!item.is_regular_file(dirEc) || dirEc) continue;
            if (lowercaseCopy(Utf8Paths::toUtf8(item.path().extension())) != ".toml") continue;
            const std::string utf8Path = Utf8Paths::toUtf8(item.path());
            const std::string stemLower = lowercaseCopy(Utf8Paths::toUtf8(item.path().stem()));
            const std::string displayLower = lowercaseCopy(readPresetDisplayName(item.path(), utf8Path));
            if (stemLower == targetLower || stemLower == sanitizedLower || displayLower == targetLower) {
                return utf8Path;
            }
        }
        return {};
    };

    // Prefer user presets over built-in presets when names collide.
    if (const std::string foundUser = searchDir(effectiveUserDir); !foundUser.empty()) {
        return foundUser;
    }
    return searchDir(builtInDir);
}

bool ConfigManager::deleteUserPreset(const std::string& presetName, const std::string& userDir) {
    if (presetName.empty()) return false;
    const std::string effectiveUserDir = userDir.empty() ? getPresetsDirectory() : userDir;
    if (effectiveUserDir.empty()) return false;

    const fs::path target =
        Utf8Paths::fromUtf8(effectiveUserDir) /
        Utf8Paths::fromUtf8(sanitizePresetFilename(presetName) + ".toml");
    std::error_code ec;
    if (!fs::exists(target, ec) || ec) return false;
    return fs::remove(target, ec) && !ec;
}

std::string ConfigManager::getConfigDirectory() {
#ifdef _WIN32
    if (const wchar_t* appData = _wgetenv(L"APPDATA")) {
        return Utf8Paths::toUtf8(fs::path(appData) / L"PlasmoidVisualizerStd");
    }
    return {};
#else
    const char* home = std::getenv("HOME");
    if (!home) return {};
    return Utf8Paths::toUtf8(fs::path(home) / ".config" / "PlasmoidVisualizerStd");
#endif
}

std::string ConfigManager::getPresetsDirectory() {
    const std::string dir = getConfigDirectory();
    if (dir.empty()) return "presets";
    return Utf8Paths::toUtf8(Utf8Paths::fromUtf8(dir) / "presets");
}

std::string ConfigManager::getConfigPath() {
    const std::string dir = getConfigDirectory();
    if (dir.empty()) return "config.toml";
    return Utf8Paths::toUtf8(Utf8Paths::fromUtf8(dir) / "config.toml");
}

std::string ConfigManager::getImGuiIniPath() {
    const std::string dir = getConfigDirectory();
    if (dir.empty()) return "imgui.ini";
    const fs::path configDir = Utf8Paths::fromUtf8(dir);
    std::error_code ec;
    fs::create_directories(configDir, ec);
    return Utf8Paths::toUtf8(configDir / "imgui.ini");
}
