#include "ConfigManager.hpp"
#include "toml.hpp"
#include <fstream>
#include <iostream>
#include <filesystem>
#include <cstdlib>

namespace fs = std::filesystem;

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
        {"audio_mode", config.audioMode},
        {"capture_device_name", config.captureDeviceName},
        {"use_specific_capture_device", config.useSpecificCaptureDevice},
        {"vid_width", config.vidWidth},
        {"vid_height", config.vidHeight},
        {"vid_fps", config.vidFps},
        {"vid_codec_idx", config.vidCodecIdx},
        {"vid_crf", config.vidCrf},
        {"vid_output_path", config.vidOutputPath}
    });

    toml::array layersArray;
    for (const auto& layer : config.layers) {
        layersArray.push_back(toml::table{
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
            {"audio_channel", layer.audioChannel},
            {"bar_anchor", layer.barAnchor}
        });
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
            config.audioMode = (*app)["audio_mode"].value_or(0);
            config.captureDeviceName = (*app)["capture_device_name"].value_or("");
            config.useSpecificCaptureDevice = (*app)["use_specific_capture_device"].value_or(false);
            config.vidWidth = (int)(*app)["vid_width"].value_or(1920);
            config.vidHeight = (int)(*app)["vid_height"].value_or(1080);
            config.vidFps = (int)(*app)["vid_fps"].value_or(60);
            config.vidCodecIdx = (int)(*app)["vid_codec_idx"].value_or(0);
            config.vidCrf = (int)(*app)["vid_crf"].value_or(16);
            config.vidOutputPath = (*app)["vid_output_path"].value_or("output.mp4");
        }

        if (auto layers = tbl["layers"].as_array()) {
            config.layers.clear();
            for (auto& item : *layers) {
                if (auto layerTbl = item.as_table()) {
                    ConfigLayer l;
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
                    l.audioChannel = (*layerTbl)["audio_channel"].value_or(0);
                    l.barAnchor = (*layerTbl)["bar_anchor"].value_or(0);
                    
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
    const char* home = std::getenv("HOME");
    if (!home) return "config.toml"; // Fallback

    fs::path configDir = fs::path(home) / ".config" / "PlasmoidVisualizerStd";
    return (configDir / "config.toml").string();
}
