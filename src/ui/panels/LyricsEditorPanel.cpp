#include "UIManager.hpp"
#include "UIHelpers.hpp"
#include "Utf8Paths.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

const std::vector<std::filesystem::path>& UIManager::getSystemFonts() {
    if (m_systemFontsLoaded) return m_systemFonts;
    m_systemFontsLoaded = true;
    m_systemFonts.clear();

    std::vector<std::filesystem::path> roots;
#ifdef _WIN32
    if (const wchar_t* windows = _wgetenv(L"WINDIR")) {
        roots.emplace_back(std::filesystem::path(windows) / L"Fonts");
    }
#else
    roots.emplace_back("/usr/share/fonts");
    if (const char* userHome = std::getenv("HOME")) {
        roots.emplace_back(std::filesystem::path(userHome) / ".local/share/fonts");
    }
#endif
    for (const auto& root : roots) {
        std::error_code error;
        if (!std::filesystem::exists(root, error) || error) continue;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(
                 root, std::filesystem::directory_options::skip_permission_denied, error)) {
            if (error) break;
            if (!entry.is_regular_file(error) || error) continue;
            std::string extension = Utf8Paths::toUtf8(entry.path().extension());
            std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            if (extension == ".ttf" || extension == ".otf" || extension == ".ttc") {
                m_systemFonts.push_back(entry.path());
            }
        }
    }
    std::sort(m_systemFonts.begin(), m_systemFonts.end(), [](const auto& a, const auto& b) {
        return Utf8Paths::toUtf8(a.filename()) < Utf8Paths::toUtf8(b.filename());
    });
    return m_systemFonts;
}

void UIManager::renderLyricsEditor(AppState& state, AudioEngine& audioEngine) {
    if (!state.ui.showLyricsEditor) return;

    ImGui::SetNextWindowSize(ImVec2(680.0f, 650.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Media Overlay Layer", &state.ui.showLyricsEditor)) {
        ImGui::End();
        return;
    }

    auto& layer = state.config.mediaOverlay;
    ImGui::Checkbox("Enable media overlay layer", &layer.enabled);
    ImGui::SameLine();
    ImGui::Checkbox("Top spectrum", &layer.style.showTopSpectrum);
    ImGui::SameLine();
    ImGui::Checkbox("Bottom spectrum", &layer.style.showBottomSpectrum);
    ImGui::SliderFloat("Overlay opacity", &layer.style.opacity, 0.1f, 1.0f, "%.2f");
    ImGui::SliderFloat("Spectrum gain", &layer.style.spectrumGain, 0.1f, 4.0f, "%.2f");
    if (ImGui::CollapsingHeader("Spectrum layout", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderFloat(
            "Amplitude cap", &layer.style.spectrumAmplitudeCap,
            0.1f, 8.0f, "%.2fx");
        ImGui::SliderInt("Bar count", &layer.style.spectrumBarCount, 16, 256);
        ImGui::SliderFloat("Bar width", &layer.style.spectrumBarWidth, 0.1f, 1.0f, "%.2f");
        ImGui::SliderFloat(
            "Baseline thickness", &layer.style.spectrumLineThickness,
            0.0005f, 0.01f, "%.4f");
        ImGui::SliderFloat(
            "Top bar height", &layer.style.topSpectrumHeight,
            0.005f, 0.25f, "%.3f");
        ImGui::SliderFloat(
            "Bottom bar height", &layer.style.bottomSpectrumHeight,
            0.005f, 0.25f, "%.3f");
        ImGui::SliderFloat("Top margin", &layer.style.topMargin, 0.01f, 0.3f, "%.3f");
        ImGui::SliderFloat(
            "Bottom margin", &layer.style.bottomMargin, 0.01f, 0.3f, "%.3f");
    }
    if (ImGui::CollapsingHeader("Typography sizes")) {
        ImGui::SliderFloat(
            "Artist size", &layer.style.artistTextScale, 0.2f, 2.5f, "%.2f");
        ImGui::SliderFloat(
            "Title size", &layer.style.titleTextScale, 0.2f, 2.5f, "%.2f");
        ImGui::SliderFloat(
            "Timestamp size", &layer.style.timestampTextScale, 0.2f, 2.5f, "%.2f");
        ImGui::SliderFloat(
            "Lyrics size", &layer.style.lyricsTextScale, 0.2f, 3.5f, "%.2f");
    }
    ImGui::Checkbox("Gaussian-blurred lyrics band", &layer.style.blurredLyricsBand);
    ImGui::SliderFloat("Lyrics band height", &layer.style.lyricsBandHeight, 0.05f, 0.3f, "%.3f");
    ImGui::SliderFloat("Lyrics band opacity", &layer.style.lyricsBandOpacity, 0.0f, 1.0f, "%.2f");

    ImGui::SeparatorText("Background and typography");
    const char* mediaTypes[] = {"None", "Still image", "Animated GIF", "Looped video"};
    int mediaType = static_cast<int>(layer.backgroundType);
    if (ImGui::Combo("Background source", &mediaType, mediaTypes, IM_ARRAYSIZE(mediaTypes))) {
        layer.backgroundType = static_cast<OverlayBackgroundType>(mediaType);
    }
    if (layer.backgroundType != OverlayBackgroundType::None) {
        UIHelpers::InputText("Media path", layer.backgroundPath);
        const char* fitModes[] = {"Cover (crop)", "Contain (letterbox)", "Stretch"};
        int fit = static_cast<int>(layer.backgroundFit);
        if (ImGui::Combo("Image scaling", &fit, fitModes, IM_ARRAYSIZE(fitModes))) {
            layer.backgroundFit = static_cast<OverlayBackgroundFit>(fit);
        }
        ImGui::SliderFloat(
            "Background opacity", &layer.backgroundOpacity, 0.0f, 1.0f, "%.2f");
        ImGui::SliderFloat(
            "Background dimming", &layer.backgroundDimming, 0.0f, 1.0f, "%.2f");
        ImGui::SliderFloat(
            "Background zoom", &layer.backgroundScale, 0.25f, 3.0f, "%.2f");
        ImGui::SliderFloat(
            "Background X", &layer.backgroundOffsetX, -1.0f, 1.0f, "%.3f");
        ImGui::SliderFloat(
            "Background Y", &layer.backgroundOffsetY, -1.0f, 1.0f, "%.3f");
        if (layer.backgroundType == OverlayBackgroundType::LoopedVideo) {
            const char* decoders[] = {
                "Auto hardware acceleration", "Software (CPU)", "D3D11VA (Windows GPU)"};
            int decoder = static_cast<int>(layer.videoDecoder);
            if (ImGui::Combo(
                    "Video decoder", &decoder, decoders, IM_ARRAYSIZE(decoders))) {
                layer.videoDecoder = static_cast<OverlayVideoDecoder>(decoder);
            }
            ImGui::SliderInt("Background video FPS", &layer.videoFps, 10, 60);
            ImGui::TextDisabled(
                "Realtime decoding runs on a worker thread; export remains deterministic.");
        }
    }
    UIHelpers::InputText("Artist", layer.artist);
    UIHelpers::InputText("Title", layer.title);
    UIHelpers::InputText("Font file", layer.fontPath);
    const auto& fonts = getSystemFonts();
    const std::string fontPreview = !layer.fontPath.empty()
        ? Utf8Paths::toUtf8(Utf8Paths::fromUtf8(layer.fontPath).filename())
        : "Choose a system font...";
    if (ImGui::BeginCombo("System fonts", fontPreview.c_str())) {
        for (const auto& font : fonts) {
            const std::string fontPathUtf8 = Utf8Paths::toUtf8(font);
            const std::string fontNameUtf8 = Utf8Paths::toUtf8(font.filename());
            const bool selected = fontPathUtf8 == layer.fontPath;
            if (ImGui::Selectable(fontNameUtf8.c_str(), selected)) {
                layer.fontPath = fontPathUtf8;
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    UIHelpers::InputText("Lyrics font file", layer.lyricsFontPath);
    const std::string lyricsFontPreview = !layer.lyricsFontPath.empty()
        ? Utf8Paths::toUtf8(Utf8Paths::fromUtf8(layer.lyricsFontPath).filename())
        : "Inherit metadata font";
    if (ImGui::BeginCombo("Lyrics system font", lyricsFontPreview.c_str())) {
        if (ImGui::Selectable("Inherit metadata font", layer.lyricsFontPath.empty())) {
            layer.lyricsFontPath.clear();
        }
        for (const auto& font : fonts) {
            const std::string fontPathUtf8 = Utf8Paths::toUtf8(font);
            const std::string fontNameUtf8 = Utf8Paths::toUtf8(font.filename());
            const bool selected = fontPathUtf8 == layer.lyricsFontPath;
            if (ImGui::Selectable(fontNameUtf8.c_str(), selected)) {
                layer.lyricsFontPath = fontPathUtf8;
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    const float playhead = audioEngine.getPosition();
    ImGui::SeparatorText("Live preview");
    ImGui::Text("Playhead  %02d:%05.2f", static_cast<int>(playhead) / 60, std::fmod(playhead, 60.0f));
    UIHelpers::InputText("Override", layer.lyricPreview);
    ImGui::SameLine();
    if (ImGui::Button("Clear override")) layer.lyricPreview.clear();
    const LyricEvent* active = layer.lyrics.activeEvent(playhead);
    ImGui::TextWrapped("Output: %s",
        !layer.lyricPreview.empty()
            ? layer.lyricPreview.c_str()
            : (active ? active->text.c_str() : "(no active lyric)"));

    if (ImGui::BeginTabBar("LyricsEditorTabs")) {
        if (ImGui::BeginTabItem("Timeline")) {
            bool timelineEdited = false;
            if (ImGui::Button("Add at playhead")) {
                layer.lyrics.events.push_back({playhead, playhead + 3.0f, "New lyric"});
                layer.lyrics.sortAndNormalize();
                const auto found = std::find_if(
                    layer.lyrics.events.begin(), layer.lyrics.events.end(),
                    [playhead](const LyricEvent& event) { return event.startSeconds == playhead; });
                state.ui.selectedLyricIdx = static_cast<int>(std::distance(layer.lyrics.events.begin(), found));
                timelineEdited = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Sort / normalize")) {
                layer.lyrics.sortAndNormalize();
                timelineEdited = true;
            }

            if (ImGui::BeginTable(
                    "LyricEvents", 3,
                    ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                    ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable,
                    ImVec2(0.0f, 230.0f))) {
                ImGui::TableSetupColumn("Start", ImGuiTableColumnFlags_WidthFixed, 75.0f);
                ImGui::TableSetupColumn("End", ImGuiTableColumnFlags_WidthFixed, 75.0f);
                ImGui::TableSetupColumn("Text");
                ImGui::TableHeadersRow();
                for (int i = 0; i < static_cast<int>(layer.lyrics.events.size()); ++i) {
                    const auto& event = layer.lyrics.events[i];
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("%.2f", event.startSeconds);
                    ImGui::TableSetColumnIndex(1);
                    ImGui::Text("%.2f", event.endSeconds);
                    ImGui::TableSetColumnIndex(2);
                    ImGui::PushID(i);
                    if (ImGui::Selectable(
                            event.text.c_str(), state.ui.selectedLyricIdx == i,
                            ImGuiSelectableFlags_SpanAllColumns)) {
                        state.ui.selectedLyricIdx = i;
                    }
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }

            if (state.ui.selectedLyricIdx >= 0 &&
                state.ui.selectedLyricIdx < static_cast<int>(layer.lyrics.events.size())) {
                auto& event = layer.lyrics.events[state.ui.selectedLyricIdx];
                ImGui::SeparatorText("Selected event");
                if (ImGui::DragFloat("Start (seconds)", &event.startSeconds, 0.01f, 0.0f, 86400.0f, "%.2f")) timelineEdited = true;
                ImGui::SameLine();
                if (ImGui::Button("Start = playhead")) { event.startSeconds = playhead; timelineEdited = true; }
                if (ImGui::DragFloat("End (seconds)", &event.endSeconds, 0.01f, 0.0f, 86400.0f, "%.2f")) timelineEdited = true;
                ImGui::SameLine();
                if (ImGui::Button("End = playhead")) { event.endSeconds = playhead; timelineEdited = true; }

                if (UIHelpers::InputText("Lyric text", event.text)) timelineEdited = true;
                if (ImGui::Button("Preview selected")) layer.lyricPreview = event.text;
                ImGui::SameLine();
                if (ImGui::Button("Seek to start")) audioEngine.seekTo(event.startSeconds);
                ImGui::SameLine();
                if (ImGui::Button("Delete event")) {
                    layer.lyrics.events.erase(layer.lyrics.events.begin() + state.ui.selectedLyricIdx);
                    state.ui.selectedLyricIdx = std::min(
                        state.ui.selectedLyricIdx, static_cast<int>(layer.lyrics.events.size()) - 1);
                    timelineEdited = true;
                }
            }
            if (timelineEdited) {
                layer.lyricSource = layer.lyrics.toLrc();
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Raw LRC")) {
            ImGui::TextUnformatted("Format: [mm:ss.xx] lyric text");
            UIHelpers::InputTextMultiline(
                "##RawLyrics", layer.lyricSource,
                ImVec2(-1.0f, 285.0f), ImGuiInputTextFlags_AllowTabInput);
            if (ImGui::Button("Parse and apply")) {
                std::string error;
                if (layer.lyrics.parseLrc(layer.lyricSource, &error)) {
                    state.ui.statusMessage = "Lyrics parsed successfully.";
                    state.ui.statusColor = ImVec4(0.2f, 1.0f, 0.3f, 1.0f);
                    state.ui.selectedLyricIdx = layer.lyrics.events.empty() ? -1 : 0;
                } else {
                    state.ui.statusMessage = error;
                    state.ui.statusColor = ImVec4(1.0f, 0.3f, 0.2f, 1.0f);
                }
            }
            ImGui::Separator();
            UIHelpers::InputText("LRC path", layer.lyricFilePath);
            if (ImGui::Button("Load LRC")) {
                std::ifstream input(
                    Utf8Paths::fromUtf8(layer.lyricFilePath), std::ios::binary);
                std::ostringstream contents;
                contents << input.rdbuf();
                if (input) {
                    layer.lyricSource = contents.str();
                    layer.lyrics.parseLrc(layer.lyricSource);
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Save LRC")) {
                std::ofstream output(
                    Utf8Paths::fromUtf8(layer.lyricFilePath), std::ios::binary);
                output << layer.lyricSource;
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
}
