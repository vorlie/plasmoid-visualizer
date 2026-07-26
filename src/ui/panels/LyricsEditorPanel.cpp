#include "UIManager.hpp"
#include "Utf8Paths.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <vector>

namespace {
void copyText(char* destination, size_t capacity, const std::string& source) {
    if (capacity == 0) return;
    std::strncpy(destination, source.c_str(), capacity - 1);
    destination[capacity - 1] = '\0';
}

const std::vector<std::filesystem::path>& systemFonts() {
    static const std::vector<std::filesystem::path> fonts = [] {
        std::vector<std::filesystem::path> result;
        std::vector<std::filesystem::path> roots;
#ifdef _WIN32
        if (const char* windows = std::getenv("WINDIR")) {
            roots.emplace_back(std::filesystem::path(windows) / "Fonts");
        }
#else
        roots.emplace_back("/usr/share/fonts");
        if (const char* userHome = std::getenv("HOME")) {
            roots.emplace_back(std::filesystem::path(userHome) / ".local/share/fonts");
        }
#endif
        for (const auto& root : roots) {
            std::error_code error;
            if (!std::filesystem::exists(root, error)) continue;
            for (const auto& entry : std::filesystem::recursive_directory_iterator(
                     root, std::filesystem::directory_options::skip_permission_denied, error)) {
                if (!entry.is_regular_file(error)) continue;
                std::string extension = entry.path().extension().string();
                std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
                if (extension == ".ttf" || extension == ".otf" || extension == ".ttc") {
                    result.push_back(entry.path());
                }
            }
        }
        std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
            return Utf8Paths::toUtf8(a.filename()) < Utf8Paths::toUtf8(b.filename());
        });
        return result;
    }();
    return fonts;
}
}

void UIManager::renderLyricsEditor(AppState& state, AudioEngine& audioEngine) {
    if (!state.showLyricsEditor) return;

    ImGui::SetNextWindowSize(ImVec2(680.0f, 650.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Media Overlay Layer", &state.showLyricsEditor)) {
        ImGui::End();
        return;
    }

    auto& layer = state.mediaOverlay;
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
        ImGui::InputText("Media path", layer.backgroundPath, sizeof(layer.backgroundPath));
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
        ImGui::TextDisabled("Video backgrounds are decoded and looped through FFmpeg.");
    }
    ImGui::InputText("Artist", layer.artist, sizeof(layer.artist));
    ImGui::InputText("Title", layer.title, sizeof(layer.title));
    ImGui::InputText("Font file", layer.fontPath, sizeof(layer.fontPath));
    const auto& fonts = systemFonts();
    const std::string fontPreview = layer.fontPath[0]
        ? Utf8Paths::toUtf8(Utf8Paths::fromUtf8(layer.fontPath).filename())
        : "Choose a system font...";
    if (ImGui::BeginCombo("System fonts", fontPreview.c_str())) {
        for (const auto& font : fonts) {
            const std::string fontPathUtf8 = Utf8Paths::toUtf8(font);
            const std::string fontNameUtf8 = Utf8Paths::toUtf8(font.filename());
            const bool selected = fontPathUtf8 == layer.fontPath;
            if (ImGui::Selectable(fontNameUtf8.c_str(), selected)) {
                copyText(layer.fontPath, sizeof(layer.fontPath), fontPathUtf8);
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    ImGui::InputText(
        "Lyrics font file", layer.lyricsFontPath, sizeof(layer.lyricsFontPath));
    const std::string lyricsFontPreview = layer.lyricsFontPath[0]
        ? Utf8Paths::toUtf8(Utf8Paths::fromUtf8(layer.lyricsFontPath).filename())
        : "Inherit metadata font";
    if (ImGui::BeginCombo("Lyrics system font", lyricsFontPreview.c_str())) {
        if (ImGui::Selectable("Inherit metadata font", layer.lyricsFontPath[0] == '\0')) {
            layer.lyricsFontPath[0] = '\0';
        }
        for (const auto& font : fonts) {
            const std::string fontPathUtf8 = Utf8Paths::toUtf8(font);
            const std::string fontNameUtf8 = Utf8Paths::toUtf8(font.filename());
            const bool selected = fontPathUtf8 == layer.lyricsFontPath;
            if (ImGui::Selectable(fontNameUtf8.c_str(), selected)) {
                copyText(layer.lyricsFontPath, sizeof(layer.lyricsFontPath), fontPathUtf8);
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    const float playhead = audioEngine.getPosition();
    ImGui::SeparatorText("Live preview");
    ImGui::Text("Playhead  %02d:%05.2f", static_cast<int>(playhead) / 60, std::fmod(playhead, 60.0f));
    ImGui::InputText("Override", layer.lyricPreview, sizeof(layer.lyricPreview));
    ImGui::SameLine();
    if (ImGui::Button("Clear override")) layer.lyricPreview[0] = '\0';
    const LyricEvent* active = layer.lyrics.activeEvent(playhead);
    ImGui::TextWrapped("Output: %s",
        layer.lyricPreview[0] ? layer.lyricPreview : (active ? active->text.c_str() : "(no active lyric)"));

    if (ImGui::BeginTabBar("LyricsEditorTabs")) {
        if (ImGui::BeginTabItem("Timeline")) {
            if (ImGui::Button("Add at playhead")) {
                layer.lyrics.events.push_back({playhead, playhead + 3.0f, "New lyric"});
                layer.lyrics.sortAndNormalize();
                const auto found = std::find_if(
                    layer.lyrics.events.begin(), layer.lyrics.events.end(),
                    [playhead](const LyricEvent& event) { return event.startSeconds == playhead; });
                m_selectedLyric = static_cast<int>(std::distance(layer.lyrics.events.begin(), found));
            }
            ImGui::SameLine();
            if (ImGui::Button("Sort / normalize")) layer.lyrics.sortAndNormalize();

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
                            event.text.c_str(), m_selectedLyric == i,
                            ImGuiSelectableFlags_SpanAllColumns)) {
                        m_selectedLyric = i;
                    }
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }

            if (m_selectedLyric >= 0 &&
                m_selectedLyric < static_cast<int>(layer.lyrics.events.size())) {
                auto& event = layer.lyrics.events[m_selectedLyric];
                ImGui::SeparatorText("Selected event");
                ImGui::DragFloat("Start (seconds)", &event.startSeconds, 0.01f, 0.0f, 86400.0f, "%.2f");
                ImGui::SameLine();
                if (ImGui::Button("Start = playhead")) event.startSeconds = playhead;
                ImGui::DragFloat("End (seconds)", &event.endSeconds, 0.01f, 0.0f, 86400.0f, "%.2f");
                ImGui::SameLine();
                if (ImGui::Button("End = playhead")) event.endSeconds = playhead;

                char text[1024]{};
                copyText(text, sizeof(text), event.text);
                if (ImGui::InputText("Lyric text", text, sizeof(text))) event.text = text;
                if (ImGui::Button("Preview selected")) copyText(
                    layer.lyricPreview, sizeof(layer.lyricPreview), event.text);
                ImGui::SameLine();
                if (ImGui::Button("Seek to start")) audioEngine.seekTo(event.startSeconds);
                ImGui::SameLine();
                if (ImGui::Button("Delete event")) {
                    layer.lyrics.events.erase(layer.lyrics.events.begin() + m_selectedLyric);
                    m_selectedLyric = std::min(
                        m_selectedLyric, static_cast<int>(layer.lyrics.events.size()) - 1);
                }
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Raw LRC")) {
            ImGui::TextUnformatted("Format: [mm:ss.xx] lyric text");
            ImGui::InputTextMultiline(
                "##RawLyrics", layer.lyricSource, sizeof(layer.lyricSource),
                ImVec2(-1.0f, 285.0f), ImGuiInputTextFlags_AllowTabInput);
            if (ImGui::Button("Parse and apply")) {
                std::string error;
                if (layer.lyrics.parseLrc(layer.lyricSource, &error)) {
                    state.statusMessage = "Lyrics parsed successfully.";
                    state.statusColor = ImVec4(0.2f, 1.0f, 0.3f, 1.0f);
                    m_selectedLyric = layer.lyrics.events.empty() ? -1 : 0;
                } else {
                    state.statusMessage = error;
                    state.statusColor = ImVec4(1.0f, 0.3f, 0.2f, 1.0f);
                }
            }
            ImGui::Separator();
            ImGui::InputText("LRC path", layer.lyricFilePath, sizeof(layer.lyricFilePath));
            if (ImGui::Button("Load LRC")) {
                std::ifstream input(
                    Utf8Paths::fromUtf8(layer.lyricFilePath), std::ios::binary);
                std::ostringstream contents;
                contents << input.rdbuf();
                if (input) {
                    copyText(layer.lyricSource, sizeof(layer.lyricSource), contents.str());
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
