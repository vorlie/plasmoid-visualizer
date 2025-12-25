#ifndef ANALYSIS_ENGINE_HPP
#define ANALYSIS_ENGINE_HPP

#include "kiss_fft.h"
#include <vector>
#include <complex>

struct FrequencyBand {
    float frequency;
    float energy;
};

class AnalysisEngine {
public:
    AnalysisEngine(size_t fftSize);
    ~AnalysisEngine();

    void process(const std::vector<float>& buffer);
    
    const std::vector<float>& getMagnitudes() const { return m_barMagnitudes; }
    float getEnergyInRange(float minFreq, float maxFreq, float sampleRate) const;

    // Falloff and Gain
    void setGain(float gain) { m_gain = gain; }
    void setFalloff(float falloff) { m_falloff = falloff; }

private:
    size_t m_fftSize;
    kiss_fft_cfg m_cfg;
    std::vector<kiss_fft_cpx> m_in, m_out;
    std::vector<float> m_magnitudes;
    std::vector<float> m_prevMagnitudes;
    
    // Logarithmic binning
    size_t m_numBars = 256;
    std::vector<float> m_barMagnitudes;
    std::vector<float> m_barPrevMagnitudes;

    float m_gain = 1.0f;
    float m_falloff = 0.9f; // Exponential decay
};

#endif // ANALYSIS_ENGINE_HPP
