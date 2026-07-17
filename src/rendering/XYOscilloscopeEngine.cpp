#include "XYOscilloscopeEngine.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {
constexpr float Pi = 3.14159265358979323846f;

float conditionAxis(
    float sample, CouplingMode coupling, float cutoff, float bandwidth,
    std::uint32_t sampleRate, XYOscilloscopeEngine::AxisFilter& state
) {
    const float rate = static_cast<float>(std::max(sampleRate, 1u));
    if (coupling == CouplingMode::AC) {
        const float alpha = 1.0f - std::exp(-2.0f * Pi * std::max(cutoff, 0.1f) / rate);
        state.dcEstimate += alpha * (sample - state.dcEstimate);
        sample -= state.dcEstimate;
    }

    const float limitedBandwidth = std::clamp(bandwidth, 10.0f, rate * 0.45f);
    const float omega = 2.0f * Pi * limitedBandwidth / rate;
    const float cosine = std::cos(omega);
    const float sine = std::sin(omega);
    const float q = 0.70710678f;
    const float alpha = sine / (2.0f * q);
    const float a0 = 1.0f + alpha;
    const float b0 = ((1.0f - cosine) * 0.5f) / a0;
    const float b1 = (1.0f - cosine) / a0;
    const float b2 = b0;
    const float a1 = (-2.0f * cosine) / a0;
    const float a2 = (1.0f - alpha) / a0;
    const float output = b0 * sample + b1 * state.x1 + b2 * state.x2
                       - a1 * state.y1 - a2 * state.y2;
    state.x2 = state.x1;
    state.x1 = sample;
    state.y2 = state.y1;
    state.y1 = output;
    return output;
}

float estimateFrequency(
    const std::vector<float>& values, std::uint32_t sampleRate, float& confidence
) {
    std::vector<size_t> crossings;
    for (size_t i = 1; i < values.size(); ++i) {
        if (values[i - 1] <= 0.0f && values[i] > 0.0f) crossings.push_back(i);
    }
    if (crossings.size() < 3) { confidence = 0.0f; return 0.0f; }
    float mean = 0.0f;
    for (size_t i = 1; i < crossings.size(); ++i) mean += static_cast<float>(crossings[i] - crossings[i - 1]);
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

XYMeasurements measure(const std::vector<float>& x, const std::vector<float>& y, std::uint32_t sampleRate) {
    XYMeasurements result;
    if (x.empty() || x.size() != y.size()) return result;
    double sumX = 0.0, sumY = 0.0, squaresX = 0.0, squaresY = 0.0;
    for (size_t i = 0; i < x.size(); ++i) {
        sumX += x[i]; sumY += y[i];
        squaresX += static_cast<double>(x[i]) * x[i];
        squaresY += static_cast<double>(y[i]) * y[i];
        result.peakX = std::max(result.peakX, std::abs(x[i]));
        result.peakY = std::max(result.peakY, std::abs(y[i]));
    }
    result.dcX = static_cast<float>(sumX / x.size());
    result.dcY = static_cast<float>(sumY / y.size());
    result.rmsX = std::sqrt(static_cast<float>(squaresX / x.size()));
    result.rmsY = std::sqrt(static_cast<float>(squaresY / y.size()));
    float confidenceX = 0.0f, confidenceY = 0.0f;
    result.frequencyX = estimateFrequency(x, sampleRate, confidenceX);
    result.frequencyY = estimateFrequency(y, sampleRate, confidenceY);
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
        int bestLag = 0;
        double bestCorrelation = -std::numeric_limits<double>::infinity();
        for (int lag = -period / 2; lag <= period / 2; ++lag) {
            double correlation = 0.0;
            for (size_t i = static_cast<size_t>(period); i + period < x.size(); ++i) {
                const int shifted = static_cast<int>(i) + lag;
                if (shifted >= 0 && shifted < static_cast<int>(y.size())) correlation += x[i] * y[shifted];
            }
            if (correlation > bestCorrelation) { bestCorrelation = correlation; bestLag = lag; }
        }
        result.phaseDegrees = 360.0f * bestLag / period;
        result.phaseValid = true;
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

    XYTraceBatch batch;
    batch.layerId = id;
    batch.firstFrame = input.firstFrame + begin;
    batch.sampleRate = input.sampleRate;
    batch.continuous = continuous;
    std::vector<float> conditionedX, conditionedY;
    conditionedX.reserve(end - begin);
    conditionedY.reserve(end - begin);

    float peak = 0.0f;
    struct Prepared { float x, y, z; bool invalid; };
    std::vector<Prepared> prepared;
    prepared.reserve(end - begin);
    for (size_t i = begin; i < end; ++i) {
        const auto& sample = input.samples[i];
        bool invalid = !std::isfinite(sample.x) || !std::isfinite(sample.y) || !std::isfinite(sample.z);
        float x = invalid ? 0.0f : conditionAxis(sample.x, settings.couplingX, settings.acCutoffHz, settings.bandwidthHz, input.sampleRate, runtime.xFilter);
        float y = invalid ? 0.0f : conditionAxis(sample.y, settings.couplingY, settings.acCutoffHz, settings.bandwidthHz, input.sampleRate, runtime.yFilter);
        conditionedX.push_back(x); conditionedY.push_back(y);
        peak = std::max({peak, std::abs(x), std::abs(y)});
        prepared.push_back({x, y, sample.z, invalid});
    }

    if (settings.autoGain && peak > 1e-4f) {
        const float target = std::min(0.9f / peak, 16.0f);
        const float seconds = static_cast<float>(prepared.size()) / std::max(input.sampleRate, 1u);
        const float rate = target < runtime.autoGain ? 12.0f : 2.5f;
        runtime.autoGain += (target - runtime.autoGain) * (1.0f - std::exp(-rate * seconds));
    } else if (!settings.autoGain) runtime.autoGain = 1.0f;

    const float angle = settings.rotationDegrees * Pi / 180.0f;
    const float cosine = std::cos(angle), sine = std::sin(angle);
    const float diagonal = std::sqrt(static_cast<float>(width * width + height * height));
    bool forceBreak = input.dropped || !runtime.hasLast;
    for (size_t i = 0; i < prepared.size(); ++i) {
        float x = prepared[i].x * settings.gainX * runtime.autoGain;
        float y = prepared[i].y * settings.gainY * runtime.autoGain;
        if (settings.invertX) x = -x;
        if (settings.invertY) y = -y;
        const float rotatedX = x * cosine - y * sine + settings.positionX;
        const float rotatedY = x * sine + y * cosine + settings.positionY;
        const float dxPixels = (rotatedX - runtime.lastX) * width * 0.5f;
        const float dyPixels = (rotatedY - runtime.lastY) * height * 0.5f;
        const float distancePixels = std::hypot(dxPixels, dyPixels);
        const bool jump = runtime.hasLast && distancePixels > settings.jumpBlanking * diagonal;
        const float dwell = 1.0f / (1.0f + distancePixels * 0.025f);
        float intensity = settings.beamIntensity * (1.0f + (dwell - 1.0f) * settings.dwellEffect);
        const float explicitZ = std::clamp(prepared[i].z * settings.zGain + settings.zOffset, 0.0f, 1.0f);
        if (settings.zMode == ZIntensityMode::Explicit) intensity = input.hasZ ? explicitZ * settings.beamIntensity : 0.0f;
        if (settings.zMode == ZIntensityMode::Multiply && input.hasZ) intensity *= explicitZ;

        const bool breakBefore = forceBreak || prepared[i].invalid || jump;
        const bool closeToPrevious = runtime.hasLast && distancePixels < 0.35f && !breakBefore;
        if (!closeToPrevious || i + 1 == prepared.size()) batch.points.push_back({rotatedX, rotatedY, intensity, breakBefore});
        runtime.lastX = rotatedX; runtime.lastY = rotatedY;
        runtime.hasLast = !prepared[i].invalid;
        forceBreak = prepared[i].invalid;
    }
    batch.measurements = measure(conditionedX, conditionedY, input.sampleRate);
    if (!continuous && !batch.points.empty()) runtime.heldSweep = batch;
    return batch;
}

void XYOscilloscopeEngine::reset(LayerId id) { m_runtime.erase(id); }
void XYOscilloscopeEngine::resetAll() { m_runtime.clear(); }
