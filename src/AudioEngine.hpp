#ifndef AUDIO_ENGINE_HPP
#define AUDIO_ENGINE_HPP

#include "miniaudio.h"
#include <string>
#include <vector>
#include <mutex>

class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine();

    bool loadFile(const std::string& filePath);
    bool startCapture(const ma_device_id* pID = nullptr);
    void play();
    void pause();
    void stop();
    void stopCapture();

    bool isPlaying() const { return m_isPlaying; }
    bool isCaptureMode() const { return m_isCaptureMode; }
    float getPosition() const;
    float getDuration() const;
    size_t getChannels() const;

    // Get current audio buffer for analysis
    void getBuffer(std::vector<float>& buffer, size_t size);

    // Device management
    struct DeviceInfo {
        ma_device_id id;
        std::string name;
        bool isDefault;
        bool isCapture;
    };
    std::vector<DeviceInfo> getAvailableDevices(bool capture = false);
    void setDevice(const ma_device_id* pID);
    
    // Test Tone
    enum TestToneType { ToneSine, ToneSquare, ToneSaw, ToneTriangle, ToneNoise };
    void setTestTone(bool enabled) { m_testTone = enabled; }
    void setTestToneType(TestToneType type) { m_testToneType = type; }
    void setTestToneFrequency(float freq) { m_testToneFrequency = freq; }
    void setTestToneVolume(float vol) { m_testToneVolume = vol; }
    bool initTestTone(); // Initialize device for test tone if needed

private:
    static void dataCallback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount);

    ma_decoder m_decoder;
    ma_device m_device;
    bool m_isDecoderInitialized = false;
    bool m_isDeviceInitialized = false;
    bool m_isPlaying = false;

    mutable std::mutex m_bufferMutex;
    std::vector<float> m_circularBuffer;
    size_t m_writePos = 0;

    ma_device_id m_selectedDeviceID;
    bool m_useSpecificDevice = false;
    
    // Test Tone State
    bool m_testTone = false;
    TestToneType m_testToneType = ToneSine;
    float m_testToneFrequency = 440.0f;
    float m_testToneVolume = 0.5f;
    float m_testTonePhase = 0.0f;

    bool m_isCaptureMode = false;
};

#endif // AUDIO_ENGINE_HPP
