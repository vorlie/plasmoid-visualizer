#pragma once
#include <string>
#include <vector>

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
#endif

class SystemStats {
public:
    SystemStats();
    void update(double dt);
    
    float getProcessCpuUsage() const { return m_processCpuUsage; }
    float getGlobalCpuUsage() const { return m_globalCpuUsage; }
    float getRamUsageMB() const { return m_ramUsageMB; }
    float getPeakRamUsageMB() const { return m_peakRamUsageMB; }
    std::string getGpuInfo() const { return m_gpuInfo; }
    float getGpuVramUsedMB() const { return m_gpuVramUsedMB; }
    float getGpuVramTotalMB() const { return m_gpuVramTotalMB; }
    int getCpuCoreCount() const { return m_cpuCoreCount; }
    
    // Frame time statistics
    float getFrameTimeMs() const { return m_frameTimeMs; }
    float getMinFrameTimeMs() const { return m_minFrameTimeMs; }
    float getMaxFrameTimeMs() const { return m_maxFrameTimeMs; }
    float getAvgFrameTimeMs() const { return m_avgFrameTimeMs; }
    
    void recordFrameTime(float deltaTime);

private:
    void updateRam();
    void updateCpu();
    
    float m_processCpuUsage = 0.0f;
    float m_globalCpuUsage = 0.0f;
    float m_ramUsageMB = 0.0f;
    float m_peakRamUsageMB = 0.0f;
    std::string m_gpuInfo = "N/A";
    float m_gpuVramUsedMB = 0.0f;
    float m_gpuVramTotalMB = 0.0f;
    int m_cpuCoreCount = 0;
    
    // Frame time tracking
    float m_frameTimeMs = 0.0f;
    float m_minFrameTimeMs = 999.0f;
    float m_maxFrameTimeMs = 0.0f;
    float m_avgFrameTimeMs = 0.0f;
    std::vector<float> m_frameTimeSamples;
    double m_frameTimeAccum = 0.0;
    
    // CPU calculation state
#ifdef _WIN32
    typedef FILETIME TicksType;
#else
    typedef long TicksType;
#endif

    TicksType m_lastProcessTicks = {};
    TicksType m_lastTotalTicks = {};
#ifdef _WIN32
    TicksType m_lastIdleTicks = {};
    TicksType m_lastKernelTicks = {};
    TicksType m_lastUserTicks = {};
#endif
    double m_updateTimer = 0.0;
};
