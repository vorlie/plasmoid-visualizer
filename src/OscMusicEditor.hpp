#ifndef OSCMUSIC_EDITOR_HPP
#define OSCMUSIC_EDITOR_HPP

#include <string>
#include <vector>

struct OscMusicPreset {
    std::string name;
    std::string xExpr;
    std::string yExpr;
    float duration;
};

class OscMusicEditor {
public:
    OscMusicEditor();
    ~OscMusicEditor();

    // Expression evaluation
    bool setExpressions(const std::string& xExpr, const std::string& yExpr);
    bool validateExpressions(std::string& errorMsg);
    
    // Audio generation
    std::vector<float> generateStereoBuffer(float duration, int sampleRate = 192000);
    
    // Preset management
    void loadPreset(int index);
    const std::vector<OscMusicPreset>& getPresets() const { return m_presets; }
    
    // Current state
    void setBaseFrequency(float freq) { m_baseFreq = freq; }
    float getBaseFrequency() const { return m_baseFreq; }
    
    std::string getXExpression() const { return m_xExpr; }
    std::string getYExpression() const { return m_yExpr; }
    std::string getErrorMessage() const { return m_lastError; }
    bool isValid() const { return m_isValid; }

private:
    std::string m_xExpr;
    std::string m_yExpr;
    std::string m_lastError;
    bool m_isValid;
    float m_baseFreq = 440.0f;
    
    std::vector<OscMusicPreset> m_presets;
    
    void initializePresets();
    float evaluateExpression(const std::string& expr, double t, bool& success);
};

#endif // OSCMUSIC_EDITOR_HPP
