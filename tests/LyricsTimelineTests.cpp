#include "LyricsTimeline.hpp"

#include <cmath>
#include <iostream>
#include <string>

namespace {
bool near(float a, float b, float eps = 0.02f) {
    return std::abs(a - b) <= eps;
}
} // namespace

int main() {
    LyricsTimeline timeline;
    std::string error;

    // 1. Multi-timestamp lines, milliseconds, unsorted order, and non-timestamp metadata lines.
    const std::string sampleLrc =
        "[ar:Test Artist]\n"
        "[00:05.500]Second line\n"
        "[00:01.25][00:10.00]Chorus\n"
        "Invalid line without timestamp\n";

    if (!timeline.parseLrc(sampleLrc, &error)) {
        std::cerr << "parseLrc failed unexpectedly: " << error << "\n";
        return 1;
    }
    if (timeline.events.size() != 3) {
        std::cerr << "Expected 3 events, got " << timeline.events.size() << "\n";
        return 1;
    }

    // Verify sorting & normalized end times.
    if (!near(timeline.events[0].startSeconds, 1.25f) || timeline.events[0].text != "Chorus" ||
        !near(timeline.events[0].endSeconds, 5.5f) ||
        !near(timeline.events[1].startSeconds, 5.5f) || timeline.events[1].text != "Second line" ||
        !near(timeline.events[1].endSeconds, 10.0f) ||
        !near(timeline.events[2].startSeconds, 10.0f) || timeline.events[2].text != "Chorus") {
        std::cerr << "Sorted/normalized lyric events do not match expected values\n";
        return 1;
    }

    // 2. Active event lookups across boundaries.
    if (timeline.activeEvent(0.5f) != nullptr) {
        std::cerr << "Expected no active lyric before first timestamp\n";
        return 1;
    }
    const LyricEvent* at2s = timeline.activeEvent(2.0f);
    if (!at2s || at2s->text != "Chorus") {
        std::cerr << "Expected 'Chorus' at 2.0s\n";
        return 1;
    }
    const LyricEvent* at6s = timeline.activeEvent(6.0f);
    if (!at6s || at6s->text != "Second line") {
        std::cerr << "Expected 'Second line' at 6.0s\n";
        return 1;
    }
    if (timeline.activeEvent(20.0f) != nullptr) {
        std::cerr << "Expected no active lyric well past final endSeconds\n";
        return 1;
    }

    // 3. Serialization round-trip via toLrc().
    const std::string serialized = timeline.toLrc();
    LyricsTimeline reloaded;
    if (!reloaded.parseLrc(serialized, &error) || reloaded.events.size() != 3) {
        std::cerr << "toLrc() round-trip failed: " << error << "\n";
        return 1;
    }
    for (size_t i = 0; i < 3; ++i) {
        if (!near(reloaded.events[i].startSeconds, timeline.events[i].startSeconds) ||
            reloaded.events[i].text != timeline.events[i].text) {
            std::cerr << "Round-trip mismatch at event " << i << "\n";
            return 1;
        }
    }

    // 4. Empty / untimed source should fail gracefully with an error message.
    LyricsTimeline emptyTimeline;
    if (emptyTimeline.parseLrc("plain text without timestamps", &error) || error.empty()) {
        std::cerr << "Untimed LRC should fail with non-empty error message\n";
        return 1;
    }

    return 0;
}
