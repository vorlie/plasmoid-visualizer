#ifndef AUDIO_ENGINE_WRAPPER_HPP
#define AUDIO_ENGINE_WRAPPER_HPP

#include <QObject>
#include <QVector>
#include <QTimer>
#include <memory>
#include "../../src/AudioEngine.hpp"
#include "../../src/AnalysisEngine.hpp"

class AudioEngineWrapper : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVector<float> magnitudes READ magnitudes NOTIFY magnitudesChanged)
    Q_PROPERTY(bool isPlaying READ isPlaying NOTIFY isPlayingChanged)
    Q_PROPERTY(float duration READ duration NOTIFY durationChanged)
    Q_PROPERTY(float position READ position NOTIFY positionChanged)

public:
    explicit AudioEngineWrapper(QObject* parent = nullptr);
    ~AudioEngineWrapper();

    Q_INVOKABLE bool startCapture(int deviceIndex = -1);
    Q_INVOKABLE void stopCapture();
    Q_INVOKABLE void setGain(float gain);
    Q_INVOKABLE void setFalloff(float falloff);
    Q_INVOKABLE void setBarHeight(float height);
    Q_INVOKABLE QStringList getCaptureDevices() const;

    QVector<float> magnitudes() const;
    bool isPlaying() const;
    float duration() const;
    float position() const;

signals:
    void magnitudesChanged();
    void isPlayingChanged();
    void durationChanged();
    void positionChanged();

private slots:
    void update();

private:
    std::unique_ptr<AudioEngine> m_audioEngine;
    std::unique_ptr<AnalysisEngine> m_analysisEngine;
    QTimer* m_updateTimer;
    QVector<float> m_currentMagnitudes;
    float m_barHeight = 0.2f;
};

#endif // AUDIO_ENGINE_WRAPPER_HPP
