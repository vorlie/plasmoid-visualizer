#include "AnalysisEngine.hpp"
#include <cmath>
#include <algorithm>

AnalysisEngine::AnalysisEngine(size_t fftSize) : m_fftSize(fftSize) {
    m_cfg = kiss_fft_alloc(fftSize, 0, NULL, NULL);
    m_in.resize(fftSize);
    m_out.resize(fftSize);
    m_magnitudes.resize(fftSize / 2);
    m_prevMagnitudes.resize(fftSize / 2, 0.0f);
    m_barMagnitudes.resize(m_numBars, 0.0f);
    m_barPrevMagnitudes.resize(m_numBars, 0.0f);
}

AnalysisEngine::~AnalysisEngine() {
    free(m_cfg);
}

void AnalysisEngine::computeFFT(const std::vector<float>& buffer) {
    if (buffer.empty()) return;

    // Fill input buffer with Hanning window
    size_t n = std::min(buffer.size(), m_fftSize);
    for (size_t i = 0; i < m_fftSize; ++i) {
        if (i < n) {
            float window = 0.5f * (1.0f - cosf(2.0f * M_PI * i / (m_fftSize - 1)));
            m_in[i].r = buffer[i] * window;
            m_in[i].i = 0;
        } else {
            m_in[i].r = 0;
            m_in[i].i = 0;
        }
    }

    kiss_fft(m_cfg, m_in.data(), m_out.data());

    // Calculate raw magnitudes
    for (size_t i = 0; i < m_fftSize / 2; ++i) {
        m_magnitudes[i] = sqrtf(m_out[i].r * m_out[i].r + m_out[i].i * m_out[i].i);
    }
}

std::vector<float> AnalysisEngine::computeLayerMagnitudes(const LayerConfig& config, std::vector<float>& prevMagnitudes) {
    std::vector<float> layerMagnitudes(config.numBars);
    if (prevMagnitudes.size() != config.numBars) {
        prevMagnitudes.assign(config.numBars, 0.0f);
    }

    float sampleRate = 48000.0f;

    for (size_t i = 0; i < config.numBars; ++i) {
        // Calculate frequency for this bar (logarithmic)
        float f = config.minFreq * powf(config.maxFreq / config.minFreq, (float)i / config.numBars);
        float binIdx = f * m_fftSize / sampleRate;
        
        int b0 = (int)binIdx;
        int b1 = b0 + 1;
        float fract = binIdx - b0;
        
        b0 = std::clamp(b0, 0, (int)(m_fftSize / 2 - 1));
        b1 = std::clamp(b1, 0, (int)(m_fftSize / 2 - 1));
        
        float mag = (m_magnitudes[b0] * (1.0f - fract) + m_magnitudes[b1] * fract) * config.gain;
        
        // Apply temporal smoothing (attack/decay)
        if (mag > prevMagnitudes[i]) {
            mag = prevMagnitudes[i] * 0.2f + mag * 0.8f;
        } else {
            mag = prevMagnitudes[i] * config.falloff;
        }

        layerMagnitudes[i] = mag;
        prevMagnitudes[i] = mag;
    }

    // Spatial smoothing (3-point moving average)
    std::vector<float> smoothed(config.numBars);
    for (size_t i = 0; i < config.numBars; ++i) {
        float sum = layerMagnitudes[i] * 2.0f;
        float weight = 2.0f;
        
        if (i > 0) {
            sum += layerMagnitudes[i-1];
            weight += 1.0f;
        }
        if (i < config.numBars - 1) {
            sum += layerMagnitudes[i+1];
            weight += 1.0f;
        }
        
        smoothed[i] = sum / weight;
    }
    return smoothed;
}

void AnalysisEngine::process(const std::vector<float>& buffer) {
    computeFFT(buffer);
    
    LayerConfig defaultConfig;
    defaultConfig.gain = m_gain;
    defaultConfig.falloff = m_falloff;
    defaultConfig.numBars = m_numBars;
    
    m_barMagnitudes = computeLayerMagnitudes(defaultConfig, m_barPrevMagnitudes);
}

float AnalysisEngine::getEnergyInRange(float minFreq, float maxFreq, float sampleRate) const {
    int startBin = (int)(minFreq * m_fftSize / sampleRate);
    int endBin = (int)(maxFreq * m_fftSize / sampleRate);
    
    startBin = std::clamp(startBin, 0, (int)(m_fftSize / 2 - 1));
    endBin = std::clamp(endBin, 0, (int)(m_fftSize / 2 - 1));

    float energy = 0;
    for (int i = startBin; i <= endBin; ++i) {
        energy = std::max(energy, m_magnitudes[i]);
    }
    return energy;
}

bool AnalysisEngine::detectBeat(float deltaTime) {
    if (m_magnitudes.empty()) return false;

    // 1. Calculate bass energy (approx 20Hz - 150Hz)
    // Bin size = 44100 / 8192 ~= 5.38 Hz
    // 20Hz ~= bin 3, 150Hz ~= bin 27
    float bassEnergy = 0.0f;
    int minBin = 3;
    int maxBin = 27;
    
    if (maxBin >= (int)m_magnitudes.size()) maxBin = (int)m_magnitudes.size() - 1;

    for (int i = minBin; i <= maxBin; ++i) {
        bassEnergy += m_magnitudes[i];
    }
    bassEnergy /= (maxBin - minBin + 1);

    // 2. Update moving average
    m_energyAverage = m_energyAverage * 0.95f + bassEnergy * 0.05f;

    // 3. Check for beat
    m_beatTimer -= deltaTime;
    bool isBeat = false;
    
    if (m_beatTimer <= 0.0f && bassEnergy > m_energyAverage * m_beatSensitivity && bassEnergy > 0.01f) {
        isBeat = true;
        m_beatTimer = 0.1f; // Cooldown (100ms)
    }

    return isBeat;
}
