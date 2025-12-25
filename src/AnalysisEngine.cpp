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

void AnalysisEngine::process(const std::vector<float>& buffer) {
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

    // Logarithmic binning
    float minFreq = 20.0f;
    float maxFreq = 20000.0f;
    float sampleRate = 48000.0f;

    for (size_t i = 0; i < m_numBars; ++i) {
        // Calculate frequency for this bar (logarithmic)
        float f = minFreq * powf(maxFreq / minFreq, (float)i / m_numBars);
        float binIdx = f * m_fftSize / sampleRate;
        
        int b0 = (int)binIdx;
        int b1 = b0 + 1;
        float fract = binIdx - b0;
        
        b0 = std::clamp(b0, 0, (int)(m_fftSize / 2 - 1));
        b1 = std::clamp(b1, 0, (int)(m_fftSize / 2 - 1));
        
        float mag = (m_magnitudes[b0] * (1.0f - fract) + m_magnitudes[b1] * fract) * m_gain;
        
        // Apply temporal smoothing (attack/decay)
        if (mag > m_barPrevMagnitudes[i]) {
            mag = m_barPrevMagnitudes[i] * 0.2f + mag * 0.8f;
        } else {
            mag = m_barPrevMagnitudes[i] * m_falloff;
        }

        m_barMagnitudes[i] = mag;
        m_barPrevMagnitudes[i] = mag;
    }

    // Spatial smoothing (3-point moving average)
    std::vector<float> smoothed(m_numBars);
    for (size_t i = 0; i < m_numBars; ++i) {
        float sum = m_barMagnitudes[i] * 2.0f;
        float weight = 2.0f;
        
        if (i > 0) {
            sum += m_barMagnitudes[i-1];
            weight += 1.0f;
        }
        if (i < m_numBars - 1) {
            sum += m_barMagnitudes[i+1];
            weight += 1.0f;
        }
        
        smoothed[i] = sum / weight;
    }
    m_barMagnitudes = smoothed;
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
