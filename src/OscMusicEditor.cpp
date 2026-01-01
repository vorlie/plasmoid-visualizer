#include "OscMusicEditor.hpp"
#include "tinyexpr.h"
#include <cmath>
#include <iostream>
#include <algorithm>

OscMusicEditor::OscMusicEditor() 
    : m_xExpr("sin(t * 2 * 3.14159)"),
      m_yExpr("cos(t * 2 * 3.14159)"),
      m_isValid(true) {
    initializePresets();
}

OscMusicEditor::~OscMusicEditor() {}

void OscMusicEditor::initializePresets() {
    m_presets = {
        {"Circle", "sin(f * t * 2 * 3.14159)", "cos(f * t * 2 * 3.14159)", 2.0f},
        {"Lissajous 1:2", "sin(f * t * 2 * 3.14159)", "sin(f * t * 4 * 3.14159)", 2.0f},
        {"Lissajous 2:3", "sin(f * t * 2 * 3.14159)", "sin(f * t * 3 * 3.14159)", 2.0f},
        {"Figure-8", "sin(f * t * 3.14159)", "sin(f * t * 2 * 3.14159)", 2.0f},
        {"Star (5-point)", "cos(5*f*t) * cos(f*t)", "cos(5*f*t) * sin(f*t)", 2.0f},
        {"Heart", "16 * pow(sin(f*t), 3)", 
                 "13*cos(f*t) - 5*cos(2*f*t) - 2*cos(3*f*t) - cos(4*f*t)", 2.0f},
        {"Spiral", "f*t/6.28 * cos(f*t*3)", "f*t/6.28 * sin(f*t*3)", 2.0f},
        {"Rose (3-petal)", "cos(3*f*t) * cos(f*t)", "cos(3*f*t) * sin(f*t)", 2.0f}
    };
}

bool OscMusicEditor::setExpressions(const std::string& xExpr, const std::string& yExpr) {
    m_xExpr = xExpr;
    m_yExpr = yExpr;
    return validateExpressions(m_lastError);
}

bool OscMusicEditor::validateExpressions(std::string& errorMsg) {
    // Test evaluation at t=0
    double t = 0.0;
    double f = (double)m_baseFreq;
    te_variable vars[] = {{"t", &t}, {"f", &f}};
    
    int xErr = 0, yErr = 0;
    te_expr* xParsed = te_compile(m_xExpr.c_str(), vars, 2, &xErr);
    te_expr* yParsed = te_compile(m_yExpr.c_str(), vars, 2, &yErr);
    
    if (!xParsed) {
        errorMsg = "X expression error at position " + std::to_string(xErr);
        m_isValid = false;
        if (yParsed) te_free(yParsed);
        return false;
    }
    
    if (!yParsed) {
        errorMsg = "Y expression error at position " + std::to_string(yErr);
        m_isValid = false;
        te_free(xParsed);
        return false;
    }
    
    // Test evaluation
    double xVal = te_eval(xParsed);
    double yVal = te_eval(yParsed);
    
    if (std::isnan(xVal) || std::isinf(xVal)) {
        errorMsg = "X expression produces invalid values (NaN/Inf)";
        m_isValid = false;
        te_free(xParsed);
        te_free(yParsed);
        return false;
    }
    
    if (std::isnan(yVal) || std::isinf(yVal)) {
        errorMsg = "Y expression produces invalid values (NaN/Inf)";
        m_isValid = false;
        te_free(xParsed);
        te_free(yParsed);
        return false;
    }
    
    te_free(xParsed);
    te_free(yParsed);
    
    m_isValid = true;
    errorMsg = "";
    return true;
}

float OscMusicEditor::evaluateExpression(const std::string& expr, double t, bool& success) {
    double f = (double)m_baseFreq;
    te_variable vars[] = {{"t", &t}, {"f", &f}};
    int err = 0;
    te_expr* parsed = te_compile(expr.c_str(), vars, 2, &err);
    
    if (!parsed) {
        success = false;
        return 0.0f;
    }
    
    double result = te_eval(parsed);
    te_free(parsed);
    
    if (std::isnan(result) || std::isinf(result)) {
        success = false;
        return 0.0f;
    }
    
    success = true;
    return (float)result;
}

std::vector<float> OscMusicEditor::generateStereoBuffer(float duration, int sampleRate) {
    std::vector<float> stereoBuffer;
    
    if (!m_isValid) {
        std::cerr << "Cannot generate audio: expressions are invalid" << std::endl;
        return stereoBuffer;
    }
    
    int totalSamples = (int)(duration * sampleRate);
    stereoBuffer.reserve(totalSamples * 2);
    
    double t;
    double f = (double)m_baseFreq;
    te_variable vars[] = {{"t", &t}, {"f", &f}};
    
    int xErr = 0, yErr = 0;
    te_expr* xParsed = te_compile(m_xExpr.c_str(), vars, 2, &xErr);
    te_expr* yParsed = te_compile(m_yExpr.c_str(), vars, 2, &yErr);
    
    if (!xParsed || !yParsed) {
        std::cerr << "Failed to compile expressions during generation" << std::endl;
        if (xParsed) te_free(xParsed);
        if (yParsed) te_free(yParsed);
        return stereoBuffer;
    }
    
    for (int i = 0; i < totalSamples; ++i) {
        t = (double)i / sampleRate;
        
        float x = (float)te_eval(xParsed);
        float y = (float)te_eval(yParsed);
        
        // Clamp to valid range
        x = std::clamp(x, -1.0f, 1.0f);
        y = std::clamp(y, -1.0f, 1.0f);
        
        stereoBuffer.push_back(x); // Left = X
        stereoBuffer.push_back(y); // Right = Y
    }
    
    te_free(xParsed);
    te_free(yParsed);
    
    std::cout << "Generated " << totalSamples << " samples (" << duration << "s) for oscilloscope music" << std::endl;
    
    return stereoBuffer;
}

void OscMusicEditor::loadPreset(int index) {
    if (index < 0 || index >= (int)m_presets.size()) {
        return;
    }
    
    const OscMusicPreset& preset = m_presets[index];
    setExpressions(preset.xExpr, preset.yExpr);
}
