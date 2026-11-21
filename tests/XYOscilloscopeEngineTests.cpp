#include "XYOscilloscopeEngine.hpp"

#include <cmath>
#include <iostream>

namespace {
constexpr float Pi = 3.14159265358979323846f;

XYInputChunk sineChunk(
    float frequencyX,
    float frequencyY,
    size_t count = 4800,
    std::uint32_t sampleRate = 48000,
    float phaseYRad = 0.0f,
    float dcX = 0.0f,
    float dcY = 0.0f,
    std::uint64_t firstFrame = 0
) {
    XYInputChunk chunk;
    chunk.firstFrame = firstFrame;
    chunk.sampleRate = sampleRate;
    chunk.discontinuityGeneration = 1;
    chunk.samples.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        const double time = static_cast<double>(firstFrame + i) / static_cast<double>(sampleRate);
        chunk.samples.push_back({
            dcX + static_cast<float>(std::sin(2.0 * Pi * frequencyX * time)),
            dcY + static_cast<float>(std::sin(2.0 * Pi * frequencyY * time + phaseYRad)),
            1.0f
        });
    }
    return chunk;
}

bool near(float actual, float expected, float tolerance) {
    return std::abs(actual - expected) <= tolerance;
}

int fail(const char* message) {
    std::cerr << "FAIL: " << message << "\n";
    return 1;
}
} // namespace

int main() {
    // 1. Basic continuous acquisition, frequency, ratio, and dropped discontinuity
    {
        XYOscilloscopeEngine engine;
        XYLayerSettings settings;
        settings.bandwidthHz = 12000.0f;

        auto input = sineChunk(1000.0f, 500.0f, 4800, 48000, 0.0f, 0.0f, 0.0f, 0);
        const auto first = engine.processContinuous(11, settings, input, 1920, 1080);
        if (first.points.empty() || !first.points.front().breakBefore) {
            return fail("Continuous acquisition did not start with a blanked beam");
        }
        if (!near(first.measurements.frequencyX, 1000.0f, 10.0f) ||
            !near(first.measurements.frequencyY, 500.0f, 8.0f)) {
            return fail("Frequency measurement is inaccurate");
        }
        if (first.measurements.ratioNumerator != 2 || first.measurements.ratioDenominator != 1) {
            return fail("Frequency ratio was not reduced to 2:1");
        }

        // Consecutive non-dropped frame must bridge seamlessly from the previous endpoint.
        auto secondInput = sineChunk(1000.0f, 500.0f, 800, 48000, 0.0f, 0.0f, 0.0f, 4800);
        const auto bridged = engine.processContinuous(11, settings, secondInput, 1920, 1080);
        if (bridged.points.size() < 2) {
            return fail("Consecutive continuous chunk produced too few points");
        }
        if (!near(bridged.points[0].x, first.points.back().x, 1e-4f) ||
            !near(bridged.points[0].y, first.points.back().y, 1e-4f)) {
            return fail("Consecutive continuous chunk did not bridge from previous frame endpoint");
        }
        if (bridged.points[1].breakBefore) {
            return fail("Bridged continuous chunk unexpectedly broke before first new sample");
        }

        // Dropped input must blank before re-entering.
        secondInput.dropped = true;
        secondInput.firstFrame = 9600;
        const auto discontinuous = engine.processContinuous(11, settings, secondInput, 1920, 1080);
        if (discontinuous.points.empty() || !discontinuous.points.front().breakBefore) {
            return fail("Dropped input was connected to the previous trace");
        }
    }

    // 2. Slow-beam arc at high sample rate must NOT collapse into a chord
    {
        XYOscilloscopeEngine engine;
        XYLayerSettings settings;
        settings.bandwidthHz = 20000.0f;
        // 5 Hz quarter-circle at 96 kHz: each step is ~0.18 px on 1080p (< 0.25 px),
        // so old lastX-based decimation collapsed the entire arc to 2 points!
        auto slowArc = sineChunk(5.0f, 5.0f, 4800, 96000, Pi * 0.5f);
        const auto trace = engine.processContinuous(21, settings, slowArc, 1920, 1080);
        if (trace.points.size() < 50) {
            return fail("Slow high-sample-rate arc collapsed instead of retaining intermediate points");
        }
    }

    // 3. Stationary beam spot must not vanish and must retain full dwell intensity
    {
        XYOscilloscopeEngine engine;
        XYLayerSettings settings;
        settings.dwellEffect = 1.0f;
        settings.beamIntensity = 1.5f;
        XYInputChunk dcDot;
        dcDot.sampleRate = 48000;
        dcDot.discontinuityGeneration = 1;
        dcDot.samples.assign(800, {0.25f, -0.35f, 1.0f});
        const auto trace = engine.processContinuous(22, settings, dcDot, 1920, 1080);
        if (trace.points.empty()) {
            return fail("Stationary CRT spot vanished");
        }
        if (!near(trace.points.back().intensity, 1.5f, 0.05f)) {
            return fail("Stationary CRT spot did not retain peak dwell intensity");
        }
    }

    // 4. Isotropic dwell across horizontal vs vertical motion & landscape vs portrait viewports
    {
        XYOscilloscopeEngine engine;
        XYLayerSettings settings;
        settings.dwellEffect = 1.0f;
        settings.beamIntensity = 1.0f;
        settings.bandwidthHz = 20000.0f;

        auto horiz = sineChunk(400.0f, 0.0f, 1600, 48000);
        auto vert = sineChunk(0.0f, 400.0f, 1600, 48000);
        const auto traceH = engine.processContinuous(31, settings, horiz, 1920, 1080);
        const auto traceV = engine.processContinuous(32, settings, vert, 1920, 1080);
        const auto tracePortrait = engine.processContinuous(33, settings, horiz, 1080, 1920);
        auto meanIntensity = [](const XYTraceBatch& b) {
            double s = 0.0;
            for (size_t i = 20; i < b.points.size(); ++i) s += b.points[i].intensity;
            return static_cast<float>(s / static_cast<double>(std::max<size_t>(1, b.points.size() - 20)));
        };
        const float iH = meanIntensity(traceH);
        const float iV = meanIntensity(traceV);
        const float iP = meanIntensity(tracePortrait);
        if (!near(iH, iV, 0.03f) || !near(iH, iP, 0.03f)) {
            return fail("Dwell intensity is not isotropic across axes / landscape vs portrait");
        }
    }

    // 5. Sample-rate-invariant dwell (48 kHz vs 192 kHz same 600 Hz circle)
    {
        XYOscilloscopeEngine engine;
        XYLayerSettings settings;
        settings.dwellEffect = 1.0f;
        settings.beamIntensity = 1.0f;
        settings.bandwidthHz = 20000.0f;

        auto circle48 = sineChunk(600.0f, 600.0f, 1600, 48000, Pi * 0.5f);
        auto circle192 = sineChunk(600.0f, 600.0f, 6400, 192000, Pi * 0.5f);
        const auto t48 = engine.processContinuous(41, settings, circle48, 1920, 1080);
        const auto t192 = engine.processContinuous(42, settings, circle192, 1920, 1080);
        auto meanI = [](const XYTraceBatch& b) {
            double s = 0.0;
            const size_t start = b.points.size() / 4;
            for (size_t i = start; i < b.points.size(); ++i) s += b.points[i].intensity;
            return static_cast<float>(s / static_cast<double>(std::max<size_t>(1, b.points.size() - start)));
        };
        if (!near(meanI(t48), meanI(t192), 0.06f)) {
            return fail("Dwell brightness drifted across 48 kHz vs 192 kHz sample rates");
        }
    }

    // 6. Triggered mode: AC coupling before trigger, Normal hold, sub-sample stability & holdoff
    {
        XYOscilloscopeEngine engine;
        XYLayerSettings triggered;
        triggered.persistence = false;
        triggered.couplingX = CouplingMode::AC;
        triggered.couplingY = CouplingMode::AC;
        triggered.triggerMode = TriggerMode::Normal;
        triggered.triggerLevel = 0.0f;
        triggered.triggerHysteresis = 0.05f;
        triggered.bandwidthHz = 12000.0f;

        // Sine wave riding on +0.75 DC offset (raw signal never crosses 0.0, only AC-coupled does!)
        auto dcBiased = sineChunk(1000.0f, 1000.0f, 4096, 48000, Pi * 0.5f, 0.75f, -0.5f, 0);
        const auto sweep = engine.processTriggered(51, triggered, dcBiased, 1280, 720);
        if (sweep.points.empty()) {
            return fail("AC-coupled triggered acquisition failed to trigger on DC-biased signal");
        }
        // Sub-sample interpolation should place the initial point right on triggerLevel (0.0).
        if (!near(sweep.points.front().x, 0.0f, 0.03f)) {
            return fail("Sub-sample trigger interpolation did not lock near triggerLevel");
        }

        // Normal mode holds last sweep when no crossing exists.
        XYInputChunk noTrigger;
        noTrigger.firstFrame = 4096;
        noTrigger.sampleRate = 48000;
        noTrigger.discontinuityGeneration = 1;
        noTrigger.samples.assign(400, {0.8f, 0.0f, 1.0f});
        const auto held = engine.processTriggered(51, triggered, noTrigger, 1280, 720);
        if (held.points.size() != sweep.points.size()) {
            return fail("Normal trigger mode did not retain its last valid sweep");
        }

        // Trigger holdoff: 100 ms = 4800 frames; a chunk inside holdoff window must hold previous sweep.
        triggered.triggerHoldoffMs = 100.0f;
        engine.reset(52);
        auto chunkA = sineChunk(500.0f, 500.0f, 2048, 48000, 0.0f, 0.0f, 0.0f, 0);
        const auto sweepA = engine.processTriggered(52, triggered, chunkA, 1280, 720);
        auto chunkB = sineChunk(250.0f, 250.0f, 2048, 48000, 0.0f, 0.0f, 0.0f, 1000);
        const auto heldB = engine.processTriggered(52, triggered, chunkB, 1280, 720);
        if (heldB.firstFrame != sweepA.firstFrame) {
            return fail("Trigger holdoff did not hold sweep while within holdoff duration");
        }
        auto chunkC = sineChunk(250.0f, 250.0f, 4096, 48000, 0.0f, 0.0f, 0.0f, 6000);
        const auto sweepC = engine.processTriggered(52, triggered, chunkC, 1280, 720);
        if (sweepC.firstFrame < 6000) {
            return fail("Trigger holdoff failed to re-arm after holdoff duration elapsed");
        }
    }

    // 7. DC-biased frequency & phase measurement accuracy
    {
        XYOscilloscopeEngine engine;
        XYLayerSettings settings;
        settings.couplingX = CouplingMode::DC;
        settings.couplingY = CouplingMode::DC;
        settings.bandwidthHz = 20000.0f;
        // 1000 Hz X and Y with +90 deg lead on Y (sin(wt + pi/2) = cos(wt)) and DC offsets
        auto biased = sineChunk(1000.0f, 1000.0f, 4800, 48000, Pi * 0.5f, 0.4f, -0.35f);
        const auto trace = engine.processContinuous(61, settings, biased, 1920, 1080);
        if (!near(trace.measurements.frequencyX, 1000.0f, 3.0f) ||
            !near(trace.measurements.frequencyY, 1000.0f, 3.0f)) {
            return fail("DC-biased sub-sample frequency measurement was inaccurate");
        }
        if (!trace.measurements.phaseValid || !near(std::abs(trace.measurements.phaseDegrees), 90.0f, 2.0f)) {
            return fail("DC-biased parabolic phase measurement was inaccurate");
        }
    }

    // 8. Centripetal Catmull-Rom oversampling smooths 44.1 kHz curves without bridging blanked jumps
    {
        XYOscilloscopeEngine engine;
        XYLayerSettings base;
        base.bandwidthHz = 20000.0f;
        base.oversampling = 1;
        base.jumpBlanking = 0.25f;

        // 4 kHz circle at 44.1 kHz (~11 samples/cycle -> coarse polygon at 1x)
        auto coarse = sineChunk(4000.0f, 4000.0f, 441, 44100, Pi * 0.5f);
        // Inject a deliberate blanked jump in the middle
        for (size_t i = 220; i < coarse.samples.size(); ++i) {
            coarse.samples[i].x *= 0.2f;
            coarse.samples[i].y *= 0.2f;
        }
        const auto trace1x = engine.processContinuous(71, base, coarse, 1920, 1080);

        XYLayerSettings smooth = base;
        smooth.oversampling = 4;
        const auto trace4x = engine.processContinuous(72, smooth, coarse, 1920, 1080);
        if (trace4x.points.size() < trace1x.points.size() * 2) {
            return fail("4x oversampling did not subdivide coarse high-frequency curves");
        }
        // Verify blanked jump break count was preserved (oversampling must never bridge blanked jumps)
        auto countBreaks = [](const XYTraceBatch& b) {
            size_t n = 0;
            for (const auto& p : b.points) { if (p.breakBefore) ++n; }
            return n;
        };
        if (countBreaks(trace4x) != countBreaks(trace1x)) {
            return fail("Oversampling altered jump-blanking break count");
        }
        // Verify subdivided points on the first unit circle stay near radius ~1.0 (no overshoot/ringing)
        for (size_t i = 20; i < 200 && i < trace4x.points.size(); ++i) {
            if (trace4x.points[i].breakBefore) break;
            const float rad = std::hypot(trace4x.points[i].x, trace4x.points[i].y);
            if (rad < 0.85f || rad > 1.15f) {
                return fail("Centripetal Catmull-Rom oversampling produced unexpected overshoot on circle");
            }
        }
    }

    return 0;
}
