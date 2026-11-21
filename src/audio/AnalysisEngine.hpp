#ifndef ANALYSIS_ENGINE_HPP
#define ANALYSIS_ENGINE_HPP

#include "kiss_fft.h"
#include <cstdlib>
#include <memory>
#include <type_traits>
#include <vector>

struct FrequencyBand {
    float frequency;
    float energy;
};

enum class AudioChannel {
    Mixed = 0,
    Left = 1,
    Right = 2
};

struct LayerConfig {
    float gain = 1.0f;
    float falloff = 0.9f;
    float minFreq = 20.0f;
    float maxFreq = 20000.0f;
    size_t numBars = 256;
    float attack = 0.8f;      // 0.0 to 1.0
    int smoothing = 1;        // Neighbor radius
    float spectrumPower = 1.0f; // 1.0 = linear, 0.5 = sqrt (boost lows)
    AudioChannel channel = AudioChannel::Mixed;
};

class AnalysisEngine {
public:
    explicit AnalysisEngine(size_t fftSize);
    ~AnalysisEngine() = default;
    AnalysisEngine(const AnalysisEngine&) = delete;
    AnalysisEngine& operator=(const AnalysisEngine&) = delete;
    AnalysisEngine(AnalysisEngine&&) = delete;
    AnalysisEngine& operator=(AnalysisEngine&&) = delete;

    void computeFFT(const std::vector<float>& buffer);
    const std::vector<float>& rawMagnitudes() const noexcept { return m_magnitudes; }

    void computeLayerMagnitudesFrom(
        const std::vector<float>& fftMagnitudes,
        const LayerConfig& config,
        std::vector<float>& prevMagnitudes,
        std::vector<float>& outMagnitudes,
        float sampleRate = 48000.0f,
        float deltaTime = 1.0f / 60.0f);

    void computeLayerMagnitudes(
        const LayerConfig& config,
        std::vector<float>& prevMagnitudes,
        std::vector<float>& outMagnitudes,
        float sampleRate = 48000.0f,
        float deltaTime = 1.0f / 60.0f);

    std::vector<float> computeLayerMagnitudes(
        const LayerConfig& config,
        std::vector<float>& prevMagnitudes,
        float sampleRate = 48000.0f,
        float deltaTime = 1.0f / 60.0f);

    // Backward compatibility helper
    void process(const std::vector<float>& buffer);
    const std::vector<float>& getMagnitudes() const { return m_barMagnitudes; }

    float getEnergyInRange(float minFreq, float maxFreq, float sampleRate) const;

    // Falloff and Gain
    void setGain(float gain) { m_gain = gain; }
    void setFalloff(float falloff) { m_falloff = falloff; }

    // Beat Detection
    bool detectBeat(float deltaTime, float sampleRate = 48000.0f);
    void setBeatSensitivity(float sensitivity) { m_beatSensitivity = sensitivity; }

private:
    struct KissFftDeleter {
        void operator()(kiss_fft_cfg ptr) const noexcept {
            std::free(ptr);
        }
    };

    size_t m_fftSize;
    std::unique_ptr<std::remove_pointer_t<kiss_fft_cfg>, KissFftDeleter> m_cfg;
    std::vector<float> m_window;
    std::vector<kiss_fft_cpx> m_in, m_out;
    std::vector<float> m_magnitudes;
    std::vector<float> m_smoothedScratch;

    // Default binning buffers
    size_t m_numBars = 256;
    std::vector<float> m_barMagnitudes;
    std::vector<float> m_barPrevMagnitudes;

    float m_gain = 1.0f;
    float m_falloff = 0.5f;

    // Beat Detection State
    float m_energyAverage = 0.0f;
    float m_beatSensitivity = 1.3f;
    float m_beatTimer = 0.0f;
};

#endif // ANALYSIS_ENGINE_HPP
