#include "VideoRenderManager.hpp"
#include <iostream>
#include <GL/glew.h>

VideoRenderManager::VideoRenderManager() {}

VideoRenderManager::~VideoRenderManager() {
    if (m_pipe) pclose(m_pipe);
}

std::string VideoRenderManager::buildFfmpegCommand(const AppState& state) {
    const auto& s = state.videoSettings;
    std::string codec;
    std::string extraParams;

    // Codecs: 0=libx264, 1=h264_nvenc, 2=h264_vaapi, 3=h264_amf
    switch (s.codecIdx) {
        case 1: 
            codec = "h264_nvenc"; 
            extraParams = "-rc vbr -cq " + std::to_string(s.crf) + " -preset slow";
            break;
        case 2: 
            codec = "h264_vaapi"; 
            extraParams = "-vaapi_device /dev/dri/renderD128 -qp " + std::to_string(s.crf);
            break;
        case 3: 
            codec = "h264_amf"; 
            extraParams = "-rc vbr -qp_cb " + std::to_string(s.crf) + " -qp_cr " + std::to_string(s.crf);
            break;
        default: 
            codec = "libx264"; 
            extraParams = "-crf " + std::to_string(s.crf) + " -preset veryslow -pix_fmt yuv420p";
            break;
    }

    // Default vflip because OpenGL is bottom-to-top. 
    // For VAAPI, we must vflip BEFORE hwupload or use vflip_vaapi.
    std::string filter;
    if (s.codecIdx == 2) {
        filter = "-vf \"vflip,format=nv12,hwupload\"";
    } else {
        filter = "-vf \"vflip,colorspace=all=bt709:iall=bt709\"";
    }

    std::string cmd = "ffmpeg -y -f rawvideo -vcodec rawvideo -s " + 
                      std::to_string(s.width) + "x" + std::to_string(s.height) + 
                      " -pix_fmt rgb24 -r " + std::to_string(s.fps) + 
                      " -i - -i \"" + std::string(state.filePath) + 
                      "\" " + filter + " -c:v " + codec + " " + extraParams + 
                      " -c:a copy -shortest \"" + std::string(s.outputPath) + "\"";
    
    return cmd;
}

void VideoRenderManager::startRender(AppState& state, AudioEngine& audioEngine) {
    if (state.videoStatus.isRendering) return;

    state.videoStatus.isRendering = true;
    state.videoStatus.progress = 0.0f;
    state.videoStatus.currentFrame = 0;
    
    float duration = audioEngine.getDuration();
    state.videoStatus.totalFrames = (int)(duration * state.videoSettings.fps);
    m_sampleRate = audioEngine.getSampleRate();
    m_dt = 1.0 / (double)state.videoSettings.fps;

    std::string cmd = buildFfmpegCommand(state);
    std::cout << "Starting FFmpeg render: " << cmd << std::endl;
    
    m_pipe = popen(cmd.c_str(), "w");
    if (!m_pipe) {
        state.videoStatus.isRendering = false;
        state.videoStatus.errorMessage = "Failed to open FFmpeg pipe.";
        return;
    }

    m_pixelBuffer.resize(state.videoSettings.width * state.videoSettings.height * 3);
    audioEngine.pause();
}

void VideoRenderManager::update(
    AppState& state, 
    AudioEngine& audioEngine, 
    AnalysisEngine& analysisEngine, 
    Visualizer& visualizer, 
    ParticleSystem& particleSystem,
    RenderManager& renderManager
) {
    if (!state.videoStatus.isRendering || !m_pipe) return;

    if (state.videoStatus.currentFrame >= state.videoStatus.totalFrames) {
        pclose(m_pipe);
        m_pipe = nullptr;
        state.videoStatus.isRendering = false;
        state.statusMessage = "Video rendering complete!";
        state.statusColor = ImVec4(0, 1, 0, 1);
        return;
    }

    // Render one frame
    int f = state.videoStatus.currentFrame;
    size_t offset = (size_t)((double)f * m_dt * (double)m_sampleRate); 
    
    std::vector<float> frameAudio;
    std::vector<float> monoAudio;
    
    audioEngine.readAudioFrames(offset, 8192, frameAudio); 
    monoAudio.resize(frameAudio.size() / 2);
    for(size_t i=0; i<monoAudio.size(); ++i) {
        monoAudio[i] = (frameAudio[i*2] + frameAudio[i*2+1]) * 0.5f;
    }

    renderManager.renderOfflineFrame(
        state,
        frameAudio,
        monoAudio,
        analysisEngine,
        visualizer,
        particleSystem,
        state.videoSettings.width,
        state.videoSettings.height,
        (float)m_dt
    );

    glReadPixels(0, 0, state.videoSettings.width, state.videoSettings.height, GL_RGB, GL_UNSIGNED_BYTE, m_pixelBuffer.data());
    glBindFramebuffer(GL_FRAMEBUFFER, 0); // Restore default FBO for UI
    fwrite(m_pixelBuffer.data(), 1, m_pixelBuffer.size(), m_pipe);

    state.videoStatus.currentFrame++;
    state.videoStatus.progress = (float)state.videoStatus.currentFrame / state.videoStatus.totalFrames;
}

void VideoRenderManager::cancelRender(AppState& state) {
    if (m_pipe) {
        pclose(m_pipe);
        m_pipe = nullptr;
    }
    state.videoStatus.isRendering = false;
    state.videoStatus.progress = 0.0f;
}
