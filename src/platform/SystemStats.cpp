#include "SystemStats.hpp"

#include <algorithm>
#include <cstdint>

#ifdef _WIN32
#include <psapi.h>
#include <GL/glew.h>
#else
#include <fstream>
#include <sstream>
#include <unistd.h>
#endif

void SystemStats::recordFrameTime(float deltaTime) {
    m_frameTimeMs = deltaTime * 1000.0f;

    if (m_frameTimeMs < m_minFrameTimeMs) m_minFrameTimeMs = m_frameTimeMs;
    if (m_frameTimeMs > m_maxFrameTimeMs) m_maxFrameTimeMs = m_frameTimeMs;

    m_frameTimeAccum += deltaTime;
    m_frameTimeSamples.push_back(m_frameTimeMs);

    if (m_frameTimeAccum >= 1.0) {
        float sum = 0.0f;
        for (float sample : m_frameTimeSamples) sum += sample;
        m_avgFrameTimeMs = m_frameTimeSamples.empty() ? 0.0f : sum / static_cast<float>(m_frameTimeSamples.size());

        m_minFrameTimeMs = 999.0f;
        m_maxFrameTimeMs = 0.0f;
        m_frameTimeAccum = 0.0;
        m_frameTimeSamples.clear();
    }
}

void SystemStats::update(double dt) {
    m_updateTimer += dt;
    if (!m_gpuInitialized || m_updateTimer >= 1.0) {
        updateRam();
        updateCpu();
        updateGpu();
        m_updateTimer = 0.0;
    }
}

#ifdef _WIN32

namespace {
uint64_t fileTimeToUint64(const FILETIME& ft) {
    return (static_cast<uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
}
}

SystemStats::SystemStats() {
    SYSTEM_INFO sysInfo{};
    GetSystemInfo(&sysInfo);
    m_cpuCoreCount = std::max<int>(1, static_cast<int>(sysInfo.dwNumberOfProcessors));
    updateRam();
    updateCpu();
}

void SystemStats::updateRam() {
    PROCESS_MEMORY_COUNTERS_EX pmc{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof(pmc))) {
        m_ramUsageMB = static_cast<float>(pmc.WorkingSetSize) / (1024.0f * 1024.0f);
        if (m_ramUsageMB > m_peakRamUsageMB) {
            m_peakRamUsageMB = m_ramUsageMB;
        }
    }
}

void SystemStats::updateCpu() {
    FILETIME creationTime{}, exitTime{}, kernelTime{}, userTime{};
    if (GetProcessTimes(GetCurrentProcess(), &creationTime, &exitTime, &kernelTime, &userTime)) {
        const uint64_t currentProcessTicks = fileTimeToUint64(kernelTime) + fileTimeToUint64(userTime);

        FILETIME idleTime{}, sysKernelTime{}, sysUserTime{};
        if (GetSystemTimes(&idleTime, &sysKernelTime, &sysUserTime)) {
            const uint64_t currentTotalTicks = fileTimeToUint64(sysKernelTime) + fileTimeToUint64(sysUserTime);

            if (fileTimeToUint64(m_lastTotalTicks) > 0) {
                const uint64_t deltaProcess = currentProcessTicks - fileTimeToUint64(m_lastProcessTicks);
                const uint64_t deltaTotal = currentTotalTicks - fileTimeToUint64(m_lastTotalTicks);
                if (deltaTotal > 0) {
                    m_globalCpuUsage = static_cast<float>(deltaProcess) / static_cast<float>(deltaTotal) * 100.0f;
                    m_processCpuUsage = m_globalCpuUsage * static_cast<float>(m_cpuCoreCount);
                }
            }
            m_lastProcessTicks.dwLowDateTime = static_cast<DWORD>(currentProcessTicks & 0xFFFFFFFFu);
            m_lastProcessTicks.dwHighDateTime = static_cast<DWORD>(currentProcessTicks >> 32);
            m_lastTotalTicks.dwLowDateTime = static_cast<DWORD>(currentTotalTicks & 0xFFFFFFFFu);
            m_lastTotalTicks.dwHighDateTime = static_cast<DWORD>(currentTotalTicks >> 32);
        }
    }
}

void SystemStats::updateGpu() {
    if (!m_gpuInitialized) {
        const GLubyte* renderer = glGetString(GL_RENDERER);
        m_gpuInfo = renderer ? std::string(reinterpret_cast<const char*>(renderer)) : "Unknown GPU";
        m_hasNvMemInfo = glewIsExtensionSupported("GL_NVX_gpu_memory_info") == GL_TRUE;
        m_gpuInitialized = true;
    }

    if (m_hasNvMemInfo) {
        GLint dedicatedMemKb = 0;
        GLint curAvailKb = 0;
        glGetIntegerv(0x9047 /* GL_GPU_MEM_INFO_DEDICATED_VIDMEM_NVX */, &dedicatedMemKb);
        glGetIntegerv(0x9049 /* GL_GPU_MEM_INFO_CURRENT_AVAILABLE_VIDMEM_NVX */, &curAvailKb);
        m_gpuVramTotalMB = dedicatedMemKb / 1024.0f;
        m_gpuVramUsedMB = (dedicatedMemKb - curAvailKb) / 1024.0f;
    } else {
        m_gpuVramUsedMB = 0.0f;
        m_gpuVramTotalMB = 0.0f;
    }
}

#else

SystemStats::SystemStats() {
    const long cores = sysconf(_SC_NPROCESSORS_ONLN);
    m_cpuCoreCount = cores > 0 ? static_cast<int>(cores) : 1;
    updateRam();
    updateCpu();
}

void SystemStats::updateRam() {
    std::ifstream file("/proc/self/status");
    std::string line;
    while (std::getline(file, line)) {
        if (line.compare(0, 6, "VmRSS:") == 0) {
            std::stringstream ss(line.substr(7));
            long kb = 0;
            if (ss >> kb) {
                m_ramUsageMB = static_cast<float>(kb) / 1024.0f;
                if (m_ramUsageMB > m_peakRamUsageMB) {
                    m_peakRamUsageMB = m_ramUsageMB;
                }
            }
            break;
        }
    }
}

void SystemStats::updateCpu() {
    std::ifstream pfile("/proc/self/stat");
    std::string statLine;
    if (!std::getline(pfile, statLine)) return;

    // Field 2 (comm) is enclosed in parentheses and may contain spaces; start
    // parsing after the last ')' so fields 3..15 remain aligned.
    const size_t closeParen = statLine.rfind(')');
    if (closeParen == std::string::npos || closeParen + 2 >= statLine.size()) return;
    std::istringstream pss(statLine.substr(closeParen + 2));
    std::string dummy;
    // Skip fields 3..13 (11 fields) to reach field 14 (utime) and 15 (stime).
    for (int i = 0; i < 11; ++i) {
        if (!(pss >> dummy)) return;
    }
    long utime = 0;
    long stime = 0;
    if (!(pss >> utime >> stime)) return;
    const long totalProcessTicks = utime + stime;

    std::ifstream sfile("/proc/stat");
    if (!(sfile >> dummy)) return;
    long user = 0, nice = 0, system = 0, idle = 0, iowait = 0, irq = 0, softirq = 0, steal = 0;
    if (!(sfile >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal)) return;
    const long totalSystemTicks = user + nice + system + idle + iowait + irq + softirq + steal;

    if (m_lastTotalTicks > 0) {
        const long deltaProcess = totalProcessTicks - static_cast<long>(m_lastProcessTicks);
        const long deltaSystem = totalSystemTicks - static_cast<long>(m_lastTotalTicks);
        if (deltaSystem > 0) {
            m_globalCpuUsage = static_cast<float>(deltaProcess) / static_cast<float>(deltaSystem) * 100.0f;
            m_processCpuUsage = m_globalCpuUsage * static_cast<float>(m_cpuCoreCount);
        }
    }
    m_lastProcessTicks = static_cast<TicksType>(totalProcessTicks);
    m_lastTotalTicks = static_cast<TicksType>(totalSystemTicks);
}

void SystemStats::updateGpu() {
    if (!m_gpuInitialized) {
        m_gpuInfo = "N/A (Linux)";
        m_gpuInitialized = true;
    }
}

#endif
