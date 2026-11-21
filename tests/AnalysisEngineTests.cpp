#include "AnalysisEngine.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

namespace {
constexpr float kTwoPi = 6.28318530717958647692f;

std::vector<float> makeSine(size_t count, float freqHz, float sampleRate, float amp = 1.0f) {
    std::vector<float> buf(count);
    for (size_t i = 0; i < count; ++i) {
        buf[i] = amp * std::sin(kTwoPi * freqHz * static_cast<float>(i) / sampleRate);
    }
    return buf;
}

bool testSinePeakAtSampleRate(float sampleRate, float targetFreqHz) {
    constexpr size_t kFftSize = 8192;
    AnalysisEngine engine(kFftSize);
    const auto sine = makeSine(kFftSize, targetFreqHz, sampleRate);
    engine.computeFFT(sine);

    const auto& mags = engine.rawMagnitudes();
    const auto maxIt = std::max_element(mags.begin(), mags.end());
    const size_t peakBin = static_cast<size_t>(std::distance(mags.begin(), maxIt));
    const float expectedBin = targetFreqHz * static_cast<float>(kFftSize) / sampleRate;
    if (std::abs(static_cast<float>(peakBin) - expectedBin) > 1.5f) {
        std::cerr << "FFT peak bin mismatch at " << sampleRate << " Hz: got "
                  << peakBin << ", expected ~" << expectedBin << "\n";
        return false;
    }

    LayerConfig cfg;
    cfg.numBars = 128;
    cfg.minFreq = 20.0f;
    cfg.maxFreq = 20000.0f;
    cfg.attack = 1.0f;
    cfg.smoothing = 0;
    cfg.gain = 1.0f;

    std::vector<float> prev;
    std::vector<float> out;
    engine.computeLayerMagnitudesFrom(mags, cfg, prev, out, sampleRate, 1.0f / 60.0f);

    const auto barMaxIt = std::max_element(out.begin(), out.end());
    const size_t peakBar = static_cast<size_t>(std::distance(out.begin(), barMaxIt));
    const float logPos = std::log(targetFreqHz / cfg.minFreq) / std::log(cfg.maxFreq / cfg.minFreq);
    const float expectedBar = logPos * static_cast<float>(cfg.numBars);
    if (std::abs(static_cast<float>(peakBar) - expectedBar) > 2.0f) {
        std::cerr << "Log-binned peak bar mismatch at " << sampleRate << " Hz: got "
                  << peakBar << ", expected ~" << expectedBar << "\n";
        return false;
    }
    return true;
}

bool testDeltaTimeInvariantFalloff() {
    constexpr size_t kFftSize = 8192;
    AnalysisEngine engine(kFftSize);
    const std::vector<float> silence(kFftSize, 0.0f);
    engine.computeFFT(silence);

    LayerConfig cfg;
    cfg.numBars = 16;
    cfg.falloff = 0.81f;
    cfg.smoothing = 0;

    std::vector<float> prev60(16, 1.0f);
    std::vector<float> out60;
    engine.computeLayerMagnitudes(cfg, prev60, out60, 48000.0f, 1.0f / 60.0f);

    std::vector<float> prev120(16, 1.0f);
    std::vector<float> out120;
    engine.computeLayerMagnitudes(cfg, prev120, out120, 48000.0f, 1.0f / 120.0f);
    engine.computeLayerMagnitudes(cfg, prev120, out120, 48000.0f, 1.0f / 120.0f);

    if (std::abs(out60[0] - out120[0]) > 1e-4f) {
        std::cerr << "Delta-time falloff invariance failed: 60Hz=" << out60[0]
                  << " vs 2x120Hz=" << out120[0] << "\n";
        return false;
    }
    return true;
}

bool testBeatDetection() {
    constexpr size_t kFftSize = 8192;
    AnalysisEngine engine(kFftSize);
    engine.setBeatSensitivity(1.3f);

    const std::vector<float> quiet = makeSine(kFftSize, 80.0f, 48000.0f, 0.01f);
    for (int i = 0; i < 60; ++i) {
        engine.computeFFT(quiet);
        (void)engine.detectBeat(1.0f / 60.0f, 48000.0f);
    }

    const std::vector<float> kick = makeSine(kFftSize, 80.0f, 48000.0f, 1.0f);
    engine.computeFFT(kick);
    if (!engine.detectBeat(1.0f / 60.0f, 48000.0f)) {
        std::cerr << "Beat detector failed to fire on bass transient\n";
        return false;
    }
    // Immediate next frame inside 100 ms cooldown should not re-trigger.
    if (engine.detectBeat(1.0f / 60.0f, 48000.0f)) {
        std::cerr << "Beat detector ignored cooldown period\n";
        return false;
    }
    return true;
}
} // namespace

int main() {
    bool ok = true;
    ok &= testSinePeakAtSampleRate(44100.0f, 1000.0f);
    ok &= testSinePeakAtSampleRate(48000.0f, 1000.0f);
    ok &= testSinePeakAtSampleRate(192000.0f, 1000.0f);
    ok &= testDeltaTimeInvariantFalloff();
    ok &= testBeatDetection();
    return ok ? 0 : 1;
}
