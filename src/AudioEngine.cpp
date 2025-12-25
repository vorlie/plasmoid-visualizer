#define MINIAUDIO_IMPLEMENTATION
#include "AudioEngine.hpp"
#include <iostream>
#include <algorithm>

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
        float* pOutputF32 = (float*)pOutput;
        size_t channels = pEngine->m_isDecoderInitialized ? pEngine->m_decoder.outputChannels : 2;
        if (channels == 0) channels = 2;
        
        std::lock_guard<std::mutex> lock(pEngine->m_bufferMutex);
        if (pEngine->m_circularBuffer.size() < 8192) pEngine->m_circularBuffer.resize(8192, 0.0f);

        for (ma_uint32 i = 0; i < frameCount; ++i) {
            float sample = sinf(pEngine->m_testTonePhase) * 0.2f;
            
            // Write to output if available (so we can hear it)
            if (pOutput) {
                for (size_t c = 0; c < channels; ++c) {
                    pOutputF32[i * channels + c] = sample;
                }
            }
            
            // Write to circular buffer for visualizer
            pEngine->m_circularBuffer[pEngine->m_writePos] = sample;
            pEngine->m_writePos = (pEngine->m_writePos + 1) % pEngine->m_circularBuffer.size();

            pEngine->m_testTonePhase += 2.0f * M_PI * 440.0f / 44100.0f;
            if (pEngine->m_testTonePhase > 2.0f * M_PI) pEngine->m_testTonePhase -= 2.0f * M_PI;
        }
        return; // Done for test tone
    } else if (pEngine->m_isCaptureMode) {
        if (pInput) {
            float* pInputF32 = (float*)pInput;
            size_t channels = pEngine->m_device.capture.channels;
            // We just need to copy input to our circular buffer
            // pOutput is usually null in capture-only mode, but miniaudio might expect us to fill it if it's duplex
            if (pOutput) memset(pOutput, 0, frameCount * channels * sizeof(float));
            
            std::lock_guard<std::mutex> lock(pEngine->m_bufferMutex);
            if (pEngine->m_circularBuffer.size() < 8192) pEngine->m_circularBuffer.resize(8192, 0.0f);
            
            for (ma_uint32 i = 0; i < frameCount; ++i) {
                float monoSample = 0;
                for (size_t c = 0; c < channels; ++c) {
                    monoSample += pInputF32[i * channels + c];
                }
                monoSample /= channels;
                pEngine->m_circularBuffer[pEngine->m_writePos] = monoSample;
                pEngine->m_writePos = (pEngine->m_writePos + 1) % pEngine->m_circularBuffer.size();
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
        
        // Ensure circular buffer is large enough (e.g., 8192 samples)
        if (pEngine->m_circularBuffer.size() < 8192) {
            pEngine->m_circularBuffer.resize(8192, 0.0f);
        }

        for (ma_uint32 i = 0; i < framesRead; ++i) {
            float sum = 0;
            for (size_t c = 0; c < channels; ++c) {
                sum += pOutputF32[i * channels + c];
            }
            pEngine->m_circularBuffer[pEngine->m_writePos] = sum / channels;
            pEngine->m_writePos = (pEngine->m_writePos + 1) % pEngine->m_circularBuffer.size();
        }
    }

    if (framesRead < frameCount) {
        ma_decoder_seek_to_pcm_frame(&pEngine->m_decoder, 0);
    }
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

    if (m_isDeviceInitialized) {
        ma_device_uninit(&m_device);
        m_isDeviceInitialized = false;
    }
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
    std::cout << "Audio device initialized: " << m_device.playback.name << std::endl;
    std::cout << "Decoder Format: " << m_decoder.outputFormat << ", Channels: " << m_decoder.outputChannels << ", Rate: " << m_decoder.outputSampleRate << std::endl;
    std::cout << "Device Format: " << m_device.playback.format << ", Channels: " << m_device.playback.channels << ", Rate: " << m_device.sampleRate << std::endl;
    
    ma_uint64 length;
    ma_decoder_get_length_in_pcm_frames(&m_decoder, &length);
    std::cout << "Audio duration: " << (float)length / m_decoder.outputSampleRate << " seconds (" << length << " frames)" << std::endl;

    return true;
}

bool AudioEngine::startCapture(const ma_device_id* pID) {
    stop();
    stopCapture();

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

float AudioEngine::getDuration() const {
    if (!m_isDecoderInitialized) return 0;
    ma_uint64 length;
    ma_decoder_get_length_in_pcm_frames((ma_decoder*)&m_decoder, &length);
    return (float)length / m_decoder.outputSampleRate;
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
