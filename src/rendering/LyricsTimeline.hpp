#pragma once

#include <string>
#include <vector>

struct LyricEvent {
    float startSeconds = 0.0f;
    float endSeconds = 0.0f;
    std::string text;
};

class LyricsTimeline {
public:
    std::vector<LyricEvent> events;

    void sortAndNormalize();
    const LyricEvent* activeEvent(float timeSeconds) const;
    bool parseLrc(const std::string& source, std::string* error = nullptr);
    std::string toLrc() const;
};
