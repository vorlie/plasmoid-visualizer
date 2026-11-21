#ifndef OSCMUSIC_EDITOR_HPP
#define OSCMUSIC_EDITOR_HPP

#include <memory>
#include <string>
#include <vector>

struct te_expr;

struct OscMusicPreset {
    std::string name;
    std::string xExpr;
    std::string yExpr;
    float duration;
    std::string zExpr = "1";
};

struct OscMusicBuffer {
    std::vector<float> stereo;
    std::vector<float> z;
};

class OscMusicEditor {
public:
    OscMusicEditor();
    ~OscMusicEditor();
    OscMusicEditor(const OscMusicEditor&) = delete;
    OscMusicEditor& operator=(const OscMusicEditor&) = delete;
    OscMusicEditor(OscMusicEditor&&) = delete;
    OscMusicEditor& operator=(OscMusicEditor&&) = delete;

    // Expression evaluation
    bool setExpressions(const std::string& xExpr, const std::string& yExpr);
    bool setExpressions(const std::string& xExpr, const std::string& yExpr, const std::string& zExpr);
    bool validateExpressions(std::string& errorMsg);
    
    // Audio generation
    std::vector<float> generateStereoBuffer(float duration, int sampleRate = 192000);
    OscMusicBuffer generateXYBuffer(float duration, int sampleRate = 192000);
    
    // Preset management
    void loadPreset(int index);
    const std::vector<OscMusicPreset>& getPresets() const { return m_presets; }
    
    // Current state
    void setBaseFrequency(float freq) { m_baseFreq = freq; }
    float getBaseFrequency() const { return m_baseFreq; }
    
    std::vector<float> getPreviewPoints(int count, float timeRange);
    
    std::string getXExpression() const { return m_xExpr; }
    std::string getYExpression() const { return m_yExpr; }
    std::string getZExpression() const { return m_zExpr; }
    std::string getErrorMessage() const { return m_lastError; }
    bool isValid() const { return m_isValid; }

private:
    std::string m_xExpr;
    std::string m_yExpr;
    std::string m_zExpr;
    std::string m_lastError;
    bool m_isValid;
    float m_baseFreq = 440.0f;
    
    struct TeExprDeleter {
        void operator()(te_expr* expr) const noexcept;
    };
    using TeExprPtr = std::unique_ptr<te_expr, TeExprDeleter>;

    // Evaluation state (tinyexpr holds raw pointers to m_evalT / m_evalF, hence non-movable)
    double m_evalT = 0.0;
    double m_evalF = 440.0;
    TeExprPtr m_xParsed;
    TeExprPtr m_yParsed;
    TeExprPtr m_zParsed;
    
    std::vector<OscMusicPreset> m_presets;
    
    void cleanupExpressions();
    void initializePresets();
};

#endif // OSCMUSIC_EDITOR_HPP
