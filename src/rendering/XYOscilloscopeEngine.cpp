#include "XYOscilloscopeEngine.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {
constexpr float Pi = 3.14159265358979323846f;
constexpr float Sqrt2 = 1.4142135623730950488f;

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
    float dcOffset,
    float peakAbs,
    std::uint32_t sampleRate,
    float& confidence,
    std::vector<float>& crossings
) {
    crossings.clear();
    if (values.size() < 4 || peakAbs < 1e-5f) {
        confidence = 0.0f;
        return 0.0f;
    }

    const float hyst = std::max(peakAbs * 0.05f, 1e-4f);
    bool armed = (values[0] - dcOffset) < -hyst;
    for (size_t i = 1; i < values.size(); ++i) {
        const float curr = values[i] - dcOffset;
        if (!armed) {
            if (curr < -hyst) armed = true;
            continue;
        }
        if (curr >= hyst) {
            size_t k = i;
            while (k > 1 && (values[k - 1] - dcOffset) >= 0.0f) {
                --k;
            }
            const float a = values[k - 1] - dcOffset;
            const float b = values[k] - dcOffset;
            const float denom = b - a;
            const float frac = denom > 1e-9f ? std::clamp(-a / denom, 0.0f, 1.0f) : 0.0f;
            crossings.push_back(static_cast<float>(k - 1) + frac);
            armed = false;
        }
    }

    if (crossings.size() < 3) {
        confidence = 0.0f;
        return 0.0f;
    }

    double mean = 0.0;
    for (size_t i = 1; i < crossings.size(); ++i) {
        mean += static_cast<double>(crossings[i] - crossings[i - 1]);
    }
    mean /= static_cast<double>(crossings.size() - 1);

    double variance = 0.0;
    for (size_t i = 1; i < crossings.size(); ++i) {
        const double difference = static_cast<double>(crossings[i] - crossings[i - 1]) - mean;
        variance += difference * difference;
    }
    variance /= static_cast<double>(crossings.size() - 1);

    confidence = static_cast<float>(
        std::clamp(1.0 - std::sqrt(variance) / std::max(mean, 1.0), 0.0, 1.0));
    return static_cast<float>(static_cast<double>(sampleRate) / std::max(mean, 1.0));
}

XYMeasurements measure(
    const std::vector<float>& x,
    const std::vector<float>& y,
    std::uint32_t sampleRate,
    std::vector<float>& crossingsScratch
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

    float acPeakX = 0.0f;
    float acPeakY = 0.0f;
    for (size_t i = 0; i < x.size(); ++i) {
        acPeakX = std::max(acPeakX, std::abs(x[i] - result.dcX));
        acPeakY = std::max(acPeakY, std::abs(y[i] - result.dcY));
    }

    float confidenceX = 0.0f, confidenceY = 0.0f;
    result.frequencyX = estimateFrequency(x, result.dcX, acPeakX, sampleRate, confidenceX, crossingsScratch);
    result.frequencyY = estimateFrequency(y, result.dcY, acPeakY, sampleRate, confidenceY, crossingsScratch);
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
        const double exactPeriod = static_cast<double>(sampleRate) / static_cast<double>(result.frequencyX);
        const int period = std::max(2, static_cast<int>(std::round(exactPeriod)));
        if (static_cast<size_t>(period) * 2 < x.size()) {
            const size_t iStart = static_cast<size_t>(period);
            const size_t iLimit = std::min(
                x.size() - static_cast<size_t>(period),
                iStart + static_cast<size_t>(period) * 4);
            const size_t sampleStride = std::max<size_t>(1, (iLimit - iStart) / 256);

            auto evalCorr = [&](int lag) {
                double correlation = 0.0;
                for (size_t i = iStart; i < iLimit; i += sampleStride) {
                    const double vx = static_cast<double>(x[i]) - static_cast<double>(result.dcX);
                    const double vy = static_cast<double>(
                        y[static_cast<size_t>(static_cast<int>(i) + lag)]) - static_cast<double>(result.dcY);
                    correlation += vx * vy;
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

            // Sub-sample parabolic refinement around the integer lag peak.
            double refinedLag = static_cast<double>(bestLag);
            if (bestLag > -halfPeriod && bestLag < halfPeriod) {
                const double cPrev = evalCorr(bestLag - 1);
                const double cMid = bestCorrelation;
                const double cNext = evalCorr(bestLag + 1);
                const double denom = cPrev - 2.0 * cMid + cNext;
                if (std::abs(denom) > 1e-12) {
                    const double delta = std::clamp(0.5 * (cPrev - cNext) / denom, -0.5, 0.5);
                    refinedLag += delta;
                }
            }

            result.phaseDegrees = static_cast<float>(360.0 * refinedLag / std::max(exactPeriod, 1.0));
            result.phaseValid = true;
        }
    }
    return result;
}

void evalCentripetalCatmullRom(
    float x0, float y0,
    float x1, float y1,
    float x2, float y2,
    float x3, float y3,
    float u,
    float& outX, float& outY
) {
    auto knotStep = [](float ax, float ay, float bx, float by) {
        const float d = std::hypot(bx - ax, by - ay);
        return std::max(std::sqrt(d), 1e-4f);
    };
    const float t0 = 0.0f;
    const float t1 = t0 + knotStep(x0, y0, x1, y1);
    const float t2 = t1 + knotStep(x1, y1, x2, y2);
    const float t3 = t2 + knotStep(x2, y2, x3, y3);
    const float t = t1 + u * (t2 - t1);

    auto lerp2 = [](float ax, float ay, float bx, float by, float ta, float tb, float tv, float& rx, float& ry) {
        const float w = (tv - ta) / std::max(tb - ta, 1e-6f);
        rx = ax + (bx - ax) * w;
        ry = ay + (by - ay) * w;
    };

    float a1x, a1y, a2x, a2y, a3x, a3y;
    lerp2(x0, y0, x1, y1, t0, t1, t, a1x, a1y);
    lerp2(x1, y1, x2, y2, t1, t2, t, a2x, a2y);
    lerp2(x2, y2, x3, y3, t2, t3, t, a3x, a3y);

    float b1x, b1y, b2x, b2y;
    lerp2(a1x, a1y, a2x, a2y, t0, t2, t, b1x, b1y);
    lerp2(a2x, a2y, a3x, a3y, t1, t3, t, b2x, b2y);

    lerp2(b1x, b1y, b2x, b2y, t1, t2, t, outX, outY);
}

void oversampleTrace(
    std::vector<XYTracePoint>& points,
    std::vector<XYTracePoint>& scratch,
    int factor,
    float scopeHalfExtent
) {
    if (factor <= 1 || points.size() < 2) return;
    scratch.clear();
    scratch.reserve(points.size() * static_cast<size_t>(factor));

    size_t runStart = 0;
    while (runStart < points.size()) {
        size_t runEnd = runStart + 1;
        while (runEnd < points.size() && !points[runEnd].breakBefore) {
            ++runEnd;
        }
        const size_t runLen = runEnd - runStart;
        if (runLen < 2) {
            scratch.push_back(points[runStart]);
            runStart = runEnd;
            continue;
        }

        scratch.push_back(points[runStart]);
        for (size_t k = 0; k + 1 < runLen; ++k) {
            const auto& p0 = points[runStart + (k > 0 ? k - 1 : 0)];
            const auto& p1 = points[runStart + k];
            const auto& p2 = points[runStart + k + 1];
            const auto& p3 = points[runStart + (k + 2 < runLen ? k + 2 : runLen - 1)];

            const float segPx = std::hypot(p2.x - p1.x, p2.y - p1.y) * scopeHalfExtent;
            if (segPx >= 0.75f) {
                for (int step = 1; step < factor; ++step) {
                    const float u = static_cast<float>(step) / static_cast<float>(factor);
                    float cx = 0.0f, cy = 0.0f;
                    evalCentripetalCatmullRom(
                        p0.x, p0.y, p1.x, p1.y, p2.x, p2.y, p3.x, p3.y, u, cx, cy);
                    const float ci = p1.intensity + (p2.intensity - p1.intensity) * u;
                    scratch.push_back({cx, cy, ci, false});
                }
            }
            scratch.push_back({p2.x, p2.y, p2.intensity, false});
        }
        runStart = runEnd;
    }

    points.swap(scratch);
}
} // namespace

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

    const AxisFilterCoeffs coeffsX = makeFilterCoeffs(
        settings.couplingX, settings.acCutoffHz, settings.bandwidthHz, input.sampleRate);
    const AxisFilterCoeffs coeffsY = makeFilterCoeffs(
        settings.couplingY, settings.acCutoffHz, settings.bandwidthHz, input.sampleRate);

    const size_t requestedFrames = std::max<size_t>(2, static_cast<size_t>(2048.0f * settings.windowScale));
    size_t begin = 0;
    float triggerFrac = 0.0f;

    m_conditionedX.clear();
    m_conditionedY.clear();
    m_prepared.clear();

    float peak = 0.0f;
    if (continuous) {
        const size_t sampleCount = input.samples.size();
        m_conditionedX.reserve(sampleCount);
        m_conditionedY.reserve(sampleCount);
        m_prepared.reserve(sampleCount);

        for (size_t i = 0; i < sampleCount; ++i) {
            const auto& sample = input.samples[i];
            const bool invalid = !std::isfinite(sample.x) || !std::isfinite(sample.y) || !std::isfinite(sample.z);
            const float x = invalid ? 0.0f : applyAxisFilter(sample.x, coeffsX, runtime.xFilter);
            const float y = invalid ? 0.0f : applyAxisFilter(sample.y, coeffsY, runtime.yFilter);
            m_conditionedX.push_back(x);
            m_conditionedY.push_back(y);
            peak = std::max({peak, std::abs(x), std::abs(y)});
            m_prepared.push_back({x, y, sample.z, invalid});
        }
    } else {
        // Triggered snapshot mode: condition the full snapshot using a local,
        // DC-initialized filter so overlapping snapshots never corrupt persistent
        // IIR state and trigger detection operates on conditioned (AC/BW) signals.
        const size_t total = input.samples.size();
        m_triggerScratch.clear();
        m_triggerScratch.reserve(total);

        AxisFilter localX;
        AxisFilter localY;
        if (total > 0) {
            double meanX = 0.0, meanY = 0.0;
            size_t validCount = 0;
            for (const auto& s : input.samples) {
                if (std::isfinite(s.x) && std::isfinite(s.y)) {
                    meanX += s.x;
                    meanY += s.y;
                    ++validCount;
                }
            }
            if (validCount > 0) {
                const float dcX = static_cast<float>(meanX / static_cast<double>(validCount));
                const float dcY = static_cast<float>(meanY / static_cast<double>(validCount));
                if (coeffsX.acCoupled) localX.dcEstimate = dcX;
                if (coeffsY.acCoupled) localY.dcEstimate = dcY;
            }
            const float firstX = std::isfinite(input.samples[0].x)
                ? (coeffsX.acCoupled ? input.samples[0].x - localX.dcEstimate : input.samples[0].x)
                : 0.0f;
            const float firstY = std::isfinite(input.samples[0].y)
                ? (coeffsY.acCoupled ? input.samples[0].y - localY.dcEstimate : input.samples[0].y)
                : 0.0f;
            localX.x1 = localX.x2 = localX.y1 = localX.y2 = firstX;
            localY.x1 = localY.x2 = localY.y1 = localY.y2 = firstY;
        }

        for (size_t i = 0; i < total; ++i) {
            const auto& sample = input.samples[i];
            const bool invalid = !std::isfinite(sample.x) || !std::isfinite(sample.y) || !std::isfinite(sample.z);
            const float x = invalid ? 0.0f : applyAxisFilter(sample.x, coeffsX, localX);
            const float y = invalid ? 0.0f : applyAxisFilter(sample.y, coeffsY, localY);
            m_triggerScratch.push_back({x, y, sample.z, invalid});
        }

        const std::uint64_t holdoffSamples = static_cast<std::uint64_t>(
            std::max(0.0f, settings.triggerHoldoffMs) * 0.001f *
            static_cast<float>(std::max(input.sampleRate, 1u)));
        if (runtime.hasTriggerFrame &&
            input.firstFrame + static_cast<std::uint64_t>(total) <= runtime.lastTriggerFrame) {
            // Audio timeline restarted or rewound.
            runtime.hasTriggerFrame = false;
        }

        size_t searchStart = 0;
        bool holdoffActive = false;
        if (holdoffSamples > 0 && runtime.hasTriggerFrame) {
            const std::uint64_t earliestAllowed = runtime.lastTriggerFrame + holdoffSamples;
            if (input.firstFrame + static_cast<std::uint64_t>(total) <= earliestAllowed) {
                holdoffActive = true;
            } else if (earliestAllowed > input.firstFrame) {
                searchStart = static_cast<size_t>(earliestAllowed - input.firstFrame);
            }
        }

        if (holdoffActive && !runtime.heldSweep.points.empty()) {
            return runtime.heldSweep;
        }

        bool found = false;
        if (total > 1 && searchStart + 1 < total) {
            const auto triggerValue = [&](size_t i) {
                return settings.triggerSource == TriggerSource::X
                    ? m_triggerScratch[i].x
                    : m_triggerScratch[i].y;
            };
            const float low = settings.triggerLevel - settings.triggerHysteresis;
            const float high = settings.triggerLevel + settings.triggerHysteresis;
            const size_t searchEnd = total > requestedFrames
                ? std::max(searchStart + 2, total - requestedFrames + 1)
                : total;
            bool armed = settings.triggerEdge == TriggerEdge::Rising
                ? triggerValue(searchStart) < low
                : triggerValue(searchStart) > high;

            for (size_t i = searchStart + 1; i < searchEnd; ++i) {
                const float value = triggerValue(i);
                if (settings.triggerEdge == TriggerEdge::Rising) {
                    if (!armed) {
                        if (value < low) armed = true;
                        continue;
                    }
                    if (value >= high) {
                        size_t k = i;
                        while (k > searchStart + 1 && triggerValue(k - 1) >= settings.triggerLevel) {
                            --k;
                        }
                        const float v0 = triggerValue(k - 1);
                        const float v1 = triggerValue(k);
                        const float denom = v1 - v0;
                        triggerFrac = denom > 1e-9f
                            ? std::clamp((settings.triggerLevel - v0) / denom, 0.0f, 1.0f)
                            : 0.0f;
                        begin = k - 1;
                        found = true;
                        break;
                    }
                } else {
                    if (!armed) {
                        if (value > high) armed = true;
                        continue;
                    }
                    if (value <= low) {
                        size_t k = i;
                        while (k > searchStart + 1 && triggerValue(k - 1) <= settings.triggerLevel) {
                            --k;
                        }
                        const float v0 = triggerValue(k - 1);
                        const float v1 = triggerValue(k);
                        const float denom = v0 - v1;
                        triggerFrac = denom > 1e-9f
                            ? std::clamp((v0 - settings.triggerLevel) / denom, 0.0f, 1.0f)
                            : 0.0f;
                        begin = k - 1;
                        found = true;
                        break;
                    }
                }
            }

            if (found) {
                runtime.lastTriggerFrame = input.firstFrame + static_cast<std::uint64_t>(begin);
                runtime.hasTriggerFrame = true;
            } else if (settings.triggerMode == TriggerMode::Normal && !runtime.heldSweep.points.empty()) {
                return runtime.heldSweep;
            }
        }

        const size_t end = std::min(total, begin + requestedFrames);
        const size_t sampleCount = end > begin ? (end - begin) : 0;
        m_conditionedX.reserve(sampleCount);
        m_conditionedY.reserve(sampleCount);
        m_prepared.reserve(sampleCount);

        for (size_t i = begin; i < end; ++i) {
            const auto& s0 = m_triggerScratch[i];
            float x = s0.x;
            float y = s0.y;
            float z = s0.z;
            bool invalid = s0.invalid;
            if (found && triggerFrac > 0.0f && i + 1 < total && !m_triggerScratch[i + 1].invalid && !s0.invalid) {
                const auto& s1 = m_triggerScratch[i + 1];
                x = s0.x + (s1.x - s0.x) * triggerFrac;
                y = s0.y + (s1.y - s0.y) * triggerFrac;
                z = s0.z + (s1.z - s0.z) * triggerFrac;
            }
            m_conditionedX.push_back(x);
            m_conditionedY.push_back(y);
            peak = std::max({peak, std::abs(x), std::abs(y)});
            m_prepared.push_back({x, y, z, invalid});
        }
    }

    XYTraceBatch batch;
    batch.layerId = id;
    batch.firstFrame = input.firstFrame + begin;
    batch.sampleRate = input.sampleRate;
    batch.continuous = continuous;

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
    // Use the isotropic square scope extent matching renderGrid / renderTrace on both
    // landscape and portrait viewports so dwell brightness & jump blanking are axis-invariant.
    const float scopeHalfExtent = 0.5f * static_cast<float>(std::max(1, std::min(width, height)));
    const float scopeDiagonal = 2.0f * scopeHalfExtent * Sqrt2;
    const float jumpLimit = settings.jumpBlanking * scopeDiagonal;
    const float scaleX = settings.gainX * runtime.autoGain * (settings.invertX ? -1.0f : 1.0f);
    const float scaleY = settings.gainY * runtime.autoGain * (settings.invertY ? -1.0f : 1.0f);
    const float rateF = static_cast<float>(std::max(input.sampleRate, 1u));

    batch.points.reserve(m_prepared.size() + 1);
    bool forceBreak = !continuous || input.dropped || !runtime.hasLast;
    float lastEmittedX = 0.0f;
    float lastEmittedY = 0.0f;
    bool hasEmitted = false;
    bool bridgedFromPrev = false;

    for (size_t i = 0; i < m_prepared.size(); ++i) {
        const auto& prep = m_prepared[i];
        const float x = prep.x * scaleX;
        const float y = prep.y * scaleY;
        const float rotatedX = x * cosine - y * sine + settings.positionX;
        const float rotatedY = x * sine + y * cosine + settings.positionY;

        const bool havePrev = (continuous || i > 0) && runtime.hasLast;
        const float dxScope = havePrev ? (rotatedX - runtime.lastX) : 0.0f;
        const float dyScope = havePrev ? (rotatedY - runtime.lastY) : 0.0f;
        const float distScope = std::hypot(dxScope, dyScope);
        const float distancePixels = distScope * scopeHalfExtent;
        const bool jump = havePrev && distancePixels > jumpLimit;

        // Sample-rate- and resolution-invariant beam speed (scope units per second).
        const float normalizedSpeed = (distScope * rateF) / 3600.0f;
        const float dwell = 1.0f / (1.0f + normalizedSpeed);
        float intensity = settings.beamIntensity * (1.0f + (dwell - 1.0f) * settings.dwellEffect);
        if (settings.densityEffect > 0.001f) {
            intensity *= (1.0f + settings.densityEffect * dwell * 1.25f);
        }
        const float explicitZ = std::clamp(prep.z * settings.zGain + settings.zOffset, 0.0f, 1.0f);
        if (settings.zMode == ZIntensityMode::Explicit) {
            intensity = input.hasZ ? explicitZ * settings.beamIntensity : 0.0f;
        } else if (settings.zMode == ZIntensityMode::Multiply && input.hasZ) {
            intensity *= explicitZ;
        }

        const bool breakBefore = forceBreak || prep.invalid || jump;

        // Bridge seamlessly from the previous frame's final point in continuous mode so
        // frame boundaries never leave 1-sample micro-gaps.
        if (i == 0 && continuous && !breakBefore && runtime.hasLast && !bridgedFromPrev) {
            batch.points.push_back({runtime.lastX, runtime.lastY, runtime.lastIntensity, true});
            lastEmittedX = runtime.lastX;
            lastEmittedY = runtime.lastY;
            hasEmitted = true;
            bridgedFromPrev = true;
        }

        // Measure decimation distance against the LAST EMITTED point (not the previous
        // skipped sample) so slow curves at 96/192 kHz never collapse into straight chords.
        const float distFromEmittedPx = hasEmitted
            ? std::hypot(rotatedX - lastEmittedX, rotatedY - lastEmittedY) * scopeHalfExtent
            : 1e9f;
        const bool closeToEmitted = hasEmitted && !breakBefore && distFromEmittedPx < 0.25f;

        if (!closeToEmitted || i + 1 == m_prepared.size()) {
            if (!prep.invalid) {
                batch.points.push_back({rotatedX, rotatedY, intensity, breakBefore});
                lastEmittedX = rotatedX;
                lastEmittedY = rotatedY;
                hasEmitted = true;
            }
        } else if (!batch.points.empty()) {
            // Keep peak dwell intensity on the held point while stationary.
            batch.points.back().intensity = std::max(batch.points.back().intensity, intensity);
        }

        runtime.lastX = rotatedX;
        runtime.lastY = rotatedY;
        runtime.lastIntensity = intensity;
        runtime.hasLast = !prep.invalid;
        forceBreak = prep.invalid;
    }

    if (settings.oversampling > 1) {
        const int factor = settings.oversampling >= 4 ? 4 : 2;
        oversampleTrace(batch.points, m_oversampleScratch, factor, scopeHalfExtent);
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
