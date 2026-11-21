#include "AnalysisEngine.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace {
constexpr float kTwoPi = 6.28318530717958647692f;
}

AnalysisEngine::AnalysisEngine(size_t fftSize)
    : m_fftSize(std::max<size_t>(fftSize, 2)),
      m_cfg(kiss_fft_alloc(static_cast<int>(m_fftSize), 0, nullptr, nullptr)) {
    m_window.resize(m_fftSize);
    const float invDenom = 1.0f / static_cast<float>(m_fftSize - 1);
    for (size_t i = 0; i < m_fftSize; ++i) {
        m_window[i] = 0.5f * (1.0f - std::cos(kTwoPi * static_cast<float>(i) * invDenom));
    }
    m_in.resize(m_fftSize);
    m_out.resize(m_fftSize);
    m_magnitudes.resize(m_fftSize / 2, 0.0f);
    m_barMagnitudes.resize(m_numBars, 0.0f);
    m_barPrevMagnitudes.resize(m_numBars, 0.0f);
}

void AnalysisEngine::computeFFT(const std::vector<float>& buffer) {
    if (buffer.empty() || !m_cfg) return;

    const size_t n = std::min(buffer.size(), m_fftSize);
    for (size_t i = 0; i < n; ++i) {
        m_in[i].r = buffer[i] * m_window[i];
        m_in[i].i = 0.0f;
    }
    for (size_t i = n; i < m_fftSize; ++i) {
        m_in[i].r = 0.0f;
        m_in[i].i = 0.0f;
    }

    kiss_fft(m_cfg.get(), m_in.data(), m_out.data());

    const size_t half = m_fftSize / 2;
    for (size_t i = 0; i < half; ++i) {
        const float re = m_out[i].r;
        const float im = m_out[i].i;
        m_magnitudes[i] = std::sqrt(re * re + im * im);
    }
}

void AnalysisEngine::computeLayerMagnitudesFrom(
    const std::vector<float>& fftMagnitudes,
    const LayerConfig& config,
    std::vector<float>& prevMagnitudes,
    std::vector<float>& outMagnitudes,
    float sampleRate,
    float deltaTime
) {
    if (config.numBars == 0 || fftMagnitudes.empty()) {
        outMagnitudes.clear();
        return;
    }

    outMagnitudes.resize(config.numBars);
    if (prevMagnitudes.size() != config.numBars) {
        prevMagnitudes.assign(config.numBars, 0.0f);
    }

    const float safeSampleRate = sampleRate > 0.0f ? sampleRate : 48000.0f;
    const float safeMinFreq = std::max(config.minFreq, 1.0f);
    const float safeMaxFreq = std::max(config.maxFreq, safeMinFreq + 1.0f);
    const float frameFactor = std::clamp(deltaTime, 0.0f, 0.25f) * 60.0f;
    const float clampedAttack = std::clamp(config.attack, 0.0f, 1.0f);
    const float clampedFalloff = std::clamp(config.falloff, 0.0f, 0.9999f);
    const float effectiveAttack = clampedAttack >= 1.0f
        ? 1.0f
        : (1.0f - std::pow(1.0f - clampedAttack, frameFactor));
    const float effectiveFalloff = std::pow(clampedFalloff, frameFactor);

    const float binScale = static_cast<float>(m_fftSize) / safeSampleRate;
    const int maxBinIdx = static_cast<int>(fftMagnitudes.size()) - 1;
    const float freqRatio = safeMaxFreq / safeMinFreq;
    const float freqStep = std::pow(freqRatio, 1.0f / static_cast<float>(config.numBars));
    float freq = safeMinFreq;

    for (size_t i = 0; i < config.numBars; ++i) {
        const float binIdx = freq * binScale;
        freq *= freqStep;

        int b0 = static_cast<int>(binIdx);
        int b1 = b0 + 1;
        const float fract = binIdx - static_cast<float>(b0);

        b0 = std::clamp(b0, 0, maxBinIdx);
        b1 = std::clamp(b1, 0, maxBinIdx);

        float mag = fftMagnitudes[b0] * (1.0f - fract) + fftMagnitudes[b1] * fract;

        if (config.spectrumPower != 1.0f && mag > 0.0f) {
            mag = std::pow(mag, config.spectrumPower);
        }

        mag *= config.gain;

        if (mag > prevMagnitudes[i]) {
            mag = prevMagnitudes[i] * (1.0f - effectiveAttack) + mag * effectiveAttack;
        } else {
            mag = prevMagnitudes[i] * effectiveFalloff;
        }

        outMagnitudes[i] = mag;
        prevMagnitudes[i] = mag;
    }

    if (config.smoothing > 0) {
        m_smoothedScratch.resize(config.numBars);
        const int radius = config.smoothing;
        const int count = static_cast<int>(config.numBars);
        for (int i = 0; i < count; ++i) {
            const int lo = std::max(0, i - radius);
            const int hi = std::min(count - 1, i + radius);
            float sum = 0.0f;
            for (int idx = lo; idx <= hi; ++idx) {
                sum += outMagnitudes[idx];
            }
            m_smoothedScratch[i] = sum / static_cast<float>(hi - lo + 1);
        }
        outMagnitudes.swap(m_smoothedScratch);
    }
}

void AnalysisEngine::computeLayerMagnitudes(
    const LayerConfig& config,
    std::vector<float>& prevMagnitudes,
    std::vector<float>& outMagnitudes,
    float sampleRate,
    float deltaTime
) {
    computeLayerMagnitudesFrom(m_magnitudes, config, prevMagnitudes, outMagnitudes, sampleRate, deltaTime);
}

std::vector<float> AnalysisEngine::computeLayerMagnitudes(
    const LayerConfig& config,
    std::vector<float>& prevMagnitudes,
    float sampleRate,
    float deltaTime
) {
    std::vector<float> result;
    computeLayerMagnitudesFrom(m_magnitudes, config, prevMagnitudes, result, sampleRate, deltaTime);
    return result;
}

void AnalysisEngine::process(const std::vector<float>& buffer) {
    computeFFT(buffer);

    LayerConfig defaultConfig;
    defaultConfig.gain = m_gain;
    defaultConfig.falloff = m_falloff;
    defaultConfig.numBars = m_numBars;

    computeLayerMagnitudes(defaultConfig, m_barPrevMagnitudes, m_barMagnitudes);
}

float AnalysisEngine::getEnergyInRange(float minFreq, float maxFreq, float sampleRate) const {
    if (m_magnitudes.empty() || sampleRate <= 0.0f) return 0.0f;
    const int maxIdx = static_cast<int>(m_magnitudes.size()) - 1;
    int startBin = std::clamp(static_cast<int>(minFreq * static_cast<float>(m_fftSize) / sampleRate), 0, maxIdx);
    int endBin = std::clamp(static_cast<int>(maxFreq * static_cast<float>(m_fftSize) / sampleRate), 0, maxIdx);
    if (endBin < startBin) std::swap(startBin, endBin);

    float energy = 0.0f;
    for (int i = startBin; i <= endBin; ++i) {
        energy = std::max(energy, m_magnitudes[i]);
    }
    return energy;
}

bool AnalysisEngine::detectBeat(float deltaTime, float sampleRate) {
    if (m_magnitudes.empty()) return false;

    const float safeRate = sampleRate > 0.0f ? sampleRate : 48000.0f;
    const int maxIdx = static_cast<int>(m_magnitudes.size()) - 1;
    const int minBin = std::clamp(static_cast<int>(20.0f * static_cast<float>(m_fftSize) / safeRate), 0, maxIdx);
    const int maxBin = std::clamp(static_cast<int>(150.0f * static_cast<float>(m_fftSize) / safeRate), minBin, maxIdx);

    float bassEnergy = 0.0f;
    for (int i = minBin; i <= maxBin; ++i) {
        bassEnergy += m_magnitudes[i];
    }
    bassEnergy /= static_cast<float>(maxBin - minBin + 1);

    const float frameFactor = std::clamp(deltaTime, 0.0f, 0.25f) * 60.0f;
    const float avgRetention = std::pow(0.95f, frameFactor);
    m_energyAverage = m_energyAverage * avgRetention + bassEnergy * (1.0f - avgRetention);

    m_beatTimer -= std::max(deltaTime, 0.0f);
    bool isBeat = false;

    if (m_beatTimer <= 0.0f && bassEnergy > m_energyAverage * m_beatSensitivity && bassEnergy > 0.01f) {
        isBeat = true;
        m_beatTimer = 0.1f;
    }

    return isBeat;
}
