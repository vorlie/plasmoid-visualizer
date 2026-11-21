#include "OscMusicEditor.hpp"
#include "tinyexpr.h"
#include <algorithm>
#include <cmath>

void OscMusicEditor::TeExprDeleter::operator()(te_expr* expr) const noexcept {
    if (expr) te_free(expr);
}

static double te_pi = 3.14159265358979323846;
static double te_tau = 6.28318530717958647692;
static double te_phi = 1.61803398874989484820;

static double te_clamp(double x, double min, double max) { return x < min ? min : (x > max ? max : x); }
static double te_step(double edge, double x) { return x < edge ? 0.0 : 1.0; }
static double te_lerp(double a, double b, double t) { return a + t * (b - a); }

static double te_noise(double t) {
    auto hash = [](int x) {
        x = ((x >> 16) ^ x) * 0x45d9f3b;
        x = ((x >> 16) ^ x) * 0x45d9f3b;
        x = (x >> 16) ^ x;
        return x;
    };
    int i = (int)floor(t);
    double f = t - i;
    double h1 = (double)(hash(i) & 0x7FFFFFFF) / (double)0x7FFFFFFF;
    double h2 = (double)(hash(i+1) & 0x7FFFFFFF) / (double)0x7FFFFFFF;
    return h1 + f * f * (3.0 - 2.0 * f) * (h2 - h1);
}

static double te_sin_hz(double f, double t) { return sin(f * t * te_tau); }
static double te_sqr_hz(double f, double t) { return (sin(f * t * te_tau) >= 0) ? 1.0 : -1.0; }
static double te_tri_hz(double f, double t) { return 2.0 * std::abs(2.0 * (f * t - std::floor(f * t + 0.5))) - 1.0; }
static double te_saw_hz(double f, double t) { double p = f * t; return 2.0 * (p - std::floor(p + 0.5)); }

OscMusicEditor::OscMusicEditor()
    : m_xExpr("sin(t * 2 * 3.14159)"),
      m_yExpr("cos(t * 2 * 3.14159)"),
      m_zExpr("1"),
      m_isValid(true) {
    initializePresets();
    validateExpressions(m_lastError);
}

OscMusicEditor::~OscMusicEditor() = default;

void OscMusicEditor::cleanupExpressions() {
    m_xParsed.reset();
    m_yParsed.reset();
    m_zParsed.reset();
}

void OscMusicEditor::initializePresets() {
    m_presets = {
        {"Circle", "sin(f * t * 2 * pi)", "cos(f * t * 2 * pi)", 2.0f},
        {"Lissajous 3:4", "sin(f * t * 3 * tau)", "sin(f * t * 4 * tau)", 2.0f},
        {"Spirograph", "(1-0.6)*cos(f*t) + 0.3*cos((1-0.6)/0.6*f*t)", "(1-0.6)*sin(f*t) - 0.3*sin((1-0.6)/0.6*f*t)", 2.0f},
        {"Butterfly", "sin(f*t)*(exp(cos(f*t)) - 2*cos(4*f*t) - pow(sin(f*t/12), 5))", 
                     "cos(f*t)*(exp(cos(f*t)) - 2*cos(4*f*t) - pow(sin(f*t/12), 5))", 4.0f},
        {"Noise Blob", "noise(f*t) * cos(t*tau)", "noise(f*t+10) * sin(t*tau)", 2.0f},
        {"Heart", "16 * pow(sin(f*t), 3)", 
                 "13*cos(f*t) - 5*cos(2*f*t) - 2*cos(3*f*t) - cos(4*f*t)", 2.0f},
        {"Harmonograph", "exp(-0.1*t)*sin(f*t*tau) + exp(-0.1*t)*sin(f*1.01*t*tau)", 
                         "exp(-0.1*t)*cos(f*t*tau) + exp(-0.1*t)*cos(f*0.99*t*tau)", 2.0f}
    };
}

bool OscMusicEditor::setExpressions(const std::string& xExpr, const std::string& yExpr) {
    return setExpressions(xExpr, yExpr, "1");
}

bool OscMusicEditor::setExpressions(const std::string& xExpr, const std::string& yExpr, const std::string& zExpr) {
    m_xExpr = xExpr;
    m_yExpr = yExpr;
    m_zExpr = zExpr;
    return validateExpressions(m_lastError);
}

bool OscMusicEditor::validateExpressions(std::string& errorMsg) {
    cleanupExpressions();
    
    m_evalT = 0.0;
    m_evalF = (double)m_baseFreq;
    te_variable vars[] = {
        {"t", &m_evalT, TE_VARIABLE},
        {"f", &m_evalF, TE_VARIABLE},
        {"pi", &te_pi, TE_VARIABLE},
        {"tau", &te_tau, TE_VARIABLE},
        {"phi", &te_phi, TE_VARIABLE},
        {"noise", (const void*)te_noise, TE_FUNCTION1},
        {"clamp", (const void*)te_clamp, TE_FUNCTION3},
        {"step", (const void*)te_step, TE_FUNCTION2},
        {"lerp", (const void*)te_lerp, TE_FUNCTION3},
        {"sin_hz", (const void*)te_sin_hz, TE_FUNCTION2},
        {"sqr_hz", (const void*)te_sqr_hz, TE_FUNCTION2},
        {"tri_hz", (const void*)te_tri_hz, TE_FUNCTION2},
        {"saw_hz", (const void*)te_saw_hz, TE_FUNCTION2}
    };
    
    int xErr = 0, yErr = 0, zErr = 0;
    m_xParsed.reset(te_compile(m_xExpr.c_str(), vars, 13, &xErr));
    m_yParsed.reset(te_compile(m_yExpr.c_str(), vars, 13, &yErr));
    m_zParsed.reset(te_compile(m_zExpr.c_str(), vars, 13, &zErr));

    if (!m_xParsed) {
        errorMsg = "X expression error at position " + std::to_string(xErr);
        m_isValid = false;
        cleanupExpressions();
        return false;
    }
    if (!m_zParsed) {
        errorMsg = "Z expression error at position " + std::to_string(zErr);
        m_isValid = false;
        cleanupExpressions();
        return false;
    }

    if (!m_yParsed) {
        errorMsg = "Y expression error at position " + std::to_string(yErr);
        m_isValid = false;
        cleanupExpressions();
        return false;
    }

    // Test evaluation
    double xVal = te_eval(m_xParsed.get());
    double yVal = te_eval(m_yParsed.get());
    double zVal = te_eval(m_zParsed.get());
    
    if (std::isnan(xVal) || std::isinf(xVal)) {
        errorMsg = "X expression produces invalid values (NaN/Inf)";
        m_isValid = false;
        cleanupExpressions();
        return false;
    }
    
    if (std::isnan(yVal) || std::isinf(yVal)) {
        errorMsg = "Y expression produces invalid values (NaN/Inf)";
        m_isValid = false;
        cleanupExpressions();
        return false;
    }
    if (std::isnan(zVal) || std::isinf(zVal)) {
        errorMsg = "Z expression produces invalid values (NaN/Inf)";
        m_isValid = false;
        cleanupExpressions();
        return false;
    }
    
    m_isValid = true;
    errorMsg = "";
    return true;
}



std::vector<float> OscMusicEditor::generateStereoBuffer(float duration, int sampleRate) {
    return generateXYBuffer(duration, sampleRate).stereo;
}

OscMusicBuffer OscMusicEditor::generateXYBuffer(float duration, int sampleRate) {
    OscMusicBuffer output;
    if (!m_isValid || !m_xParsed || !m_yParsed || !m_zParsed) return output;
    if (duration <= 0.0f || sampleRate <= 0) return output;

    constexpr int kMaxSamples = 192000 * 60;
    const double rawSamples = static_cast<double>(duration) * static_cast<double>(sampleRate);
    const int totalSamples = static_cast<int>(std::clamp(rawSamples, 1.0, static_cast<double>(kMaxSamples)));
    output.stereo.reserve(static_cast<size_t>(totalSamples) * 2);
    output.z.reserve(static_cast<size_t>(totalSamples));
    
    m_evalF = (double)m_baseFreq;
    
    auto sanitizeClamp = [](double value, float lo, float hi) -> float {
        if (!std::isfinite(value)) return 0.0f;
        return std::clamp(static_cast<float>(value), lo, hi);
    };

    for (int i = 0; i < totalSamples; ++i) {
        m_evalT = (double)i / sampleRate;

        const float x = sanitizeClamp(te_eval(m_xParsed.get()), -1.0f, 1.0f);
        const float y = sanitizeClamp(te_eval(m_yParsed.get()), -1.0f, 1.0f);
        const float z = sanitizeClamp(te_eval(m_zParsed.get()), 0.0f, 1.0f);

        output.stereo.push_back(x);
        output.stereo.push_back(y);
        output.z.push_back(z);
    }

    return output;
}

std::vector<float> OscMusicEditor::getPreviewPoints(int count, float timeRange) {
    std::vector<float> points;
    if (!m_isValid || !m_xParsed || !m_yParsed || count < 2) return points;

    points.reserve(static_cast<size_t>(count) * 2);
    m_evalF = (double)m_baseFreq;

    auto sanitizeClamp = [](double value) -> float {
        if (!std::isfinite(value)) return 0.0f;
        return std::clamp(static_cast<float>(value), -1.0f, 1.0f);
    };

    for (int i = 0; i < count; ++i) {
        m_evalT = (double)i / (count - 1) * timeRange;
        points.push_back(sanitizeClamp(te_eval(m_xParsed.get())));
        points.push_back(sanitizeClamp(te_eval(m_yParsed.get())));
    }
    return points;
}

void OscMusicEditor::loadPreset(int index) {
    if (index < 0 || index >= (int)m_presets.size()) {
        return;
    }
    
    const OscMusicPreset& preset = m_presets[index];
    setExpressions(preset.xExpr, preset.yExpr, preset.zExpr);
}
