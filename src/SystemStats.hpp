// Initial thought on SystemStats.hpp
#pragma once
#include <string>

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
    long m_lastProcessTicks = 0;
    long m_lastTotalTicks = 0;
    double m_updateTimer = 0.0;
};
