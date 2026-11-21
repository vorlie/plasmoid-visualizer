#include "XYOscilloscopeEngine.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {
constexpr float Pi = 3.14159265358979323846f;

struct AxisFilterCoeffs {
    bool acCoupled = false;
    float acAlpha = 0.0f;
    float b0 = 1.0f;
    float b1 = 0.0f;
    float b2 = 0.0f;
    float a1 = 0.0f;
    float a2 = 0.0f;
};

AxisFilterCoeffs makeFilterCoeffs(
    CouplingMode coupling, float cutoff, float bandwidth, std::uint32_t sampleRate
) {
    AxisFilterCoeffs c;
    const float rate = static_cast<float>(std::max(sampleRate, 1u));
    c.acCoupled = (coupling == CouplingMode::AC);
    if (c.acCoupled) {
        c.acAlpha = 1.0f - std::exp(-2.0f * Pi * std::max(cutoff, 0.1f) / rate);
    }

    const float limitedBandwidth = std::clamp(bandwidth, 10.0f, rate * 0.45f);
    const float omega = 2.0f * Pi * limitedBandwidth / rate;
    const float cosine = std::cos(omega);
    const float sine = std::sin(omega);
    constexpr float q = 0.70710678f;
    const float alpha = sine / (2.0f * q);
    const float invA0 = 1.0f / (1.0f + alpha);
    c.b0 = ((1.0f - cosine) * 0.5f) * invA0;
    c.b1 = (1.0f - cosine) * invA0;
    c.b2 = c.b0;
    c.a1 = (-2.0f * cosine) * invA0;
    c.a2 = (1.0f - alpha) * invA0;
    return c;
}

inline float applyAxisFilter(
    float sample, const AxisFilterCoeffs& c, XYOscilloscopeEngine::AxisFilter& state
) {
    if (c.acCoupled) {
        state.dcEstimate += c.acAlpha * (sample - state.dcEstimate);
        sample -= state.dcEstimate;
    }
    const float output = c.b0 * sample + c.b1 * state.x1 + c.b2 * state.x2
                       - c.a1 * state.y1 - c.a2 * state.y2;
    state.x2 = state.x1;
    state.x1 = sample;
    state.y2 = state.y1;
    state.y1 = output;
    return output;
}

float estimateFrequency(
    const std::vector<float>& values,
    std::uint32_t sampleRate,
    float& confidence,
    std::vector<size_t>& crossings
) {
    crossings.clear();
    for (size_t i = 1; i < values.size(); ++i) {
        if (values[i - 1] <= 0.0f && values[i] > 0.0f) crossings.push_back(i);
    }
    if (crossings.size() < 3) {
        confidence = 0.0f;
        return 0.0f;
    }
    float mean = 0.0f;
    for (size_t i = 1; i < crossings.size(); ++i) {
        mean += static_cast<float>(crossings[i] - crossings[i - 1]);
    }
    mean /= static_cast<float>(crossings.size() - 1);
    float variance = 0.0f;
    for (size_t i = 1; i < crossings.size(); ++i) {
        const float difference = static_cast<float>(crossings[i] - crossings[i - 1]) - mean;
        variance += difference * difference;
    }
    variance /= static_cast<float>(crossings.size() - 1);
    confidence = std::clamp(1.0f - std::sqrt(variance) / std::max(mean, 1.0f), 0.0f, 1.0f);
    return static_cast<float>(sampleRate) / std::max(mean, 1.0f);
}

XYMeasurements measure(
    const std::vector<float>& x,
    const std::vector<float>& y,
    std::uint32_t sampleRate,
    std::vector<size_t>& crossingsScratch
) {
    XYMeasurements result;
    if (x.empty() || x.size() != y.size()) return result;
    double sumX = 0.0, sumY = 0.0, squaresX = 0.0, squaresY = 0.0;
    for (size_t i = 0; i < x.size(); ++i) {
        sumX += x[i];
        sumY += y[i];
        squaresX += static_cast<double>(x[i]) * x[i];
        squaresY += static_cast<double>(y[i]) * y[i];
        result.peakX = std::max(result.peakX, std::abs(x[i]));
        result.peakY = std::max(result.peakY, std::abs(y[i]));
    }
    result.dcX = static_cast<float>(sumX / static_cast<double>(x.size()));
    result.dcY = static_cast<float>(sumY / static_cast<double>(x.size()));
    result.rmsX = std::sqrt(static_cast<float>(squaresX / static_cast<double>(x.size())));
    result.rmsY = std::sqrt(static_cast<float>(squaresY / static_cast<double>(x.size())));
    float confidenceX = 0.0f, confidenceY = 0.0f;
    result.frequencyX = estimateFrequency(x, sampleRate, confidenceX, crossingsScratch);
    result.frequencyY = estimateFrequency(y, sampleRate, confidenceY, crossingsScratch);
    result.confidence = std::min(confidenceX, confidenceY);

    if (result.frequencyX > 0.0f && result.frequencyY > 0.0f) {
        const float ratio = result.frequencyX / result.frequencyY;
        float bestError = std::numeric_limits<float>::max();
        for (int denominator = 1; denominator <= 16; ++denominator) {
            const int numerator = std::max(1, static_cast<int>(std::round(ratio * denominator)));
            const float error = std::abs(ratio - static_cast<float>(numerator) / denominator);
            if (error < bestError) {
                bestError = error;
                result.ratioNumerator = numerator;
                result.ratioDenominator = denominator;
            }
        }
    }

    if (result.confidence > 0.5f && result.frequencyX > 0.0f &&
        std::abs(result.frequencyX - result.frequencyY) / result.frequencyX < 0.03f) {
        const int period = std::max(2, static_cast<int>(sampleRate / result.frequencyX));
        if (static_cast<size_t>(period) * 2 < x.size()) {
            const size_t iStart = static_cast<size_t>(period);
            const size_t iLimit = std::min(x.size() - static_cast<size_t>(period), iStart + static_cast<size_t>(period) * 4);
            const size_t sampleStride = std::max<size_t>(1, (iLimit - iStart) / 256);

            auto evalCorr = [&](int lag) {
                double correlation = 0.0;
                for (size_t i = iStart; i < iLimit; i += sampleStride) {
                    correlation += static_cast<double>(x[i]) * static_cast<double>(y[static_cast<size_t>(static_cast<int>(i) + lag)]);
                }
                return correlation;
            };

            const int halfPeriod = period / 2;
            const int coarseStep = std::max(1, period / 64);
            int bestLag = 0;
            double bestCorrelation = -std::numeric_limits<double>::infinity();
            for (int lag = -halfPeriod; lag <= halfPeriod; lag += coarseStep) {
                const double corr = evalCorr(lag);
                if (corr > bestCorrelation) {
                    bestCorrelation = corr;
                    bestLag = lag;
                }
            }
            // Refine +/- coarseStep at 1-sample resolution.
            if (coarseStep > 1) {
                const int refineLo = std::max(-halfPeriod, bestLag - coarseStep);
                const int refineHi = std::min(halfPeriod, bestLag + coarseStep);
                for (int lag = refineLo; lag <= refineHi; ++lag) {
                    const double corr = evalCorr(lag);
                    if (corr > bestCorrelation) {
                        bestCorrelation = corr;
                        bestLag = lag;
                    }
                }
            }
            result.phaseDegrees = 360.0f * static_cast<float>(bestLag) / static_cast<float>(period);
            result.phaseValid = true;
        }
    }
    return result;
}
}

XYTraceBatch XYOscilloscopeEngine::processContinuous(
    LayerId id, const XYLayerSettings& settings, const XYInputChunk& input,
    int width, int height
) { return process(id, settings, input, true, width, height); }

XYTraceBatch XYOscilloscopeEngine::processTriggered(
    LayerId id, const XYLayerSettings& settings, const XYInputChunk& input,
    int width, int height
) { return process(id, settings, input, false, width, height); }

XYTraceBatch XYOscilloscopeEngine::process(
    LayerId id, const XYLayerSettings& settings, const XYInputChunk& input,
    bool continuous, int width, int height
) {
    Runtime& runtime = m_runtime[id];
    if (runtime.generation != input.discontinuityGeneration) {
        runtime = {};
        runtime.generation = input.discontinuityGeneration;
    }

    size_t begin = 0;
    if (!continuous && input.samples.size() > 1) {
        const auto triggerValue = [&](size_t i) {
            return settings.triggerSource == TriggerSource::X ? input.samples[i].x : input.samples[i].y;
        };
        const float low = settings.triggerLevel - settings.triggerHysteresis;
        const float high = settings.triggerLevel + settings.triggerHysteresis;
        bool armed = settings.triggerEdge == TriggerEdge::Rising ? triggerValue(0) < low : triggerValue(0) > high;
        bool found = false;
        for (size_t i = 1; i < input.samples.size(); ++i) {
            const float value = triggerValue(i);
            if (settings.triggerEdge == TriggerEdge::Rising) {
                armed |= value < low;
                if (armed && value >= high) { begin = i; found = true; break; }
            } else {
                armed |= value > high;
                if (armed && value <= low) { begin = i; found = true; break; }
            }
        }
        if (!found && settings.triggerMode == TriggerMode::Normal && !runtime.heldSweep.points.empty()) return runtime.heldSweep;
    }

    const size_t requestedFrames = std::max<size_t>(2, static_cast<size_t>(2048.0f * settings.windowScale));
    const size_t end = continuous ? input.samples.size() : std::min(input.samples.size(), begin + requestedFrames);
    const size_t sampleCount = end > begin ? (end - begin) : 0;

    XYTraceBatch batch;
    batch.layerId = id;
    batch.firstFrame = input.firstFrame + begin;
    batch.sampleRate = input.sampleRate;
    batch.continuous = continuous;

    m_conditionedX.clear();
    m_conditionedY.clear();
    m_prepared.clear();
    m_conditionedX.reserve(sampleCount);
    m_conditionedY.reserve(sampleCount);
    m_prepared.reserve(sampleCount);

    // Precompute per-axis filter coefficients ONCE per chunk instead of per sample.
    const AxisFilterCoeffs coeffsX = makeFilterCoeffs(
        settings.couplingX, settings.acCutoffHz, settings.bandwidthHz, input.sampleRate);
    const AxisFilterCoeffs coeffsY = makeFilterCoeffs(
        settings.couplingY, settings.acCutoffHz, settings.bandwidthHz, input.sampleRate);

    float peak = 0.0f;
    for (size_t i = begin; i < end; ++i) {
        const auto& sample = input.samples[i];
        const bool invalid = !std::isfinite(sample.x) || !std::isfinite(sample.y) || !std::isfinite(sample.z);
        const float x = invalid ? 0.0f : applyAxisFilter(sample.x, coeffsX, runtime.xFilter);
        const float y = invalid ? 0.0f : applyAxisFilter(sample.y, coeffsY, runtime.yFilter);
        m_conditionedX.push_back(x);
        m_conditionedY.push_back(y);
        peak = std::max({peak, std::abs(x), std::abs(y)});
        m_prepared.push_back({x, y, sample.z, invalid});
    }

    if (settings.autoGain && peak > 1e-4f) {
        const float target = std::min(0.9f / peak, 16.0f);
        const float seconds = static_cast<float>(m_prepared.size()) / std::max(input.sampleRate, 1u);
        const float rate = target < runtime.autoGain ? 12.0f : 2.5f;
        runtime.autoGain += (target - runtime.autoGain) * (1.0f - std::exp(-rate * seconds));
    } else if (!settings.autoGain) {
        runtime.autoGain = 1.0f;
    }

    const float angle = settings.rotationDegrees * Pi / 180.0f;
    const float cosine = std::cos(angle);
    const float sine = std::sin(angle);
    const float diagonal = std::sqrt(static_cast<float>(width * width + height * height));
    const float jumpLimit = settings.jumpBlanking * diagonal;
    const float halfW = static_cast<float>(width) * 0.5f;
    const float halfH = static_cast<float>(height) * 0.5f;
    const float scaleX = settings.gainX * runtime.autoGain * (settings.invertX ? -1.0f : 1.0f);
    const float scaleY = settings.gainY * runtime.autoGain * (settings.invertY ? -1.0f : 1.0f);

    batch.points.reserve(m_prepared.size());
    bool forceBreak = input.dropped || !runtime.hasLast;
    for (size_t i = 0; i < m_prepared.size(); ++i) {
        const auto& prep = m_prepared[i];
        const float x = prep.x * scaleX;
        const float y = prep.y * scaleY;
        const float rotatedX = x * cosine - y * sine + settings.positionX;
        const float rotatedY = x * sine + y * cosine + settings.positionY;
        const float dxPixels = (rotatedX - runtime.lastX) * halfW;
        const float dyPixels = (rotatedY - runtime.lastY) * halfH;
        const float distancePixels = std::sqrt(dxPixels * dxPixels + dyPixels * dyPixels);
        const bool jump = runtime.hasLast && distancePixels > jumpLimit;
        const float dwell = 1.0f / (1.0f + distancePixels * 0.025f);
        float intensity = settings.beamIntensity * (1.0f + (dwell - 1.0f) * settings.dwellEffect);
        const float explicitZ = std::clamp(prep.z * settings.zGain + settings.zOffset, 0.0f, 1.0f);
        if (settings.zMode == ZIntensityMode::Explicit) intensity = input.hasZ ? explicitZ * settings.beamIntensity : 0.0f;
        if (settings.zMode == ZIntensityMode::Multiply && input.hasZ) intensity *= explicitZ;

        const bool breakBefore = forceBreak || prep.invalid || jump;
        const bool closeToPrevious = runtime.hasLast && distancePixels < 0.35f && !breakBefore;
        if (!closeToPrevious || i + 1 == m_prepared.size()) {
            batch.points.push_back({rotatedX, rotatedY, intensity, breakBefore});
        }
        runtime.lastX = rotatedX;
        runtime.lastY = rotatedY;
        runtime.hasLast = !prep.invalid;
        forceBreak = prep.invalid;
    }

    if (!runtime.hasMeasured || runtime.framesSinceMeasure >= 5) {
        runtime.cachedMeasurements = measure(m_conditionedX, m_conditionedY, input.sampleRate, m_crossingsScratch);
        runtime.hasMeasured = true;
        runtime.framesSinceMeasure = 0;
    } else {
        ++runtime.framesSinceMeasure;
    }
    batch.measurements = runtime.cachedMeasurements;
    if (!continuous && !batch.points.empty()) runtime.heldSweep = batch;
    return batch;
}

void XYOscilloscopeEngine::reset(LayerId id) { m_runtime.erase(id); }
void XYOscilloscopeEngine::resetAll() { m_runtime.clear(); }
