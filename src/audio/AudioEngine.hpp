#ifndef AUDIO_ENGINE_HPP
#define AUDIO_ENGINE_HPP

#include "miniaudio.h"
#include "XYOscilloscopeTypes.hpp"

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine();

    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;
    AudioEngine(AudioEngine&&) = delete;
    AudioEngine& operator=(AudioEngine&&) = delete;

    bool loadFile(const std::string& filePath);
    bool startCapture(const ma_device_id* pID = nullptr, ma_device_type type = ma_device_type_capture);
    void play();
    void pause();
    void stop();
    void stopCapture();
    void seekTo(float seconds);

    bool isPlaying() const { return m_isPlaying.load(std::memory_order_relaxed); }
    bool isCaptureMode() const { return m_isCaptureMode.load(std::memory_order_relaxed); }
    void setLoopTrack(bool loop) { m_loopTrack.store(loop, std::memory_order_relaxed); }
    bool isLoopTrack() const { return m_loopTrack.load(std::memory_order_relaxed); }
    bool consumeTrackEnded() { return m_trackEnded.exchange(false, std::memory_order_acq_rel); }
    std::uint64_t consumeDecoderErrorCount() { return m_decoderErrorCount.exchange(0, std::memory_order_relaxed); }

    float getPosition() const;
    float getDuration() const;
    ma_uint32 getSampleRate() const;
    size_t getChannels() const;

    // Get current audio buffer for analysis (Mono)
    void getBuffer(std::vector<float>& buffer, size_t size);

    // Get specific channel buffer (0=Mixed, 1=Left, 2=Right)
    void getChannelBuffer(std::vector<float>& buffer, size_t size, int channel);

    // Get current stereo buffer for Oscilloscope XY (Interleaved L/R)
    void getStereoBuffer(std::vector<float>& buffer, size_t frames);
    XYInputChunk readXYSince(std::uint64_t cursor, size_t maxFrames) const;
    XYInputChunk snapshotXY(size_t frames) const;
    std::uint64_t latestXYFrame() const;

    // Device management
    struct DeviceInfo {
        ma_device_id id;
        std::string name;
        bool isDefault;
        ma_device_type type;
    };
    std::vector<DeviceInfo> getAvailableDevices(bool capture = false);
    void setDevice(const ma_device_id* pID);

    // Test Tone
    enum TestToneType { ToneSine, ToneSquare, ToneSaw, ToneTriangle, ToneNoise };
    void setTestTone(bool enabled) { m_testTone.store(enabled, std::memory_order_relaxed); }
    void setTestToneType(TestToneType type) { m_testToneType.store(type, std::memory_order_relaxed); }
    void setTestToneFrequency(float freq) { m_testToneFrequency.store(freq, std::memory_order_relaxed); }
    void setTestToneStereo(bool stereo) { m_testToneStereo.store(stereo, std::memory_order_relaxed); }
    void setTestToneFrequencyRight(float freq) { m_testToneFrequencyRight.store(freq, std::memory_order_relaxed); }
    void setTestToneVolume(float vol) { m_testToneVolume.store(vol, std::memory_order_relaxed); }
    bool initTestTone(); // Initialize device for test tone if needed

    // Oscilloscope Music Mode
    void setOscMusicMode(bool enabled) { m_oscMusicMode.store(enabled, std::memory_order_relaxed); }
    void setOscMusicBuffer(std::vector<float> buffer);
    void setOscMusicBuffer(std::vector<float> buffer, std::vector<float> z);
    bool isOscMusicMode() const { return m_oscMusicMode.load(std::memory_order_relaxed); }
    bool initOscMusic(int sampleRate = 192000);

    // Silent synthetic audio injection for deterministic benchmarking (no audio output)
    void pushSyntheticFrames(size_t frameCount, double& phase, ma_uint32 sampleRate = 48000);

private:
    static constexpr size_t kRingCapacity = 65536; // Must be power of 2
    static constexpr size_t kRingMask = kRingCapacity - 1;

    struct OscMusicClip {
        std::vector<float> stereo;
        std::vector<float> z;
    };

    void resetDevice();
    void writeFrameRT(float left, float right, float z = 1.0f, bool hasZ = false) noexcept;
    void markXYDiscontinuity();
    float nextNoiseSampleRT() noexcept;
    static void dataCallback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount);

    ma_decoder m_decoder{};
    ma_device m_device{};
    std::atomic<bool> m_isDecoderInitialized{false};
    bool m_isDeviceInitialized = false;
    std::atomic<bool> m_isPlaying{false};
    std::atomic<bool> m_loopTrack{true};
    std::atomic<bool> m_trackEnded{false};
    std::atomic<std::uint64_t> m_decoderErrorCount{0};

    // Cached decoder metadata (written on loadFile while stopped; read lock-free on UI thread)
    std::atomic<std::uint64_t> m_durationFrames{0};
    std::atomic<std::uint64_t> m_cursorFrames{0};
    std::atomic<std::int64_t> m_pendingSeekFrame{-1};
    std::atomic<ma_uint32> m_decoderSampleRate{0};
    std::atomic<ma_uint32> m_decoderChannels{0};

    // Wait-free SPSC audio visualization ring buffer (audio thread writes, main thread reads)
    std::unique_ptr<std::atomic<float>[]> m_leftRing;
    std::unique_ptr<std::atomic<float>[]> m_rightRing;
    std::unique_ptr<std::atomic<float>[]> m_zRing;
    std::unique_ptr<std::atomic<std::uint8_t>[]> m_zValidRing;
    std::atomic<std::uint64_t> m_writeCounter{0};
    std::atomic<std::uint64_t> m_validStartFrame{0};
    std::atomic<std::uint64_t> m_xyDiscontinuityGeneration{1};
    std::atomic<ma_uint32> m_currentSampleRate{48000};

    ma_device_id m_selectedDeviceID{};
    bool m_useSpecificDevice = false;

    // Test Tone State (atomics for UI -> RT communication; phases & PRNG are RT-thread private)
    std::atomic<bool> m_testTone{false};
    std::atomic<TestToneType> m_testToneType{ToneSine};
    std::atomic<float> m_testToneFrequency{440.0f};
    std::atomic<float> m_testToneFrequencyRight{440.0f};
    std::atomic<bool> m_testToneStereo{false};
    std::atomic<float> m_testToneVolume{0.5f};
    float m_testTonePhase = 0.0f;
    float m_testTonePhaseRight = 0.0f;
    std::uint32_t m_noiseState = 0x12345678u;

    std::atomic<bool> m_isCaptureMode{false};

    // OscMusic playback state (hazard-pointer published, zero lock/copy on RT thread)
    std::atomic<bool> m_oscMusicMode{false};
    std::atomic<const OscMusicClip*> m_activeOscClip{nullptr};
    std::atomic<const OscMusicClip*> m_audioInUseOscClip{nullptr};
    std::atomic<std::uint64_t> m_oscClipGeneration{0};
    std::vector<std::unique_ptr<OscMusicClip>> m_ownedOscClips;
    std::uint64_t m_audioOscClipGen = 0;
    size_t m_oscMusicReadPos = 0;
};

#endif // AUDIO_ENGINE_HPP
