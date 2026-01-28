#pragma once
#include <string>

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
    std::string getGpuInfo() const { return m_gpuInfo; }

private:
    void updateRam();
    void updateCpu();
    
    float m_processCpuUsage = 0.0f;
    float m_globalCpuUsage = 0.0f;
    float m_ramUsageMB = 0.0f;
    std::string m_gpuInfo = "N/A";
    
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
