#pragma once

#include <cstdint>
#include <vector>

using LayerId = std::uint64_t;

enum class ScopeProfile { Custom, Analog, Stylized };
enum class CouplingMode { DC, AC };
enum class TriggerMode { Auto, Normal };
enum class TriggerSource { X, Y };
enum class TriggerEdge { Rising, Falling };
enum class ZIntensityMode { Derived, Explicit, Multiply };

struct XYInputSample {
    float x = 0.0f;
    float y = 0.0f;
    float z = 1.0f;
};

struct XYInputChunk {
    std::uint64_t firstFrame = 0;
    std::uint32_t sampleRate = 48000;
    std::uint64_t discontinuityGeneration = 0;
    bool hasZ = false;
    bool dropped = false;
    std::vector<XYInputSample> samples;
};

struct XYTracePoint {
    float x = 0.0f;
    float y = 0.0f;
    float intensity = 1.0f;
    bool breakBefore = false;
};

struct XYMeasurements {
    float frequencyX = 0.0f;
    float frequencyY = 0.0f;
    float phaseDegrees = 0.0f;
    float peakX = 0.0f;
    float peakY = 0.0f;
    float rmsX = 0.0f;
    float rmsY = 0.0f;
    float dcX = 0.0f;
    float dcY = 0.0f;
    int ratioNumerator = 0;
    int ratioDenominator = 0;
    float confidence = 0.0f;
    bool phaseValid = false;
};

struct XYTraceBatch {
    LayerId layerId = 0;
    std::uint64_t firstFrame = 0;
    std::uint32_t sampleRate = 48000;
    bool continuous = false;
    std::vector<XYTracePoint> points;
    XYMeasurements measurements;
};

struct XYLayerSettings {
    ScopeProfile profile = ScopeProfile::Custom;
    bool persistence = true;
    float windowScale = 1.0f;

    CouplingMode couplingX = CouplingMode::DC;
    CouplingMode couplingY = CouplingMode::DC;
    float acCutoffHz = 5.0f;
    float bandwidthHz = 20000.0f;
    float gainX = 1.0f;
    float gainY = 1.0f;
    float positionX = 0.0f;
    float positionY = 0.0f;
    bool invertX = false;
    bool invertY = false;
    float rotationDegrees = 0.0f;
    bool autoGain = false;

    TriggerMode triggerMode = TriggerMode::Auto;
    TriggerSource triggerSource = TriggerSource::X;
    TriggerEdge triggerEdge = TriggerEdge::Rising;
    float triggerLevel = 0.0f;
    float triggerHysteresis = 0.02f;
    float triggerHoldoffMs = 0.0f;
    float jumpBlanking = 0.35f;

    float traceWidth = 2.0f;
    float bloom = 1.0f;
    float beamHeadSize = 0.0f;
    float beamIntensity = 1.0f;
    float dwellEffect = 0.0f;
    float densityEffect = 0.0f;
    ZIntensityMode zMode = ZIntensityMode::Derived;
    float zGain = 1.0f;
    float zOffset = 0.0f;
};

struct OscilloscopeDisplaySettings {
    ScopeProfile profile = ScopeProfile::Custom;
    bool graticuleEnabled = true;
    float graticuleOpacity = 0.18f;
    int graticuleColumns = 10;
    int graticuleRows = 8;
    float phosphorFastDecayMs = 65.0f;
    float phosphorSlowDecayMs = 500.0f;
    float phosphorSlowWeight = 0.25f;
    float phosphorSaturation = 1.0f;
    float decayColorShift = 0.08f;
    bool measurementOverlay = false;
    bool overlayInVideo = false;
};
