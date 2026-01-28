#define MINIAUDIO_IMPLEMENTATION
#include "AudioEngine.hpp"
#include <iostream>
#include <algorithm>
#include <cmath>

AudioEngine::AudioEngine() {
}

AudioEngine::~AudioEngine() {
    if (m_isDeviceInitialized) {
        ma_device_uninit(&m_device);
    }
    if (m_isDecoderInitialized) {
        ma_decoder_uninit(&m_decoder);
    }
}

void AudioEngine::dataCallback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    AudioEngine* pEngine = (AudioEngine*)pDevice->pUserData;
    if (pEngine == nullptr) return;

    ma_uint32 framesRead = 0;
    if (pEngine->m_testTone) {
        if (pOutput && !pInput) { // Only generate test tone if in playback mode
            float* pOutputF32 = (float*)pOutput;
            
            std::lock_guard<std::mutex> lock(pEngine->m_bufferMutex);
            if (pEngine->m_circularBuffer.size() < 8192) {
                pEngine->m_circularBuffer.resize(8192, 0.0f);
            }
            if (pEngine->m_stereoBuffer.size() < 16384) {
                pEngine->m_stereoBuffer.resize(16384, 0.0f);
            }

            for (ma_uint32 i = 0; i < frameCount; ++i) {
                float sampleLeft = 0.0f;
                float sampleRight = 0.0f;
                
                // Generate left channel
                float phaseL = pEngine->m_testTonePhase;
                switch (pEngine->m_testToneType) {
                    case ToneSine: sampleLeft = std::sin(phaseL); break;
                    case ToneSquare: sampleLeft = (phaseL < M_PI) ? 1.0f : -1.0f; break;
                    case ToneSaw: sampleLeft = (phaseL / M_PI) - 1.0f; break;
                    case ToneTriangle: sampleLeft = (phaseL < M_PI) ? (2.0f * phaseL / M_PI - 1.0f) : (3.0f - 2.0f * phaseL / M_PI); break;
                    case ToneNoise: sampleLeft = ((float)rand() / RAND_MAX) * 2.0f - 1.0f; break;
                }
                sampleLeft *= pEngine->m_testToneVolume;
                
                // Generate right channel (different frequency if stereo mode)
                if (pEngine->m_testToneStereo) {
                    float phaseR = pEngine->m_testTonePhaseRight;
                    switch (pEngine->m_testToneType) {
                        case ToneSine: sampleRight = std::sin(phaseR); break;
                        case ToneSquare: sampleRight = (phaseR < M_PI) ? 1.0f : -1.0f; break;
                        case ToneSaw: sampleRight = (phaseR / M_PI) - 1.0f; break;
                        case ToneTriangle: sampleRight = (phaseR < M_PI) ? (2.0f * phaseR / M_PI - 1.0f) : (3.0f - 2.0f * phaseR / M_PI); break;
                        case ToneNoise: sampleRight = ((float)rand() / RAND_MAX) * 2.0f - 1.0f; break;
                    }
                    sampleRight *= pEngine->m_testToneVolume;
                } else {
                    sampleRight = sampleLeft; // Mono mode: duplicate left
                }

                // Write to output
                pOutputF32[i * 2 + 0] = sampleLeft;
                pOutputF32[i * 2 + 1] = sampleRight;

                // Write to circular buffer (Mono mix)
                float monoSample = (sampleLeft + sampleRight) * 0.5f;
                pEngine->m_circularBuffer[pEngine->m_writePos] = monoSample;
                pEngine->m_writePos = (pEngine->m_writePos + 1) % pEngine->m_circularBuffer.size();

                // Write to stereo buffer
                pEngine->m_stereoBuffer[pEngine->m_stereoWritePos] = sampleLeft;
                pEngine->m_stereoBuffer[(pEngine->m_stereoWritePos + 1) % pEngine->m_stereoBuffer.size()] = sampleRight;
                pEngine->m_stereoWritePos = (pEngine->m_stereoWritePos + 2) % pEngine->m_stereoBuffer.size();

                // Advance phases
                pEngine->m_testTonePhase += 2.0f * M_PI * pEngine->m_testToneFrequency / 44100.0f;
                if (pEngine->m_testTonePhase > 2.0f * M_PI) pEngine->m_testTonePhase -= 2.0f * M_PI;
                
                if (pEngine->m_testToneStereo) {
                    pEngine->m_testTonePhaseRight += 2.0f * M_PI * pEngine->m_testToneFrequencyRight / 44100.0f;
                    if (pEngine->m_testTonePhaseRight > 2.0f * M_PI) pEngine->m_testTonePhaseRight -= 2.0f * M_PI;
                }
            }
            return;
        }
    } // Done for test tone
    else if (pEngine->m_oscMusicMode) {
        if (pOutput && !pInput) {
            float* pOutputF32 = (float*)pOutput;
            std::lock_guard<std::mutex> lock(pEngine->m_bufferMutex);
            
            if (pEngine->m_oscMusicBuffer.empty()) return;

            for (ma_uint32 i = 0; i < frameCount; ++i) {
                float sampleL = 0.0f;
                float sampleR = 0.0f;
                
                if (pEngine->m_oscMusicReadPos + 1 < pEngine->m_oscMusicBuffer.size()) {
                    sampleL = pEngine->m_oscMusicBuffer[pEngine->m_oscMusicReadPos];
                    sampleR = pEngine->m_oscMusicBuffer[pEngine->m_oscMusicReadPos + 1];
                    pEngine->m_oscMusicReadPos += 2;
                } else {
                    // Loop or stop? Let's loop for preview
                    pEngine->m_oscMusicReadPos = 0;
                    sampleL = pEngine->m_oscMusicBuffer[0];
                    sampleR = pEngine->m_oscMusicBuffer[1];
                    pEngine->m_oscMusicReadPos = 2;
                }

                pOutputF32[i * 2 + 0] = sampleL;
                pOutputF32[i * 2 + 1] = sampleR;

                // Update buffers for visualization
                if (pEngine->m_circularBuffer.size() < 8192) pEngine->m_circularBuffer.resize(8192, 0.0f);
                pEngine->m_circularBuffer[pEngine->m_writePos] = (sampleL + sampleR) * 0.5f;
                pEngine->m_writePos = (pEngine->m_writePos + 1) % pEngine->m_circularBuffer.size();

                if (pEngine->m_stereoBuffer.size() < 16384) pEngine->m_stereoBuffer.resize(16384, 0.0f);
                pEngine->m_stereoBuffer[pEngine->m_stereoWritePos] = sampleL;
                pEngine->m_stereoBuffer[(pEngine->m_stereoWritePos + 1) % pEngine->m_stereoBuffer.size()] = sampleR;
                pEngine->m_stereoWritePos = (pEngine->m_stereoWritePos + 2) % pEngine->m_stereoBuffer.size();
            }
        }
        return;
    }
    else if (pEngine->m_isCaptureMode) {
        if (pInput) {
            float* pInputF32 = (float*)pInput;
            size_t channels = pEngine->m_device.capture.channels;
            // We just need to copy input to our circular buffer
            // pOutput is usually null in capture-only mode, but miniaudio might expect us to fill it if it's duplex
            if (pOutput) memset(pOutput, 0, frameCount * channels * sizeof(float));
            
            std::lock_guard<std::mutex> lock(pEngine->m_bufferMutex);
            if (pEngine->m_circularBuffer.size() < 8192) pEngine->m_circularBuffer.resize(8192, 0.0f);
            
            if (pEngine->m_stereoBuffer.size() < 16384) pEngine->m_stereoBuffer.resize(16384, 0.0f);

            for (ma_uint32 i = 0; i < frameCount; ++i) {
                float monoSample = 0;
                float left = 0;
                float right = 0;

                if (channels >= 2) {
                    left = pInputF32[i * channels + 0];
                    right = pInputF32[i * channels + 1];
                    monoSample = (left + right) * 0.5f;
                } else if (channels == 1) {
                    left = right = monoSample = pInputF32[i * channels + 0];
                }

                // Mono for FFT
                pEngine->m_circularBuffer[pEngine->m_writePos] = monoSample;
                pEngine->m_writePos = (pEngine->m_writePos + 1) % pEngine->m_circularBuffer.size();
                
                // Stereo for XY
                pEngine->m_stereoBuffer[pEngine->m_stereoWritePos] = left;
                pEngine->m_stereoBuffer[(pEngine->m_stereoWritePos + 1) % pEngine->m_stereoBuffer.size()] = right;
                pEngine->m_stereoWritePos = (pEngine->m_stereoWritePos + 2) % pEngine->m_stereoBuffer.size();
            }
            return; // Skip the rest of the callback for capture
        }
    } else if (pEngine->m_isDecoderInitialized) {
        ma_uint64 framesRead64 = 0;
        ma_result result = ma_decoder_read_pcm_frames(&pEngine->m_decoder, pOutput, frameCount, &framesRead64);
        framesRead = (ma_uint32)framesRead64;
        
        if (result != MA_SUCCESS) {
            static int errorCounter = 0;
            if (++errorCounter % 100 == 0) {
                std::cerr << "Decoder read error: " << result << std::endl;
            }
        }

        if (framesRead < frameCount) {
            float* pOutputF32 = (float*)pOutput;
            size_t channels = pEngine->m_decoder.outputChannels;
            if (pOutput) memset(pOutputF32 + framesRead * channels, 0, (frameCount - framesRead) * channels * sizeof(float));
            ma_decoder_seek_to_pcm_frame(&pEngine->m_decoder, 0);
        }
    } else {
        if (pOutput) memset(pOutput, 0, frameCount * 2 * sizeof(float));
    }
    
    if (framesRead > 0) {
        std::lock_guard<std::mutex> lock(pEngine->m_bufferMutex);
        float* pOutputF32 = (float*)pOutput;
        size_t channels = pEngine->m_isDecoderInitialized ? pEngine->m_decoder.outputChannels : 2;
        
        // Ensure circular buffers are large enough
        // Mono buffer: 8192 samples
        if (pEngine->m_circularBuffer.size() < 8192) {
            pEngine->m_circularBuffer.resize(8192, 0.0f);
        }
        // Stereo buffer: 8192 frames * 2 channels = 16384 samples
        if (pEngine->m_stereoBuffer.size() < 16384) {
            pEngine->m_stereoBuffer.resize(16384, 0.0f);
        }

        for (ma_uint32 i = 0; i < framesRead; ++i) {
            float left = 0.0f;
            float right = 0.0f;
            
            if (channels >= 2) {
                left = pOutputF32[i * channels + 0];
                right = pOutputF32[i * channels + 1];
            } else if (channels == 1) {
                left = right = pOutputF32[i * channels + 0];
            }

            // Mono Mix for FFT
            pEngine->m_circularBuffer[pEngine->m_writePos] = (left + right) * 0.5f;
            pEngine->m_writePos = (pEngine->m_writePos + 1) % pEngine->m_circularBuffer.size();

            // Stereo Interleaved for XY
            pEngine->m_stereoBuffer[pEngine->m_stereoWritePos] = left;
            pEngine->m_stereoBuffer[(pEngine->m_stereoWritePos + 1) % pEngine->m_stereoBuffer.size()] = right;
            pEngine->m_stereoWritePos = (pEngine->m_stereoWritePos + 2) % pEngine->m_stereoBuffer.size();
        }
    }

    if (framesRead < frameCount) {
        ma_decoder_seek_to_pcm_frame(&pEngine->m_decoder, 0);
    }
}

bool AudioEngine::readAudioFrames(size_t offsetFrames, size_t count, std::vector<float>& outBuffer) {
    if (!m_isDecoderInitialized) return false;
    
    // We need to protect the decoder since it's shared with the audio thread (though usually we pause before this)
    std::lock_guard<std::mutex> lock(m_bufferMutex); 

    ma_result result = ma_decoder_seek_to_pcm_frame(&m_decoder, (ma_uint64)offsetFrames);
    if (result != MA_SUCCESS) return false;

    size_t channels = m_decoder.outputChannels;
    outBuffer.resize(count * channels);

    ma_uint64 framesRead = 0;
    result = ma_decoder_read_pcm_frames(&m_decoder, outBuffer.data(), (ma_uint64)count, &framesRead);
    
    if (framesRead < count) {
        // Zero pad if we hit EOF
        std::fill(outBuffer.begin() + framesRead * channels, outBuffer.end(), 0.0f);
    }

    return (result == MA_SUCCESS);
}

void AudioEngine::getStereoBuffer(std::vector<float>& buffer, size_t frames) {
    std::lock_guard<std::mutex> lock(m_bufferMutex);
    if (m_stereoBuffer.empty()) return;
    
    size_t samples = frames * 2;
    buffer.resize(samples);
    
    // We want the most recent 'frames'
    // m_stereoWritePos points to the next write slot (interleaved index)
    
    size_t bufferSize = m_stereoBuffer.size();
    size_t readPos = (m_stereoWritePos + bufferSize - samples) % bufferSize;
    
    for (size_t i = 0; i < samples; ++i) {
        buffer[i] = m_stereoBuffer[(readPos + i) % bufferSize];
    }
}

void AudioEngine::resetDevice() {
    stop(); // Stop playback
    if (m_isDeviceInitialized) {
        ma_device_uninit(&m_device);
        m_isDeviceInitialized = false;
    }
    m_isCaptureMode = false;
    m_isPlaying = false;
}

bool AudioEngine::loadFile(const std::string& filePath) {
    if (filePath.empty()) return false;
    m_isPlaying = false;
    std::cout << "Attempting to load file: " << filePath << std::endl;

    // Check if file exists and get size
    FILE* f = fopen(filePath.c_str(), "rb");
    if (!f) {
        std::cerr << "Failed to open file for reading: " << filePath << std::endl;
        return false;
    }
    fseek(f, 0, SEEK_END);
    long fileSize = ftell(f);
    fclose(f);
    std::cout << "File size: " << fileSize << " bytes" << std::endl;

    resetDevice(); // Clean up previous device
    
    if (m_isDecoderInitialized) {
        ma_decoder_uninit(&m_decoder);
        m_isDecoderInitialized = false;
    }

    ma_decoder_config decoderConfig = ma_decoder_config_init(ma_format_f32, 0, 0);
    ma_result result = ma_decoder_init_file(filePath.c_str(), &decoderConfig, &m_decoder);
    if (result != MA_SUCCESS) {
        std::cerr << "Failed to load audio file: " << filePath << " (Error: " << result << ")" << std::endl;
        return false;
    }
    m_isDecoderInitialized = true;

    ma_device_config deviceConfig = ma_device_config_init(ma_device_type_playback);
    deviceConfig.playback.pDeviceID = m_useSpecificDevice ? &m_selectedDeviceID : NULL;
    deviceConfig.playback.format   = ma_format_f32;
    deviceConfig.playback.channels = m_decoder.outputChannels;
    deviceConfig.sampleRate        = m_decoder.outputSampleRate;
    deviceConfig.dataCallback      = dataCallback;
    deviceConfig.pUserData         = this;

    result = ma_device_init(NULL, &deviceConfig, &m_device);
    if (result != MA_SUCCESS) {
        std::cerr << "Failed to open playback device. (Error: " << result << ")" << std::endl;
        ma_decoder_uninit(&m_decoder);
        m_isDecoderInitialized = false;
        return false;
    }
    m_isDeviceInitialized = true;
    m_isCaptureMode = false;
    std::cout << "Audio device initialized: " << m_device.playback.name << std::endl;
    // ...
    return true;
}

bool AudioEngine::startCapture(const ma_device_id* pID) {
    resetDevice(); // Clean up previous device (playback or capture)

    ma_device_config deviceConfig = ma_device_config_init(ma_device_type_capture);
    deviceConfig.capture.format = ma_format_f32;
    deviceConfig.capture.channels = 2;
    deviceConfig.sampleRate = 48000;
    deviceConfig.dataCallback = dataCallback;
    deviceConfig.pUserData = this;

    if (pID) {
        deviceConfig.capture.pDeviceID = (ma_device_id*)pID;
    }

    if (ma_device_init(NULL, &deviceConfig, &m_device) != MA_SUCCESS) {
        std::cerr << "Failed to initialize capture device" << std::endl;
        return false;
    }

    if (ma_device_start(&m_device) != MA_SUCCESS) {
        std::cerr << "Failed to start capture device" << std::endl;
        ma_device_uninit(&m_device);
        return false;
    }

    m_isDeviceInitialized = true;
    m_isCaptureMode = true;
    m_isPlaying = true;
    return true;
}

bool AudioEngine::initTestTone() {
    stop();
    // Initialize standard playback device if not already set up
    // But if we are in Capture Mode, we MUST reset, even if device is initialized
    if (m_isDeviceInitialized && !m_isCaptureMode) return true;
    
    resetDevice();

    ma_device_config deviceConfig = ma_device_config_init(ma_device_type_playback);
    deviceConfig.playback.pDeviceID = m_useSpecificDevice ? &m_selectedDeviceID : NULL;
    deviceConfig.playback.format   = ma_format_f32;
    deviceConfig.playback.channels = 2;
    deviceConfig.sampleRate        = 44100;
    deviceConfig.dataCallback      = dataCallback;
    deviceConfig.pUserData         = this;

    ma_result result = ma_device_init(NULL, &deviceConfig, &m_device);
    if (result != MA_SUCCESS) {
        std::cerr << "Failed to init device for test tone. Error: " << result << std::endl;
        return false;
    }
    m_isDeviceInitialized = true;
    m_isCaptureMode = false;
    m_testTone = true;
    return true;
}

bool AudioEngine::initOscMusic(int sampleRate) {
    stop();
    // Even if initialized, we want high sample rate, so re-init if needed
    resetDevice();

    ma_device_config deviceConfig = ma_device_config_init(ma_device_type_playback);
    deviceConfig.playback.pDeviceID = m_useSpecificDevice ? &m_selectedDeviceID : NULL;
    deviceConfig.playback.format   = ma_format_f32;
    deviceConfig.playback.channels = 2;
    deviceConfig.sampleRate        = sampleRate;
    deviceConfig.dataCallback      = dataCallback;
    deviceConfig.pUserData         = this;

    ma_result result = ma_device_init(NULL, &deviceConfig, &m_device);
    if (result != MA_SUCCESS) {
        std::cerr << "Failed to init device for OscMusic at " << sampleRate << "Hz. Error: " << result << std::endl;
        return false;
    }
    m_isDeviceInitialized = true;
    m_isCaptureMode = false;
    std::cout << "OscMusic initialized at " << sampleRate << "Hz" << std::endl;
    return true;
}

void AudioEngine::play() {
    if (m_isDeviceInitialized) {
        ma_result result = ma_device_start(&m_device);
        if (result != MA_SUCCESS) {
            std::cerr << "Failed to start playback device. (Error: " << result << ")" << std::endl;
            m_isPlaying = false;
        } else {
            m_isPlaying = true;
            std::cout << "Playback started." << std::endl;
        }
    }
}

void AudioEngine::pause() {
    if (m_isDeviceInitialized) {
        ma_device_stop(&m_device);
        m_isPlaying = false;
    }
}

void AudioEngine::stop() {
    if (m_isDeviceInitialized) {
        ma_device_stop(&m_device);
    }
    m_isPlaying = false;
}

void AudioEngine::stopCapture() {
    if (m_isCaptureMode) {
        ma_device_stop(&m_device);
        ma_device_uninit(&m_device);
        m_isDeviceInitialized = false;
        m_isCaptureMode = false;
        m_isPlaying = false;
    }
}

float AudioEngine::getPosition() const {
    if (!m_isDecoderInitialized) return 0;
    ma_uint64 cursor;
    ma_decoder_get_cursor_in_pcm_frames((ma_decoder*)&m_decoder, &cursor);
    return (float)cursor / m_decoder.outputSampleRate;
}
void AudioEngine::seekTo(float seconds) {
    if (!m_isDecoderInitialized) return;
    ma_uint64 frameIndex = (ma_uint64)(seconds * m_decoder.outputSampleRate);
    ma_decoder_seek_to_pcm_frame(&m_decoder, frameIndex);
}

float AudioEngine::getDuration() const {
    if (!m_isDecoderInitialized) return 0.0f;
    ma_uint64 length;
    ma_decoder_get_length_in_pcm_frames((ma_decoder*)&m_decoder, &length);
    return (float)length / m_decoder.outputSampleRate;
}

ma_uint32 AudioEngine::getSampleRate() const {
    if (m_isDecoderInitialized) return m_decoder.outputSampleRate;
    return 44100; // Default fallback
}

size_t AudioEngine::getChannels() const {
    if (!m_isDecoderInitialized) return 0;
    return m_decoder.outputChannels;
}

void AudioEngine::getBuffer(std::vector<float>& buffer, size_t size) {
    std::lock_guard<std::mutex> lock(m_bufferMutex);
    if (m_circularBuffer.empty()) return;

    buffer.resize(size);
    size_t readPos = (m_writePos + m_circularBuffer.size() - size) % m_circularBuffer.size();
    for (size_t i = 0; i < size; ++i) {
        buffer[i] = m_circularBuffer[(readPos + i) % m_circularBuffer.size()];
    }
}

void AudioEngine::getChannelBuffer(std::vector<float>& buffer, size_t size, int channel) {
    std::lock_guard<std::mutex> lock(m_bufferMutex);
    
    // channel: 0 = Mixed, 1 = Left, 2 = Right
    if (channel == 0) {
        // Mixed - use mono buffer
        if (m_circularBuffer.empty()) return;
        buffer.resize(size);
        size_t readPos = (m_writePos + m_circularBuffer.size() - size) % m_circularBuffer.size();
        for (size_t i = 0; i < size; ++i) {
            buffer[i] = m_circularBuffer[(readPos + i) % m_circularBuffer.size()];
        }
    } else {
        // Left (1) or Right (2) - extract from stereo buffer
        if (m_stereoBuffer.empty()) return;
        buffer.resize(size);
        
        // Calculate read position in stereo buffer (interleaved samples)
        size_t stereoSamples = size * 2;
        size_t bufferSize = m_stereoBuffer.size();
        size_t readPos = (m_stereoWritePos + bufferSize - stereoSamples) % bufferSize;
        
        // Extract channel: Left = even indices (0,2,4...), Right = odd indices (1,3,5...)
        int offset = (channel == 1) ? 0 : 1;
        for (size_t i = 0; i < size; ++i) {
            buffer[i] = m_stereoBuffer[(readPos + i * 2 + offset) % bufferSize];
        }
    }
}

std::vector<AudioEngine::DeviceInfo> AudioEngine::getAvailableDevices(bool capture) {
    std::vector<DeviceInfo> devices;
    ma_context context;
    if (ma_context_init(NULL, 0, NULL, &context) != MA_SUCCESS) return devices;

    ma_device_info* pPlaybackInfos;
    ma_uint32 playbackCount;
    ma_device_info* pCaptureInfos;
    ma_uint32 captureCount;

    if (ma_context_get_devices(&context, &pPlaybackInfos, &playbackCount, &pCaptureInfos, &captureCount) != MA_SUCCESS) {
        ma_context_uninit(&context);
        return devices;
    }

    if (capture) {
        for (ma_uint32 i = 0; i < captureCount; ++i) {
            devices.push_back({ pCaptureInfos[i].id, pCaptureInfos[i].name, (bool)pCaptureInfos[i].isDefault, true });
        }
    } else {
        for (ma_uint32 i = 0; i < playbackCount; ++i) {
            devices.push_back({ pPlaybackInfos[i].id, pPlaybackInfos[i].name, (bool)pPlaybackInfos[i].isDefault, false });
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
