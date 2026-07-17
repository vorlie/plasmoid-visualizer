#include "ConfigManager.hpp"
#include "toml.hpp"
#include <fstream>
#include <iostream>
#include <filesystem>
#include <cstdlib>

namespace fs = std::filesystem;

namespace {
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
    xy.profile = static_cast<ScopeProfile>(table["profile"].value_or(0));
    xy.persistence = table["persistence"].value_or(true); xy.windowScale = table["window_scale"].value_or(1.0f);
    xy.couplingX = static_cast<CouplingMode>(table["coupling_x"].value_or(0)); xy.couplingY = static_cast<CouplingMode>(table["coupling_y"].value_or(0));
    xy.acCutoffHz = table["ac_cutoff_hz"].value_or(5.0f); xy.bandwidthHz = table["bandwidth_hz"].value_or(20000.0f);
    xy.gainX = table["gain_x"].value_or(1.0f); xy.gainY = table["gain_y"].value_or(1.0f);
    xy.positionX = table["position_x"].value_or(0.0f); xy.positionY = table["position_y"].value_or(0.0f);
    xy.invertX = table["invert_x"].value_or(false); xy.invertY = table["invert_y"].value_or(false); xy.rotationDegrees = table["rotation"].value_or(0.0f);
    xy.autoGain = table["auto_gain"].value_or(false); xy.triggerMode = static_cast<TriggerMode>(table["trigger_mode"].value_or(0));
    xy.triggerSource = static_cast<TriggerSource>(table["trigger_source"].value_or(0)); xy.triggerEdge = static_cast<TriggerEdge>(table["trigger_edge"].value_or(0));
    xy.triggerLevel = table["trigger_level"].value_or(0.0f); xy.triggerHysteresis = table["trigger_hysteresis"].value_or(0.02f);
    xy.triggerHoldoffMs = table["trigger_holdoff_ms"].value_or(0.0f); xy.jumpBlanking = table["jump_blanking"].value_or(0.35f);
    xy.traceWidth = table["trace_width"].value_or(2.0f); xy.bloom = table["bloom"].value_or(1.0f); xy.beamHeadSize = table["beam_head_size"].value_or(0.0f);
    xy.beamIntensity = table["beam_intensity"].value_or(1.0f); xy.dwellEffect = table["dwell_effect"].value_or(0.0f); xy.densityEffect = table["density_effect"].value_or(0.0f);
    xy.zMode = static_cast<ZIntensityMode>(table["z_mode"].value_or(0)); xy.zGain = table["z_gain"].value_or(1.0f); xy.zOffset = table["z_offset"].value_or(0.0f);
}
}

bool ConfigManager::save(const std::string& filename, const AppConfig& config) {
    std::string fullPath = filename.empty() ? getConfigPath() : filename;
    
    fs::path p(fullPath);
    if (p.has_parent_path() && !fs::exists(p.parent_path())) {
        fs::create_directories(p.parent_path());
    }

    toml::table tbl;

    tbl.insert_or_assign("application", toml::table{
        {"music_folder", config.musicFolder},
        {"particles_enabled", config.particlesEnabled},
        {"beat_sensitivity", config.beatSensitivity},
        {"particle_count", config.particleCount},
        {"particle_speed", config.particleSpeed},
        {"particle_size", config.particleSize},
        {"particle_color", toml::array{config.particleColor[0], config.particleColor[1], config.particleColor[2], config.particleColor[3]}},
        {"phosphor_decay", config.phosphorDecay},
        {"global_gain", config.globalGain},
        {"enable_vsync", config.enableVsync},
        {"target_fps", config.targetFps},
        {"audio_mode", config.audioMode},
        {"capture_device_name", config.captureDeviceName},
        {"use_specific_capture_device", config.useSpecificCaptureDevice},
        {"vid_width", config.vidWidth},
        {"vid_height", config.vidHeight},
        {"vid_fps", config.vidFps},
        {"vid_codec_idx", config.vidCodecIdx},
        {"vid_crf", config.vidCrf},
        {"vid_output_path", config.vidOutputPath},
        {"zen_kun_enabled", config.zenKunEnabled},
        {"bg_path", config.bgPath},
        {"bg_pulse", config.bgPulse},
        {"bg_shake", config.bgShake},
        {"bg_shake_tilt", config.shakeTilt},
        {"bg_shake_zoom", config.shakeZoom},
        {"song_title", config.songTitle},
        {"song_artist", config.artistName},
        {"show_song_info", config.showSongInfo}
    });

    tbl.insert_or_assign("oscilloscope", toml::table{
        {"profile", static_cast<int>(config.oscilloscopeDisplay.profile)}, {"graticule_enabled", config.oscilloscopeDisplay.graticuleEnabled},
        {"graticule_opacity", config.oscilloscopeDisplay.graticuleOpacity}, {"graticule_columns", config.oscilloscopeDisplay.graticuleColumns},
        {"graticule_rows", config.oscilloscopeDisplay.graticuleRows}, {"fast_decay_ms", config.oscilloscopeDisplay.phosphorFastDecayMs},
        {"slow_decay_ms", config.oscilloscopeDisplay.phosphorSlowDecayMs}, {"slow_weight", config.oscilloscopeDisplay.phosphorSlowWeight},
        {"saturation", config.oscilloscopeDisplay.phosphorSaturation}, {"decay_color_shift", config.oscilloscopeDisplay.decayColorShift},
        {"measurement_overlay", config.oscilloscopeDisplay.measurementOverlay}, {"overlay_in_video", config.oscilloscopeDisplay.overlayInVideo}
    });

    toml::array layersArray;
    for (const auto& layer : config.layers) {
        toml::table layerTable{
            {"layer_id", static_cast<int64_t>(layer.layerId)},
            {"name", layer.name},
            {"gain", layer.gain},
            {"falloff", layer.falloff},
            {"min_freq", layer.minFreq},
            {"max_freq", layer.maxFreq},
            {"num_bars", (int64_t)layer.numBars},
            {"attack", layer.attack},
            {"smoothing", layer.smoothing},
            {"spectrum_power", layer.spectrumPower},
            {"shape", layer.shape},
            {"color", toml::array{layer.color[0], layer.color[1], layer.color[2], layer.color[3]}},
            {"bar_height", layer.barHeight},
            {"mirrored", layer.mirrored},
            {"corner_radius", layer.cornerRadius},
            {"visible", layer.visible},
            {"time_scale", layer.timeScale},
            {"rotation", layer.rotation},
            {"flip_x", layer.flipX},
            {"flip_y", layer.flipY},
            {"bloom", layer.bloom},
            {"show_grid", layer.showGrid},
            {"trace_width", layer.traceWidth},
            {"fill_opacity", layer.fillOpacity},
            {"beam_head_size", layer.beamHeadSize},
            {"velocity_modulation", layer.velocityModulation},
            {"xy_auto_gain", layer.xyAutoGain},
            {"x_offset", layer.xOffset}, {"y_offset", layer.yOffset},
            {"x_scale", layer.xScale}, {"y_scale", layer.yScale},
            {"use_layer_persistence", layer.useLayerPersistence},
            {"audio_channel", layer.audioChannel},
            {"bar_anchor", layer.barAnchor}
        };
        layerTable.insert_or_assign("xy", writeXYSettings(layer.xy));
        layersArray.push_back(std::move(layerTable));
    }
    tbl.insert_or_assign("layers", layersArray);

    std::ofstream file(fullPath);
    if (!file) return false;
    file << tbl;
    return true;
}

bool ConfigManager::load(const std::string& filename, AppConfig& config) {
    std::string fullPath = filename.empty() ? getConfigPath() : filename;
    if (!fs::exists(fullPath)) return false;

    try {
        toml::table tbl = toml::parse_file(fullPath);

        if (auto app = tbl["application"].as_table()) {
            config.musicFolder = (*app)["music_folder"].value_or("");
            config.particlesEnabled = (*app)["particles_enabled"].value_or(false);
            config.beatSensitivity = (*app)["beat_sensitivity"].value_or(1.3f);
            config.particleCount = (*app)["particle_count"].value_or(500);
            config.particleSpeed = (*app)["particle_speed"].value_or(1.0f);
            config.particleSize = (*app)["particle_size"].value_or(5.0f);
            
            if (auto pColor = (*app)["particle_color"].as_array()) {
                for (int i = 0; i < 4 && i < (int)pColor->size(); ++i) {
                    config.particleColor[i] = (*pColor)[i].value_or(1.0f);
                }
            }
            config.phosphorDecay = (*app)["phosphor_decay"].value_or(0.1f);
            config.globalGain = (*app)["global_gain"].value_or(1.0f);
            config.enableVsync = (*app)["enable_vsync"].value_or(true);
            config.targetFps = (*app)["target_fps"].value_or(60);
            config.audioMode = (*app)["audio_mode"].value_or(0);
            config.captureDeviceName = (*app)["capture_device_name"].value_or("");
            config.useSpecificCaptureDevice = (*app)["use_specific_capture_device"].value_or(false);
            config.vidWidth = (int)(*app)["vid_width"].value_or(1920);
            config.vidHeight = (int)(*app)["vid_height"].value_or(1080);
            config.vidFps = (int)(*app)["vid_fps"].value_or(60);
            config.vidCodecIdx = (int)(*app)["vid_codec_idx"].value_or(0);
            config.vidCrf = (int)(*app)["vid_crf"].value_or(16);
            config.vidOutputPath = (*app)["vid_output_path"].value_or("output.mp4");
            config.zenKunEnabled = (*app)["zen_kun_enabled"].value_or(false);
            config.bgPath = (*app)["bg_path"].value_or("");
            config.bgPulse = (*app)["bg_pulse"].value_or(0.05f);
            config.bgShake = (*app)["bg_shake"].value_or(0.02f);
            config.shakeTilt = (*app)["bg_shake_tilt"].value_or(0.05f);
            config.shakeZoom = (*app)["bg_shake_zoom"].value_or(0.02f);
            config.songTitle = (*app)["song_title"].value_or("Song Title");
            config.artistName = (*app)["song_artist"].value_or("Artist Name");
            config.showSongInfo = (*app)["show_song_info"].value_or(true);
        }

        if (auto scope = tbl["oscilloscope"].as_table()) {
            config.hasOscilloscopeDisplay = true;
            config.oscilloscopeDisplay.profile = static_cast<ScopeProfile>((*scope)["profile"].value_or(0));
            config.oscilloscopeDisplay.graticuleEnabled = (*scope)["graticule_enabled"].value_or(true);
            config.oscilloscopeDisplay.graticuleOpacity = (*scope)["graticule_opacity"].value_or(0.18f);
            config.oscilloscopeDisplay.graticuleColumns = (*scope)["graticule_columns"].value_or(10);
            config.oscilloscopeDisplay.graticuleRows = (*scope)["graticule_rows"].value_or(8);
            config.oscilloscopeDisplay.phosphorFastDecayMs = (*scope)["fast_decay_ms"].value_or(65.0f);
            config.oscilloscopeDisplay.phosphorSlowDecayMs = (*scope)["slow_decay_ms"].value_or(500.0f);
            config.oscilloscopeDisplay.phosphorSlowWeight = (*scope)["slow_weight"].value_or(0.25f);
            config.oscilloscopeDisplay.phosphorSaturation = (*scope)["saturation"].value_or(1.0f);
            config.oscilloscopeDisplay.decayColorShift = (*scope)["decay_color_shift"].value_or(0.08f);
            config.oscilloscopeDisplay.measurementOverlay = (*scope)["measurement_overlay"].value_or(false);
            config.oscilloscopeDisplay.overlayInVideo = (*scope)["overlay_in_video"].value_or(false);
        }

        if (auto layers = tbl["layers"].as_array()) {
            config.layers.clear();
            for (auto& item : *layers) {
                if (auto layerTbl = item.as_table()) {
                    ConfigLayer l;
                    l.layerId = static_cast<LayerId>((*layerTbl)["layer_id"].value_or(int64_t{0}));
                    l.name = (*layerTbl)["name"].value_or("Unnamed Layer");
                    l.gain = (*layerTbl)["gain"].value_or(1.0f);
                    l.falloff = (*layerTbl)["falloff"].value_or(0.9f);
                    l.minFreq = (*layerTbl)["min_freq"].value_or(20.0f);
                    l.maxFreq = (*layerTbl)["max_freq"].value_or(20000.0f);
                    l.numBars = (size_t)(*layerTbl)["num_bars"].value_or(256);
                    l.attack = (*layerTbl)["attack"].value_or(0.8f);
                    l.smoothing = (*layerTbl)["smoothing"].value_or(1);
                    l.spectrumPower = (*layerTbl)["spectrum_power"].value_or(1.0f);
                    l.shape = (*layerTbl)["shape"].value_or(0);
                    
                    if (auto lColor = (*layerTbl)["color"].as_array()) {
                        for (int i = 0; i < 4 && i < (int)lColor->size(); ++i) {
                            l.color[i] = (*lColor)[i].value_or(1.0f);
                        }
                    }
                    
                    l.barHeight = (*layerTbl)["bar_height"].value_or(0.2f);
                    l.mirrored = (*layerTbl)["mirrored"].value_or(false);
                    l.cornerRadius = (*layerTbl)["corner_radius"].value_or(0.0f);
                    l.visible = (*layerTbl)["visible"].value_or(true);
                    l.timeScale = (*layerTbl)["time_scale"].value_or(1.0f);
                    l.rotation = (*layerTbl)["rotation"].value_or(0.0f);
                    l.flipX = (*layerTbl)["flip_x"].value_or(false);
                    l.flipY = (*layerTbl)["flip_y"].value_or(false);
                    l.bloom = (*layerTbl)["bloom"].value_or(1.0f);
                    l.showGrid = (*layerTbl)["show_grid"].value_or(true);
                    l.traceWidth = (*layerTbl)["trace_width"].value_or(2.0f);
                    l.fillOpacity = (*layerTbl)["fill_opacity"].value_or(0.0f);
                    l.beamHeadSize = (*layerTbl)["beam_head_size"].value_or(0.0f);
                    l.velocityModulation = (*layerTbl)["velocity_modulation"].value_or(0.0f);
                    l.xyAutoGain = (*layerTbl)["xy_auto_gain"].value_or(false);
                    l.xOffset = (*layerTbl)["x_offset"].value_or(0.0f);
                    l.yOffset = (*layerTbl)["y_offset"].value_or(0.0f);
                    l.xScale = (*layerTbl)["x_scale"].value_or(1.0f);
                    l.yScale = (*layerTbl)["y_scale"].value_or(1.0f);
                    l.useLayerPersistence = (*layerTbl)["use_layer_persistence"].value_or(true);
                    l.audioChannel = (*layerTbl)["audio_channel"].value_or(0);
                    l.barAnchor = (*layerTbl)["bar_anchor"].value_or(0);
                    if (auto xy = (*layerTbl)["xy"].as_table()) {
                        readXYSettings(*xy, l.xy);
                        l.hasXYSettings = true;
                    }
                    
                    config.layers.push_back(l);
                }
            }
        }
        return true;
    } catch (const toml::parse_error& err) {
        std::cerr << "Parsing failed:\n" << err << "\n";
        return false;
    } catch (...) {
        return false;
    }
}

std::string ConfigManager::getConfigPath() {
#ifdef _WIN32
    const char* appData = std::getenv("APPDATA");
    if (!appData) return "config.toml";
    fs::path configDir = fs::path(appData) / "PlasmoidVisualizerStd";
    return (configDir / "config.toml").string();
#else
    const char* home = std::getenv("HOME");
    if (!home) return "config.toml"; // Fallback

    fs::path configDir = fs::path(home) / ".config" / "PlasmoidVisualizerStd";
    return (configDir / "config.toml").string();
#endif
}
