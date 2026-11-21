#define MINIAUDIO_IMPLEMENTATION
#include "AudioEngine.hpp"
#include "Utf8Paths.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>

namespace {
constexpr float kTwoPi = 6.28318530717958647692f;
constexpr float kPi = 3.14159265358979323846f;
}

AudioEngine::AudioEngine()
    : m_leftRing(std::make_unique<std::atomic<float>[]>(kRingCapacity)),
      m_rightRing(std::make_unique<std::atomic<float>[]>(kRingCapacity)),
      m_zRing(std::make_unique<std::atomic<float>[]>(kRingCapacity)),
      m_zValidRing(std::make_unique<std::atomic<std::uint8_t>[]>(kRingCapacity)) {
    for (size_t i = 0; i < kRingCapacity; ++i) {
        m_leftRing[i].store(0.0f, std::memory_order_relaxed);
        m_rightRing[i].store(0.0f, std::memory_order_relaxed);
        m_zRing[i].store(1.0f, std::memory_order_relaxed);
        m_zValidRing[i].store(0u, std::memory_order_relaxed);
    }
}

AudioEngine::~AudioEngine() {
    if (m_isDeviceInitialized) {
        ma_device_uninit(&m_device);
        m_isDeviceInitialized = false;
    }
    if (m_isDecoderInitialized.exchange(false, std::memory_order_acq_rel)) {
        ma_decoder_uninit(&m_decoder);
    }
}

void AudioEngine::writeFrameRT(float left, float right, float z, bool hasZ) noexcept {
    const std::uint64_t idx = m_writeCounter.load(std::memory_order_relaxed);
    const size_t slot = static_cast<size_t>(idx & kRingMask);
    m_leftRing[slot].store(left, std::memory_order_relaxed);
    m_rightRing[slot].store(right, std::memory_order_relaxed);
    m_zRing[slot].store(z, std::memory_order_relaxed);
    m_zValidRing[slot].store(hasZ ? 1u : 0u, std::memory_order_relaxed);
    m_writeCounter.store(idx + 1, std::memory_order_release);
}

void AudioEngine::markXYDiscontinuity() {
    m_validStartFrame.store(m_writeCounter.load(std::memory_order_acquire), std::memory_order_release);
    m_xyDiscontinuityGeneration.fetch_add(1, std::memory_order_release);
}

float AudioEngine::nextNoiseSampleRT() noexcept {
    // Fast RT-safe xorshift32 PRNG (no CRT lock or global state)
    std::uint32_t x = m_noiseState;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    m_noiseState = x;
    return static_cast<float>(static_cast<std::int32_t>(x)) * (1.0f / 2147483648.0f);
}

void AudioEngine::setOscMusicBuffer(std::vector<float> buffer) {
    setOscMusicBuffer(std::move(buffer), {});
}

void AudioEngine::setOscMusicBuffer(std::vector<float> buffer, std::vector<float> z) {
    auto clip = std::make_unique<OscMusicClip>();
    clip->stereo = std::move(buffer);
    clip->z = std::move(z);
    const OscMusicClip* rawPtr = clip.get();
    m_ownedOscClips.push_back(std::move(clip));

    m_oscClipGeneration.fetch_add(1, std::memory_order_relaxed);
    m_activeOscClip.store(rawPtr, std::memory_order_seq_cst);

    // Reclaim older retired clips that the audio thread is not currently reading.
    const OscMusicClip* inUse = m_audioInUseOscClip.load(std::memory_order_seq_cst);
    m_ownedOscClips.erase(
        std::remove_if(m_ownedOscClips.begin(), m_ownedOscClips.end(),
            [rawPtr, inUse](const std::unique_ptr<OscMusicClip>& entry) {
                return entry.get() != rawPtr && entry.get() != inUse;
            }),
        m_ownedOscClips.end());
}

void AudioEngine::dataCallback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    auto* pEngine = static_cast<AudioEngine*>(pDevice->pUserData);
    if (pEngine == nullptr || frameCount == 0) return;

    const size_t outChannels = pDevice->playback.channels > 0 ? pDevice->playback.channels : 2;
    const float sampleRate = pDevice->sampleRate > 0 ? static_cast<float>(pDevice->sampleRate) : 48000.0f;

    if (pEngine->m_testTone.load(std::memory_order_relaxed)) {
        if (pOutput && !pInput) {
            auto* pOutputF32 = static_cast<float*>(pOutput);
            const TestToneType toneType = pEngine->m_testToneType.load(std::memory_order_relaxed);
            const float freqL = pEngine->m_testToneFrequency.load(std::memory_order_relaxed);
            const float freqR = pEngine->m_testToneFrequencyRight.load(std::memory_order_relaxed);
            const bool stereo = pEngine->m_testToneStereo.load(std::memory_order_relaxed);
            const float volume = pEngine->m_testToneVolume.load(std::memory_order_relaxed);

            auto evalWave = [&](float phase) -> float {
                switch (toneType) {
                    case ToneSine: return std::sin(phase);
                    case ToneSquare: return (phase < kPi) ? 1.0f : -1.0f;
                    case ToneSaw: return (phase / kPi) - 1.0f;
                    case ToneTriangle:
                        return (phase < kPi)
                            ? (2.0f * phase / kPi - 1.0f)
                            : (3.0f - 2.0f * phase / kPi);
                    case ToneNoise: return pEngine->nextNoiseSampleRT();
                }
                return 0.0f;
            };

            const float stepL = kTwoPi * freqL / sampleRate;
            const float stepR = kTwoPi * freqR / sampleRate;

            for (ma_uint32 i = 0; i < frameCount; ++i) {
                const float sampleLeft = evalWave(pEngine->m_testTonePhase) * volume;
                const float sampleRight = stereo
                    ? (evalWave(pEngine->m_testTonePhaseRight) * volume)
                    : sampleLeft;

                if (outChannels >= 2) {
                    pOutputF32[i * outChannels + 0] = sampleLeft;
                    pOutputF32[i * outChannels + 1] = sampleRight;
                    for (size_t c = 2; c < outChannels; ++c) pOutputF32[i * outChannels + c] = 0.0f;
                } else {
                    pOutputF32[i] = (sampleLeft + sampleRight) * 0.5f;
                }

                pEngine->writeFrameRT(sampleLeft, sampleRight);

                pEngine->m_testTonePhase += stepL;
                if (pEngine->m_testTonePhase >= kTwoPi) {
                    pEngine->m_testTonePhase = std::fmod(pEngine->m_testTonePhase, kTwoPi);
                }
                if (stereo) {
                    pEngine->m_testTonePhaseRight += stepR;
                    if (pEngine->m_testTonePhaseRight >= kTwoPi) {
                        pEngine->m_testTonePhaseRight = std::fmod(pEngine->m_testTonePhaseRight, kTwoPi);
                    }
                }
            }
        } else if (pOutput) {
            std::memset(pOutput, 0, frameCount * outChannels * sizeof(float));
        }
        return;
    }

    if (pEngine->m_oscMusicMode.load(std::memory_order_relaxed)) {
        if (pOutput && !pInput) {
            auto* pOutputF32 = static_cast<float*>(pOutput);

            // Acquire hazard pointer to the active immutable clip without locking.
            const OscMusicClip* clip = nullptr;
            do {
                clip = pEngine->m_activeOscClip.load(std::memory_order_seq_cst);
                pEngine->m_audioInUseOscClip.store(clip, std::memory_order_seq_cst);
            } while (clip != pEngine->m_activeOscClip.load(std::memory_order_seq_cst));

            const std::uint64_t clipGen = pEngine->m_oscClipGeneration.load(std::memory_order_relaxed);
            if (clipGen != pEngine->m_audioOscClipGen) {
                pEngine->m_audioOscClipGen = clipGen;
                pEngine->m_oscMusicReadPos = 0;
            }

            if (!clip || clip->stereo.size() < 2) {
                std::memset(pOutput, 0, frameCount * outChannels * sizeof(float));
                pEngine->m_audioInUseOscClip.store(nullptr, std::memory_order_release);
                return;
            }

            const size_t stereoSize = clip->stereo.size() & ~size_t{1};
            const bool hasZ = !clip->z.empty();

            for (ma_uint32 i = 0; i < frameCount; ++i) {
                if (pEngine->m_oscMusicReadPos + 1 >= stereoSize) {
                    pEngine->m_oscMusicReadPos = 0;
                }
                const size_t frame = pEngine->m_oscMusicReadPos / 2;
                const float sampleL = clip->stereo[pEngine->m_oscMusicReadPos];
                const float sampleR = clip->stereo[pEngine->m_oscMusicReadPos + 1];
                const float sampleZ = (hasZ && frame < clip->z.size()) ? clip->z[frame] : 1.0f;
                pEngine->m_oscMusicReadPos += 2;

                if (outChannels >= 2) {
                    pOutputF32[i * outChannels + 0] = sampleL;
                    pOutputF32[i * outChannels + 1] = sampleR;
                    for (size_t c = 2; c < outChannels; ++c) pOutputF32[i * outChannels + c] = 0.0f;
                } else {
                    pOutputF32[i] = (sampleL + sampleR) * 0.5f;
                }

                pEngine->writeFrameRT(sampleL, sampleR, sampleZ, hasZ);
            }

            pEngine->m_audioInUseOscClip.store(nullptr, std::memory_order_release);
        } else if (pOutput) {
            std::memset(pOutput, 0, frameCount * outChannels * sizeof(float));
        }
        return;
    }

    if (pEngine->m_isCaptureMode.load(std::memory_order_acquire)) {
        if (pOutput) {
            std::memset(pOutput, 0, frameCount * outChannels * sizeof(float));
        }
        if (pInput) {
            const auto* pInputF32 = static_cast<const float*>(pInput);
            const size_t inChannels = pDevice->capture.channels;
            for (ma_uint32 i = 0; i < frameCount; ++i) {
                float left = 0.0f;
                float right = 0.0f;
                if (inChannels >= 2) {
                    left = pInputF32[i * inChannels + 0];
                    right = pInputF32[i * inChannels + 1];
                } else if (inChannels == 1) {
                    left = right = pInputF32[i];
                }
                pEngine->writeFrameRT(left, right);
            }
        }
        return;
    }

    if (pEngine->m_isDecoderInitialized.load(std::memory_order_acquire) && pOutput) {
        const std::int64_t requestedSeek = pEngine->m_pendingSeekFrame.exchange(-1, std::memory_order_acq_rel);
        if (requestedSeek >= 0) {
            ma_decoder_seek_to_pcm_frame(&pEngine->m_decoder, static_cast<ma_uint64>(requestedSeek));
            pEngine->m_cursorFrames.store(static_cast<std::uint64_t>(requestedSeek), std::memory_order_relaxed);
        }

        auto* pOutputF32 = static_cast<float*>(pOutput);
        const size_t channels = pEngine->m_decoder.outputChannels > 0 ? pEngine->m_decoder.outputChannels : outChannels;
        ma_uint32 totalRead = 0;

        while (totalRead < frameCount) {
            ma_uint64 framesRead64 = 0;
            const ma_result result = ma_decoder_read_pcm_frames(
                &pEngine->m_decoder,
                pOutputF32 + totalRead * channels,
                frameCount - totalRead,
                &framesRead64);
            const ma_uint32 chunk = static_cast<ma_uint32>(framesRead64);
            totalRead += chunk;

            if (result != MA_SUCCESS && result != MA_AT_END) {
                pEngine->m_decoderErrorCount.fetch_add(1, std::memory_order_relaxed);
            }

            if (totalRead < frameCount) {
                if (pEngine->m_loopTrack.load(std::memory_order_relaxed)) {
                    const ma_result seekRes = ma_decoder_seek_to_pcm_frame(&pEngine->m_decoder, 0);
                    if (seekRes != MA_SUCCESS || chunk == 0) {
                        // Avoid infinite spin if file is empty or cannot seek.
                        std::memset(pOutputF32 + totalRead * channels, 0, (frameCount - totalRead) * channels * sizeof(float));
                        break;
                    }
                } else {
                    std::memset(pOutputF32 + totalRead * channels, 0, (frameCount - totalRead) * channels * sizeof(float));
                    pEngine->m_trackEnded.store(true, std::memory_order_release);
                    break;
                }
            }
        }

        ma_uint64 cursor = 0;
        if (ma_decoder_get_cursor_in_pcm_frames(&pEngine->m_decoder, &cursor) == MA_SUCCESS) {
            pEngine->m_cursorFrames.store(static_cast<std::uint64_t>(cursor), std::memory_order_relaxed);
        }

        for (ma_uint32 i = 0; i < totalRead; ++i) {
            float left = 0.0f;
            float right = 0.0f;
            if (channels >= 2) {
                left = pOutputF32[i * channels + 0];
                right = pOutputF32[i * channels + 1];
            } else if (channels == 1) {
                left = right = pOutputF32[i];
            }
            pEngine->writeFrameRT(left, right);
        }
        return;
    }

    if (pOutput) {
        std::memset(pOutput, 0, frameCount * outChannels * sizeof(float));
    }
}

void AudioEngine::getStereoBuffer(std::vector<float>& buffer, size_t frames) {
    const size_t clampedFrames = std::min(frames, kRingCapacity);
    for (int attempt = 0; attempt < 3; ++attempt) {
        const std::uint64_t gen1 = m_xyDiscontinuityGeneration.load(std::memory_order_acquire);
        const std::uint64_t end = m_writeCounter.load(std::memory_order_acquire);
        const std::uint64_t validStart = m_validStartFrame.load(std::memory_order_acquire);
        const std::uint64_t oldest = std::max(validStart, end > kRingCapacity ? end - kRingCapacity : 0);
        const size_t available = end > oldest ? static_cast<size_t>(end - oldest) : 0;
        const size_t count = std::min(clampedFrames, available);
        const std::uint64_t first = end - count;

        buffer.resize(count * 2);
        for (size_t i = 0; i < count; ++i) {
            const size_t slot = static_cast<size_t>((first + i) & kRingMask);
            buffer[i * 2] = m_leftRing[slot].load(std::memory_order_relaxed);
            buffer[i * 2 + 1] = m_rightRing[slot].load(std::memory_order_relaxed);
        }

        const std::uint64_t after = m_writeCounter.load(std::memory_order_acquire);
        const std::uint64_t gen2 = m_xyDiscontinuityGeneration.load(std::memory_order_acquire);
        if (gen1 == gen2 && (after - first) <= kRingCapacity) return;
    }
}

XYInputChunk AudioEngine::readXYSince(std::uint64_t cursor, size_t maxFrames) const {
    XYInputChunk chunk;
    const size_t clampedMax = std::min(maxFrames, kRingCapacity);
    for (int attempt = 0; attempt < 3; ++attempt) {
        chunk = {};
        chunk.sampleRate = getSampleRate();
        const std::uint64_t gen1 = m_xyDiscontinuityGeneration.load(std::memory_order_acquire);
        const std::uint64_t end = m_writeCounter.load(std::memory_order_acquire);
        const std::uint64_t validStart = m_validStartFrame.load(std::memory_order_acquire);
        const std::uint64_t oldest = std::max(validStart, end > kRingCapacity ? end - kRingCapacity : 0);
        chunk.discontinuityGeneration = gen1;

        std::uint64_t startCursor = cursor;
        if (startCursor < oldest) {
            startCursor = oldest;
            chunk.dropped = true;
        }
        startCursor = std::min(startCursor, end);
        const size_t available = static_cast<size_t>(end - startCursor);
        const size_t count = std::min(available, clampedMax);
        if (available <= clampedMax) {
            chunk.firstFrame = startCursor;
        } else {
            chunk.firstFrame = end - count;
            chunk.dropped = true;
        }

        chunk.samples.reserve(count);
        for (size_t i = 0; i < count; ++i) {
            const size_t slot = static_cast<size_t>((chunk.firstFrame + i) & kRingMask);
            const bool hasZ = m_zValidRing[slot].load(std::memory_order_relaxed) != 0;
            chunk.hasZ |= hasZ;
            chunk.samples.push_back({
                m_leftRing[slot].load(std::memory_order_relaxed),
                m_rightRing[slot].load(std::memory_order_relaxed),
                hasZ ? m_zRing[slot].load(std::memory_order_relaxed) : 1.0f});
        }

        const std::uint64_t after = m_writeCounter.load(std::memory_order_acquire);
        const std::uint64_t gen2 = m_xyDiscontinuityGeneration.load(std::memory_order_acquire);
        if (gen1 == gen2 && (after - chunk.firstFrame) <= kRingCapacity) return chunk;
    }
    return chunk;
}

XYInputChunk AudioEngine::snapshotXY(size_t frames) const {
    XYInputChunk chunk;
    const size_t clampedFrames = std::min(frames, kRingCapacity);
    for (int attempt = 0; attempt < 3; ++attempt) {
        chunk = {};
        chunk.sampleRate = getSampleRate();
        const std::uint64_t gen1 = m_xyDiscontinuityGeneration.load(std::memory_order_acquire);
        const std::uint64_t end = m_writeCounter.load(std::memory_order_acquire);
        const std::uint64_t validStart = m_validStartFrame.load(std::memory_order_acquire);
        const std::uint64_t oldest = std::max(validStart, end > kRingCapacity ? end - kRingCapacity : 0);
        const size_t available = end > oldest ? static_cast<size_t>(end - oldest) : 0;
        const size_t count = std::min(clampedFrames, available);
        chunk.discontinuityGeneration = gen1;
        chunk.firstFrame = end - count;

        chunk.samples.reserve(count);
        for (size_t i = 0; i < count; ++i) {
            const size_t slot = static_cast<size_t>((chunk.firstFrame + i) & kRingMask);
            const bool hasZ = m_zValidRing[slot].load(std::memory_order_relaxed) != 0;
            chunk.hasZ |= hasZ;
            chunk.samples.push_back({
                m_leftRing[slot].load(std::memory_order_relaxed),
                m_rightRing[slot].load(std::memory_order_relaxed),
                hasZ ? m_zRing[slot].load(std::memory_order_relaxed) : 1.0f});
        }

        const std::uint64_t after = m_writeCounter.load(std::memory_order_acquire);
        const std::uint64_t gen2 = m_xyDiscontinuityGeneration.load(std::memory_order_acquire);
        if (gen1 == gen2 && (after - chunk.firstFrame) <= kRingCapacity) return chunk;
    }
    return chunk;
}

std::uint64_t AudioEngine::latestXYFrame() const {
    return m_writeCounter.load(std::memory_order_acquire);
}

void AudioEngine::resetDevice() {
    stop();
    if (m_isDeviceInitialized) {
        ma_device_uninit(&m_device);
        m_isDeviceInitialized = false;
    }
    m_isCaptureMode.store(false, std::memory_order_release);
    m_isPlaying.store(false, std::memory_order_release);
    m_oscMusicMode.store(false, std::memory_order_release);
    m_testTone.store(false, std::memory_order_release);
    m_trackEnded.store(false, std::memory_order_release);
    m_pendingSeekFrame.store(-1, std::memory_order_release);
    markXYDiscontinuity();
}

bool AudioEngine::loadFile(const std::string& filePath) {
    if (filePath.empty()) return false;
    m_isPlaying.store(false, std::memory_order_release);
    std::cout << "Attempting to load file: " << filePath << std::endl;

    FILE* f = nullptr;
#ifdef _WIN32
    const std::wstring widePath = Utf8Paths::toWide(filePath);
    if (!widePath.empty()) f = _wfopen(widePath.c_str(), L"rb");
#else
    f = fopen(filePath.c_str(), "rb");
#endif
    if (!f) {
        std::cerr << "Failed to open file for reading: " << filePath << std::endl;
        return false;
    }
    fclose(f);

    resetDevice();

    if (m_isDecoderInitialized.exchange(false, std::memory_order_acq_rel)) {
        ma_decoder_uninit(&m_decoder);
    }
    m_durationFrames.store(0, std::memory_order_relaxed);
    m_cursorFrames.store(0, std::memory_order_relaxed);
    m_decoderSampleRate.store(0, std::memory_order_relaxed);
    m_decoderChannels.store(0, std::memory_order_relaxed);

    ma_decoder_config decoderConfig = ma_decoder_config_init(ma_format_f32, 0, 0);
    ma_result result = MA_ERROR;
#ifdef _WIN32
    result = ma_decoder_init_file_w(widePath.c_str(), &decoderConfig, &m_decoder);
#else
    result = ma_decoder_init_file(filePath.c_str(), &decoderConfig, &m_decoder);
#endif
    if (result != MA_SUCCESS) {
        std::cerr << "Failed to load audio file: " << filePath << " (Error: " << result << ")" << std::endl;
        return false;
    }

    ma_uint64 lengthFrames = 0;
    if (ma_decoder_get_length_in_pcm_frames(&m_decoder, &lengthFrames) == MA_SUCCESS) {
        m_durationFrames.store(static_cast<std::uint64_t>(lengthFrames), std::memory_order_relaxed);
    }
    m_decoderSampleRate.store(m_decoder.outputSampleRate, std::memory_order_relaxed);
    m_decoderChannels.store(m_decoder.outputChannels, std::memory_order_relaxed);
    m_cursorFrames.store(0, std::memory_order_relaxed);
    m_isDecoderInitialized.store(true, std::memory_order_release);

    ma_device_config deviceConfig = ma_device_config_init(ma_device_type_playback);
    deviceConfig.playback.pDeviceID = m_useSpecificDevice ? &m_selectedDeviceID : NULL;
    deviceConfig.playback.format = ma_format_f32;
    deviceConfig.playback.channels = m_decoder.outputChannels;
    deviceConfig.sampleRate = m_decoder.outputSampleRate;
    deviceConfig.dataCallback = dataCallback;
    deviceConfig.pUserData = this;

    result = ma_device_init(NULL, &deviceConfig, &m_device);
    if (result != MA_SUCCESS) {
        std::cerr << "Failed to open playback device. (Error: " << result << ")" << std::endl;
        m_isDecoderInitialized.store(false, std::memory_order_release);
        ma_decoder_uninit(&m_decoder);
        return false;
    }
    m_isDeviceInitialized = true;
    m_isCaptureMode.store(false, std::memory_order_release);
    m_currentSampleRate.store(m_decoder.outputSampleRate, std::memory_order_release);
    std::cout << "Audio device initialized: " << m_device.playback.name << std::endl;
    return true;
}

bool AudioEngine::startCapture(const ma_device_id* pID, ma_device_type type) {
    resetDevice();

    ma_device_config deviceConfig = ma_device_config_init(type);
    if (type == ma_device_type_loopback) {
        deviceConfig.playback.pDeviceID = const_cast<ma_device_id*>(pID);
    } else {
        deviceConfig.capture.pDeviceID = const_cast<ma_device_id*>(pID);
    }

    deviceConfig.capture.format = ma_format_f32;
    deviceConfig.capture.channels = 2;
    deviceConfig.sampleRate = 48000;
    deviceConfig.dataCallback = dataCallback;
    deviceConfig.pUserData = this;

    if (ma_device_init(NULL, &deviceConfig, &m_device) != MA_SUCCESS) {
        std::cerr << "Failed to initialize capture device" << std::endl;
        return false;
    }

    // Publish capture mode & sample rate BEFORE starting the audio callback thread!
    m_isDeviceInitialized = true;
    m_currentSampleRate.store(m_device.sampleRate > 0 ? m_device.sampleRate : 48000, std::memory_order_release);
    m_isCaptureMode.store(true, std::memory_order_release);

    if (ma_device_start(&m_device) != MA_SUCCESS) {
        std::cerr << "Failed to start capture device" << std::endl;
        m_isCaptureMode.store(false, std::memory_order_release);
        ma_device_uninit(&m_device);
        m_isDeviceInitialized = false;
        return false;
    }

    m_isPlaying.store(true, std::memory_order_release);
    return true;
}

bool AudioEngine::initTestTone() {
    stop();
    // Re-initialize if device is uninitialized, in capture mode, or mismatched channel/sample-rate
    if (m_isDeviceInitialized &&
        !m_isCaptureMode.load(std::memory_order_acquire) &&
        m_device.playback.channels == 2 &&
        m_device.sampleRate == 44100) {
        m_oscMusicMode.store(false, std::memory_order_release);
        m_testTone.store(true, std::memory_order_release);
        return true;
    }

    resetDevice();

    ma_device_config deviceConfig = ma_device_config_init(ma_device_type_playback);
    deviceConfig.playback.pDeviceID = m_useSpecificDevice ? &m_selectedDeviceID : NULL;
    deviceConfig.playback.format = ma_format_f32;
    deviceConfig.playback.channels = 2;
    deviceConfig.sampleRate = 44100;
    deviceConfig.dataCallback = dataCallback;
    deviceConfig.pUserData = this;

    ma_result result = ma_device_init(NULL, &deviceConfig, &m_device);
    if (result != MA_SUCCESS) {
        std::cerr << "Failed to init device for test tone. Error: " << result << std::endl;
        return false;
    }
    m_isDeviceInitialized = true;
    m_isCaptureMode.store(false, std::memory_order_release);
    m_testTone.store(true, std::memory_order_release);
    m_currentSampleRate.store(m_device.sampleRate > 0 ? m_device.sampleRate : 44100, std::memory_order_release);
    return true;
}

bool AudioEngine::initOscMusic(int sampleRate) {
    stop();
    resetDevice();

    const ma_uint32 clampedRate = static_cast<ma_uint32>(std::clamp(sampleRate, 8000, 384000));
    ma_device_config deviceConfig = ma_device_config_init(ma_device_type_playback);
    deviceConfig.playback.pDeviceID = m_useSpecificDevice ? &m_selectedDeviceID : NULL;
    deviceConfig.playback.format = ma_format_f32;
    deviceConfig.playback.channels = 2;
    deviceConfig.sampleRate = clampedRate;
    deviceConfig.dataCallback = dataCallback;
    deviceConfig.pUserData = this;

    ma_result result = ma_device_init(NULL, &deviceConfig, &m_device);
    if (result != MA_SUCCESS) {
        std::cerr << "Failed to init device for OscMusic at " << clampedRate << "Hz. Error: " << result << std::endl;
        return false;
    }
    m_isDeviceInitialized = true;
    m_isCaptureMode.store(false, std::memory_order_release);
    m_currentSampleRate.store(m_device.sampleRate > 0 ? m_device.sampleRate : clampedRate, std::memory_order_release);
    std::cout << "OscMusic initialized at " << m_currentSampleRate.load(std::memory_order_relaxed) << "Hz" << std::endl;
    return true;
}

void AudioEngine::play() {
    if (m_isDeviceInitialized) {
        m_trackEnded.store(false, std::memory_order_release);
        ma_result result = ma_device_start(&m_device);
        if (result != MA_SUCCESS) {
            std::cerr << "Failed to start playback device. (Error: " << result << ")" << std::endl;
            m_isPlaying.store(false, std::memory_order_release);
        } else {
            m_isPlaying.store(true, std::memory_order_release);
            std::cout << "Playback started." << std::endl;
        }
    }
}

void AudioEngine::pause() {
    if (m_isDeviceInitialized) {
        ma_device_stop(&m_device);
        m_isPlaying.store(false, std::memory_order_release);
    }
}

void AudioEngine::stop() {
    if (m_isDeviceInitialized) {
        ma_device_stop(&m_device);
    }
    m_isPlaying.store(false, std::memory_order_release);
}

void AudioEngine::stopCapture() {
    if (m_isCaptureMode.load(std::memory_order_acquire)) {
        ma_device_stop(&m_device);
        ma_device_uninit(&m_device);
        m_isDeviceInitialized = false;
        m_isCaptureMode.store(false, std::memory_order_release);
        m_isPlaying.store(false, std::memory_order_release);
    }
}

float AudioEngine::getPosition() const {
    if (!m_isDecoderInitialized.load(std::memory_order_acquire)) return 0.0f;
    const ma_uint32 rate = m_decoderSampleRate.load(std::memory_order_relaxed);
    if (rate == 0) return 0.0f;
    return static_cast<float>(m_cursorFrames.load(std::memory_order_relaxed)) / static_cast<float>(rate);
}

void AudioEngine::seekTo(float seconds) {
    if (!m_isDecoderInitialized.load(std::memory_order_acquire)) return;
    const ma_uint32 rate = m_decoderSampleRate.load(std::memory_order_relaxed);
    if (rate == 0) return;

    const float clampedSeconds = std::max(seconds, 0.0f);
    std::uint64_t frameIndex = static_cast<std::uint64_t>(clampedSeconds * static_cast<float>(rate));
    const std::uint64_t totalFrames = m_durationFrames.load(std::memory_order_relaxed);
    if (totalFrames > 0) {
        frameIndex = std::min(frameIndex, totalFrames);
    }

    m_trackEnded.store(false, std::memory_order_release);
    m_cursorFrames.store(frameIndex, std::memory_order_relaxed);
    if (m_isPlaying.load(std::memory_order_acquire)) {
        // Defer decoder seek to the audio callback so only the audio thread touches m_decoder.
        m_pendingSeekFrame.store(static_cast<std::int64_t>(frameIndex), std::memory_order_release);
    } else {
        m_pendingSeekFrame.store(-1, std::memory_order_release);
        ma_decoder_seek_to_pcm_frame(&m_decoder, static_cast<ma_uint64>(frameIndex));
    }
    markXYDiscontinuity();
}

float AudioEngine::getDuration() const {
    if (!m_isDecoderInitialized.load(std::memory_order_acquire)) return 0.0f;
    const ma_uint32 rate = m_decoderSampleRate.load(std::memory_order_relaxed);
    if (rate == 0) return 0.0f;
    return static_cast<float>(m_durationFrames.load(std::memory_order_relaxed)) / static_cast<float>(rate);
}

ma_uint32 AudioEngine::getSampleRate() const {
    return m_currentSampleRate.load(std::memory_order_relaxed);
}

size_t AudioEngine::getChannels() const {
    if (!m_isDecoderInitialized.load(std::memory_order_acquire)) return 0;
    return static_cast<size_t>(m_decoderChannels.load(std::memory_order_relaxed));
}

void AudioEngine::getBuffer(std::vector<float>& buffer, size_t size) {
    getChannelBuffer(buffer, size, 0);
}

void AudioEngine::getChannelBuffer(std::vector<float>& buffer, size_t size, int channel) {
    const size_t clampedSize = std::min(size, kRingCapacity);
    buffer.assign(clampedSize, 0.0f);
    if (clampedSize == 0) return;

    for (int attempt = 0; attempt < 3; ++attempt) {
        const std::uint64_t gen1 = m_xyDiscontinuityGeneration.load(std::memory_order_acquire);
        const std::uint64_t end = m_writeCounter.load(std::memory_order_acquire);
        const std::uint64_t validStart = m_validStartFrame.load(std::memory_order_acquire);
        const std::uint64_t oldest = std::max(validStart, end > kRingCapacity ? end - kRingCapacity : 0);
        const size_t available = end > oldest ? static_cast<size_t>(end - oldest) : 0;
        if (available == 0) return;

        const size_t count = std::min(clampedSize, available);
        const size_t dstOffset = clampedSize - count;
        const std::uint64_t first = end - count;

        if (dstOffset > 0) {
            std::fill_n(buffer.begin(), dstOffset, 0.0f);
        }

        if (channel == 1) {
            for (size_t i = 0; i < count; ++i) {
                const size_t slot = static_cast<size_t>((first + i) & kRingMask);
                buffer[dstOffset + i] = m_leftRing[slot].load(std::memory_order_relaxed);
            }
        } else if (channel == 2) {
            for (size_t i = 0; i < count; ++i) {
                const size_t slot = static_cast<size_t>((first + i) & kRingMask);
                buffer[dstOffset + i] = m_rightRing[slot].load(std::memory_order_relaxed);
            }
        } else {
            for (size_t i = 0; i < count; ++i) {
                const size_t slot = static_cast<size_t>((first + i) & kRingMask);
                buffer[dstOffset + i] =
                    (m_leftRing[slot].load(std::memory_order_relaxed) +
                     m_rightRing[slot].load(std::memory_order_relaxed)) * 0.5f;
            }
        }

        const std::uint64_t after = m_writeCounter.load(std::memory_order_acquire);
        const std::uint64_t gen2 = m_xyDiscontinuityGeneration.load(std::memory_order_acquire);
        if (gen1 == gen2 && (after - first) <= kRingCapacity) return;
    }
}

std::vector<AudioEngine::DeviceInfo> AudioEngine::getAvailableDevices(bool capture) {
    std::vector<DeviceInfo> devices;
    ma_context context;
    if (ma_context_init(NULL, 0, NULL, &context) != MA_SUCCESS) return devices;

    ma_device_info* pPlaybackInfos = nullptr;
    ma_uint32 playbackCount = 0;
    ma_device_info* pCaptureInfos = nullptr;
    ma_uint32 captureCount = 0;

    if (ma_context_get_devices(&context, &pPlaybackInfos, &playbackCount, &pCaptureInfos, &captureCount) != MA_SUCCESS) {
        ma_context_uninit(&context);
        return devices;
    }

    if (capture) {
        for (ma_uint32 i = 0; i < captureCount; ++i) {
            devices.push_back({ pCaptureInfos[i].id, pCaptureInfos[i].name, (bool)pCaptureInfos[i].isDefault, ma_device_type_capture });
        }
#ifdef _WIN32
        for (ma_uint32 i = 0; i < playbackCount; ++i) {
            std::string name = "[Loopback] " + std::string(pPlaybackInfos[i].name);
            devices.push_back({ pPlaybackInfos[i].id, name, false, ma_device_type_loopback });
        }
#endif
    } else {
        for (ma_uint32 i = 0; i < playbackCount; ++i) {
            devices.push_back({ pPlaybackInfos[i].id, pPlaybackInfos[i].name, (bool)pPlaybackInfos[i].isDefault, ma_device_type_playback });
        }
    }

    ma_context_uninit(&context);
    return devices;
}

void AudioEngine::setDevice(const ma_device_id* pID) {
    if (pID) {
        m_selectedDeviceID = *pID;
        m_useSpecificDevice = true;
    } else {
        m_useSpecificDevice = false;
    }
}

void AudioEngine::pushSyntheticFrames(size_t frameCount, double& phase, ma_uint32 sampleRate) {
    m_currentSampleRate.store(sampleRate, std::memory_order_relaxed);
    const double dt = 1.0 / static_cast<double>(std::max<ma_uint32>(sampleRate, 1));
    for (size_t i = 0; i < frameCount; ++i) {
        const double t = phase;
        const float left = static_cast<float>(
            0.55 * std::sin(2.0 * 3.14159265358979323846 * 110.0 * t) +
            0.25 * std::sin(2.0 * 3.14159265358979323846 * 440.0 * t) +
            0.15 * std::sin(2.0 * 3.14159265358979323846 * 2200.0 * t));
        const float right = static_cast<float>(
            0.55 * std::cos(2.0 * 3.14159265358979323846 * 110.0 * t) +
            0.25 * std::sin(2.0 * 3.14159265358979323846 * 660.0 * t) +
            0.15 * std::cos(2.0 * 3.14159265358979323846 * 3300.0 * t));
        writeFrameRT(left, right);
        phase += dt;
    }
}
