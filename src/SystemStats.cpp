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

    // GPU Info - Minimal fallback on Windows
    m_gpuInfo = "N/A (Win32)";
    
    // Check OpenGL extensions if context is valid (glew might be initialized by now)
    if (glewIsExtensionSupported("GL_NVX_gpu_memory_info")) {
        GLint dedicatedMemKb = 0;
        GLint curAvailKb = 0;
        glGetIntegerv(0x9047 /* GL_GPU_MEM_INFO_DEDICATED_VIDMEM_NVX */, &dedicatedMemKb);
        glGetIntegerv(0x9049 /* GL_GPU_MEM_INFO_CURRENT_AVAILABLE_VIDMEM_NVX */, &curAvailKb);
        
        m_gpuInfo = "NV: " + std::to_string((dedicatedMemKb - curAvailKb) / 1024) + " / " + std::to_string(dedicatedMemKb / 1024) + " MB";
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
