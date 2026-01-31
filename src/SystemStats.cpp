#include "SystemStats.hpp"

#ifdef _WIN32
#include <psapi.h>
#include <GL/glew.h>
#include <string>
#include <cstdint>
#include <iostream>

SystemStats::SystemStats() {
    updateRam();
    updateCpu();
    
    // Get CPU core count
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    m_cpuCoreCount = sysInfo.dwNumberOfProcessors;
}

void SystemStats::recordFrameTime(float deltaTime) {
    m_frameTimeMs = deltaTime * 1000.0f; // Convert to ms
    
    // Track min/max
    if (m_frameTimeMs < m_minFrameTimeMs) m_minFrameTimeMs = m_frameTimeMs;
    if (m_frameTimeMs > m_maxFrameTimeMs) m_maxFrameTimeMs = m_frameTimeMs;
    
    // Accumulate for average over 1 second
    m_frameTimeAccum += deltaTime;
    m_frameTimeSamples.push_back(m_frameTimeMs);
    
    if (m_frameTimeAccum >= 1.0) {
        // Calculate average
        float sum = 0.0f;
        for (float sample : m_frameTimeSamples) sum += sample;
        m_avgFrameTimeMs = m_frameTimeSamples.empty() ? 0.0f : sum / m_frameTimeSamples.size();
        
        // Reset min/max for next second
        m_minFrameTimeMs = 999.0f;
        m_maxFrameTimeMs = 0.0f;
        m_frameTimeAccum = 0.0;
        m_frameTimeSamples.clear();
    }
}

void SystemStats::update(double dt) {
    m_updateTimer += dt;
    if (m_updateTimer >= 1.0) {
        updateRam();
        updateCpu();
        m_updateTimer = 0.0;
    }
}

void SystemStats::updateRam() {
    PROCESS_MEMORY_COUNTERS_EX pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
        m_ramUsageMB = pmc.WorkingSetSize / (1024.0f * 1024.0f);
        if (m_ramUsageMB > m_peakRamUsageMB) {
            m_peakRamUsageMB = m_ramUsageMB;
        }
    }
}

static uint64_t FileTimeToUint64(const FILETIME& ft) {
    return (((uint64_t)ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
}

void SystemStats::updateCpu() {
    // 1. Process CPU
    FILETIME creationTime, exitTime, kernelTime, userTime;
    if (GetProcessTimes(GetCurrentProcess(), &creationTime, &exitTime, &kernelTime, &userTime)) {
        uint64_t currentProcessTicks = FileTimeToUint64(kernelTime) + FileTimeToUint64(userTime);
        
        // 2. Global CPU
        FILETIME idleTime, sysKernelTime, sysUserTime;
        if (GetSystemTimes(&idleTime, &sysKernelTime, &sysUserTime)) {
            uint64_t currentTotalTicks = FileTimeToUint64(sysKernelTime) + FileTimeToUint64(sysUserTime);
            
            if (FileTimeToUint64(m_lastTotalTicks) > 0) {
                uint64_t deltaProcess = currentProcessTicks - FileTimeToUint64(m_lastProcessTicks);
                uint64_t deltaTotal = currentTotalTicks - FileTimeToUint64(m_lastTotalTicks);
                
                if (deltaTotal > 0) {
                    m_globalCpuUsage = (float)deltaProcess / (float)deltaTotal * 100.0f;
                    
                    SYSTEM_INFO sysInfo;
                    GetSystemInfo(&sysInfo);
                    m_processCpuUsage = m_globalCpuUsage * sysInfo.dwNumberOfProcessors;
                }
            }
            // Store raw ticks
            m_lastProcessTicks.dwLowDateTime = (DWORD)(currentProcessTicks & 0xFFFFFFFF);
            m_lastProcessTicks.dwHighDateTime = (DWORD)(currentProcessTicks >> 32);
            m_lastTotalTicks.dwLowDateTime = (DWORD)(currentTotalTicks & 0xFFFFFFFF);
            m_lastTotalTicks.dwHighDateTime = (DWORD)(currentTotalTicks >> 32);
        }
    }

    // GPU Info - Query VRAM if available
    const GLubyte* renderer = glGetString(GL_RENDERER);
    if (renderer) {
        m_gpuInfo = std::string((const char*)renderer);
    } else {
        m_gpuInfo = "Unknown GPU";
    }
    
    // Try to get VRAM info from NVIDIA extension
    if (glewIsExtensionSupported("GL_NVX_gpu_memory_info")) {
        GLint dedicatedMemKb = 0;
        GLint curAvailKb = 0;
        glGetIntegerv(0x9047 /* GL_GPU_MEM_INFO_DEDICATED_VIDMEM_NVX */, &dedicatedMemKb);
        glGetIntegerv(0x9049 /* GL_GPU_MEM_INFO_CURRENT_AVAILABLE_VIDMEM_NVX */, &curAvailKb);
        
        m_gpuVramTotalMB = dedicatedMemKb / 1024.0f;
        m_gpuVramUsedMB = (dedicatedMemKb - curAvailKb) / 1024.0f;
    }
    // Try AMD extension
    else if (glewIsExtensionSupported("GL_ATI_meminfo")) {
        GLint memInfo[4] = {0};
        glGetIntegerv(0x87FC /* GL_TEXTURE_FREE_MEMORY_ATI */, memInfo);
        // memInfo[0] = total free memory in KB
        // We can't get total VRAM easily from this, only free
        m_gpuVramUsedMB = 0.0f; // Can't determine used from this extension
        m_gpuVramTotalMB = 0.0f;
    } else {
        // No extension available
        m_gpuVramUsedMB = 0.0f;
        m_gpuVramTotalMB = 0.0f;
    }
}

#else
// ... (Linux Implementation remains isolated)
#include <unistd.h>
#include <vector>
#include <fstream>
#include <sstream>

SystemStats::SystemStats() {
    updateRam();
    updateCpu();
    
    // Get CPU core count
    m_cpuCoreCount = sysconf(_SC_NPROCESSORS_ONLN);
}

void SystemStats::recordFrameTime(float deltaTime) {
    m_frameTimeMs = deltaTime * 1000.0f; // Convert to ms
    
    // Track min/max
    if (m_frameTimeMs < m_minFrameTimeMs) m_minFrameTimeMs = m_frameTimeMs;
    if (m_frameTimeMs > m_maxFrameTimeMs) m_maxFrameTimeMs = m_frameTimeMs;
    
    // Accumulate for average over 1 second
    m_frameTimeAccum += deltaTime;
    m_frameTimeSamples.push_back(m_frameTimeMs);
    
    if (m_frameTimeAccum >= 1.0) {
        // Calculate average
        float sum = 0.0f;
        for (float sample : m_frameTimeSamples) sum += sample;
        m_avgFrameTimeMs = m_frameTimeSamples.empty() ? 0.0f : sum / m_frameTimeSamples.size();
        
        // Reset min/max for next second
        m_minFrameTimeMs = 999.0f;
        m_maxFrameTimeMs = 0.0f;
        m_frameTimeAccum = 0.0;
        m_frameTimeSamples.clear();
    }
}

void SystemStats::update(double dt) {
    m_updateTimer += dt;
    if (m_updateTimer >= 1.0) {
        updateRam();
        updateCpu();
        m_updateTimer = 0.0;
    }
}

void SystemStats::updateRam() {
    std::ifstream file("/proc/self/status");
    std::string line;
    while (std::getline(file, line)) {
        if (line.compare(0, 6, "VmRSS:") == 0) {
            std::stringstream ss(line.substr(7));
            long kb;
            ss >> kb;
            m_ramUsageMB = kb / 1024.0f;
            if (m_ramUsageMB > m_peakRamUsageMB) {
                m_peakRamUsageMB = m_ramUsageMB;
            }
            break;
        }
    }
}

void SystemStats::updateCpu() {
    std::ifstream pfile("/proc/self/stat");
    std::string dummy;
    for (int i = 0; i < 13; ++i) pfile >> dummy;
    long utime, stime;
    pfile >> utime >> stime;
    long totalProcessTicks = utime + stime;

    std::ifstream sfile("/proc/stat");
    sfile >> dummy;
    long user, nice, system, idle, iowait, irq, softirq, steal;
    sfile >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal;
    long totalSystemTicks = user + nice + system + idle + iowait + irq + softirq + steal;

    if (m_lastTotalTicks > 0) {
        long deltaProcess = totalProcessTicks - (long)m_lastProcessTicks;
        long deltaSystem = totalSystemTicks - (long)m_lastTotalTicks;
        if (deltaSystem > 0) {
            m_globalCpuUsage = (float)deltaProcess / (float)deltaSystem * 100.0f;
            static long numCores = sysconf(_SC_NPROCESSORS_ONLN);
            m_processCpuUsage = m_globalCpuUsage * numCores;
        }
    }
    m_lastProcessTicks = (TicksType)totalProcessTicks;
    m_lastTotalTicks = (TicksType)totalSystemTicks;
    m_gpuInfo = "N/A (Linux)";
}
#endif
