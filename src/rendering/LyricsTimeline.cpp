#include "LyricsTimeline.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <regex>
#include <sstream>

namespace {
std::string trim(std::string value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}
} // namespace

void LyricsTimeline::sortAndNormalize() {
    events.erase(
        std::remove_if(events.begin(), events.end(), [](const LyricEvent& event) {
            return event.text.empty();
        }),
        events.end());
    std::stable_sort(events.begin(), events.end(), [](const LyricEvent& a, const LyricEvent& b) {
        return a.startSeconds < b.startSeconds;
    });
    for (size_t i = 0; i < events.size(); ++i) {
        const float nextStart = i + 1 < events.size()
            ? events[i + 1].startSeconds
            : events[i].startSeconds + 5.0f;
        if (events[i].endSeconds <= events[i].startSeconds) {
            events[i].endSeconds = nextStart;
        }
        events[i].endSeconds = std::max(events[i].endSeconds, events[i].startSeconds + 0.05f);
    }
}

const LyricEvent* LyricsTimeline::activeEvent(float timeSeconds) const {
    const LyricEvent* result = nullptr;
    for (const auto& event : events) {
        if (event.startSeconds > timeSeconds) break;
        if (timeSeconds < event.endSeconds) result = &event;
    }
    return result;
}

bool LyricsTimeline::parseLrc(const std::string& source, std::string* error) {
    static const std::regex timeTag(R"(\[(\d{1,3}):(\d{2})(?:[.:](\d{1,3}))?\])");
    std::vector<LyricEvent> parsed;
    std::istringstream lines(source);
    std::string line;
    int lineNumber = 0;

    while (std::getline(lines, line)) {
        ++lineNumber;
        std::sregex_iterator match(line.begin(), line.end(), timeTag);
        const std::sregex_iterator end;
        if (match == end) {
            if (!trim(line).empty() && error) {
                *error = "Line " + std::to_string(lineNumber) + " has no [mm:ss.xx] timestamp.";
            }
            continue;
        }

        size_t textStart = 0;
        for (auto cursor = match; cursor != end; ++cursor) {
            textStart = static_cast<size_t>(cursor->position() + cursor->length());
        }
        const std::string lyric = trim(line.substr(textStart));
        for (; match != end; ++match) {
            const float minutes = std::stof((*match)[1].str());
            const float seconds = std::stof((*match)[2].str());
            float fraction = 0.0f;
            if ((*match)[3].matched) {
                const std::string digits = (*match)[3].str();
                fraction = std::stof(digits) / std::pow(10.0f, static_cast<float>(digits.size()));
            }
            parsed.push_back({minutes * 60.0f + seconds + fraction, 0.0f, lyric});
        }
    }

    if (parsed.empty()) {
        if (error) *error = "No timestamped lyric lines were found.";
        return false;
    }
    events = std::move(parsed);
    sortAndNormalize();
    if (error) error->clear();
    return true;
}

std::string LyricsTimeline::toLrc() const {
    std::string result;
    for (const auto& event : events) {
        const float clamped = std::max(event.startSeconds, 0.0f);
        const int totalCentis = static_cast<int>(std::round(clamped * 100.0f));
        const int minutes = totalCentis / 6000;
        const int seconds = (totalCentis / 100) % 60;
        const int centis = totalCentis % 100;
        char stamp[32]{};
        std::snprintf(stamp, sizeof(stamp), "[%02d:%02d.%02d]", minutes, seconds, centis);
        if (!result.empty()) result.push_back('\n');
        result += stamp;
        result += event.text;
    }
    return result;
}
