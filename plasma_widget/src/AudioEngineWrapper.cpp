#include "AudioEngineWrapper.hpp"
#include <QDebug>
#include <QUrl>

AudioEngineWrapper::AudioEngineWrapper(QObject* parent) : QObject(parent) {
    m_audioEngine = std::make_unique<AudioEngine>();
    m_analysisEngine = std::make_unique<AnalysisEngine>(8192); // Sync with main app

    m_updateTimer = new QTimer(this);
    connect(m_updateTimer, &QTimer::timeout, this, &AudioEngineWrapper::update);
    m_updateTimer->start(16); // ~60 FPS
}

AudioEngineWrapper::~AudioEngineWrapper() {
    m_audioEngine->stop();
}

bool AudioEngineWrapper::loadFile(const QString& filePath) {
    QString path = filePath;
    if (path.startsWith("file://")) {
        path = QUrl(path).toLocalFile();
    }
    
    bool success = m_audioEngine->loadFile(path.toStdString());
    if (success) {
        emit durationChanged();
    }
    return success;
}

bool AudioEngineWrapper::startCapture(int deviceIndex) {
    auto devices = m_audioEngine->getAvailableDevices(true);
    if (deviceIndex >= 0 && deviceIndex < (int)devices.size()) {
        return m_audioEngine->startCapture(&devices[deviceIndex].id);
    }
    return m_audioEngine->startCapture(nullptr); 
}

void AudioEngineWrapper::play() {
    m_audioEngine->play();
    emit isPlayingChanged();
}

void AudioEngineWrapper::stop() {
    m_audioEngine->stop();
    emit isPlayingChanged();
}

void AudioEngineWrapper::stopCapture() {
    m_audioEngine->stopCapture();
    emit isPlayingChanged();
}

void AudioEngineWrapper::setGain(float gain) {
    m_analysisEngine->setGain(gain);
}

void AudioEngineWrapper::setFalloff(float falloff) {
    m_analysisEngine->setFalloff(falloff);
}

void AudioEngineWrapper::setBarHeight(float height) {
    m_barHeight = height;
}

QStringList AudioEngineWrapper::getCaptureDevices() const {
    QStringList names;
    auto devices = m_audioEngine->getAvailableDevices(true);
    for (const auto& d : devices) {
        names << QString::fromStdString(d.name);
    }
    return names;
}

QVector<float> AudioEngineWrapper::magnitudes() const {
    return m_currentMagnitudes;
}

bool AudioEngineWrapper::isPlaying() const {
    return m_audioEngine->isPlaying();
}

float AudioEngineWrapper::duration() const {
    return m_audioEngine->getDuration();
}

float AudioEngineWrapper::position() const {
    return m_audioEngine->getPosition();
}

void AudioEngineWrapper::update() {
    if (!m_audioEngine->isPlaying() && !m_audioEngine->isCaptureMode()) return;

    std::vector<float> audioBuffer;
    m_audioEngine->getBuffer(audioBuffer, 8192);
    m_analysisEngine->process(audioBuffer);

    const auto& mags = m_analysisEngine->getMagnitudes();
    
    if (m_currentMagnitudes.size() != mags.size()) {
        m_currentMagnitudes.resize(mags.size());
    }

    for (int i = 0; i < mags.size(); ++i) {
        m_currentMagnitudes[i] = std::min(mags[i] * m_barHeight, 1.0f); // Normalize for QML (0-1 range usually better)
    }

    emit magnitudesChanged();
    emit positionChanged();
    
    // Check if playback finished
    static bool wasPlaying = false;
    bool nowPlaying = m_audioEngine->isPlaying();
    if (wasPlaying && !nowPlaying) {
        emit isPlayingChanged();
    }
    wasPlaying = nowPlaying;
}
