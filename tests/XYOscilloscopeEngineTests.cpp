#include "XYOscilloscopeEngine.hpp"

#include <cmath>
#include <iostream>

namespace {
XYInputChunk sineChunk(float frequencyX, float frequencyY, size_t count = 4800) {
    XYInputChunk chunk;
    chunk.sampleRate = 48000;
    chunk.discontinuityGeneration = 1;
    chunk.samples.reserve(count);
    constexpr float pi = 3.14159265358979323846f;
    for (size_t i = 0; i < count; ++i) {
        const float time = static_cast<float>(i) / chunk.sampleRate;
        chunk.samples.push_back({std::sin(2.0f * pi * frequencyX * time), std::sin(2.0f * pi * frequencyY * time), 1.0f});
    }
    return chunk;
}

bool near(float actual, float expected, float tolerance) {
    return std::abs(actual - expected) <= tolerance;
}
}

int main() {
    XYOscilloscopeEngine engine;
    XYLayerSettings settings;
    settings.bandwidthHz = 12000.0f;

    auto input = sineChunk(1000.0f, 500.0f);
    const auto first = engine.processContinuous(11, settings, input, 1920, 1080);
    if (first.points.empty() || !first.points.front().breakBefore) {
        std::cerr << "Continuous acquisition did not start with a blanked beam\n";
        return 1;
    }
    if (!near(first.measurements.frequencyX, 1000.0f, 20.0f) || !near(first.measurements.frequencyY, 500.0f, 12.0f)) {
        std::cerr << "Frequency measurement is inaccurate\n";
        return 1;
    }
    if (first.measurements.ratioNumerator != 2 || first.measurements.ratioDenominator != 1) {
        std::cerr << "Frequency ratio was not reduced to 2:1\n";
        return 1;
    }

    input.dropped = true;
    input.firstFrame = 4800;
    const auto discontinuous = engine.processContinuous(11, settings, input, 1920, 1080);
    if (discontinuous.points.empty() || !discontinuous.points.front().breakBefore) {
        std::cerr << "Dropped input was connected to the previous trace\n";
        return 1;
    }

    XYLayerSettings triggered = settings;
    triggered.persistence = false;
    triggered.triggerMode = TriggerMode::Normal;
    const auto sweep = engine.processTriggered(12, triggered, sineChunk(1000.0f, 1000.0f), 1280, 720);
    if (sweep.points.empty()) {
        std::cerr << "Triggered acquisition did not produce a sweep\n";
        return 1;
    }
    XYInputChunk noTrigger;
    noTrigger.sampleRate = 48000;
    noTrigger.discontinuityGeneration = 1;
    noTrigger.samples.assign(200, {0.8f, 0.0f, 1.0f});
    const auto held = engine.processTriggered(12, triggered, noTrigger, 1280, 720);
    if (held.points.size() != sweep.points.size()) {
        std::cerr << "Normal trigger mode did not retain its last valid sweep\n";
        return 1;
    }
    return 0;
}
