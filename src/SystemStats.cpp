#include "SystemStats.hpp"
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <vector>
#include <GL/glew.h>

SystemStats::SystemStats() {
    updateRam();
    updateCpu();
}

void SystemStats::update(double dt) {
    m_updateTimer += dt;
    if (m_updateTimer >= 1.0) { // Update once per second
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
    // 1. Get process ticks
    std::ifstream pfile("/proc/self/stat");
    std::string dummy;
    for (int i = 0; i < 13; ++i) pfile >> dummy;
    long utime, stime;
    pfile >> utime >> stime;
    long totalProcessTicks = utime + stime;

    // 2. Get total system ticks
    std::ifstream sfile("/proc/stat");
    sfile >> dummy; // "cpu"
    long user, nice, system, idle, iowait, irq, softirq, steal;
    sfile >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal;
    long totalSystemTicks = user + nice + system + idle + iowait + irq + softirq + steal;
    if (m_lastTotalTicks > 0) {
        long deltaProcess = totalProcessTicks - m_lastProcessTicks;
        long deltaSystem = totalSystemTicks - m_lastTotalTicks;
        if (deltaSystem > 0) {
            m_globalCpuUsage = (float)deltaProcess / (float)deltaSystem * 100.0f;
            
            static long numCores = sysconf(_SC_NPROCESSORS_ONLN);
            m_processCpuUsage = m_globalCpuUsage * numCores;
        }
    }

    m_lastProcessTicks = totalProcessTicks;
    m_lastTotalTicks = totalSystemTicks;

    // GPU Info (Memory only on Linux via sysfs or OpenGL extensions)
    m_gpuInfo = "N/A";

    // 1. Try Linux sysfs (reliable for AMD/recent NVIDIA/Intel)
    // We check common card indices
    for (int i = 0; i < 2; ++i) {
        std::string cardPath = "/sys/class/drm/card" + std::to_string(i) + "/device/";
        std::ifstream vramTotFile(cardPath + "mem_info_vram_total");
        if (vramTotFile) {
            std::ifstream vramUsedFile(cardPath + "mem_info_vram_used");
            long long total = 0, used = 0;
            vramTotFile >> total;
            vramUsedFile >> used;
            
            if (total > 0) {
                std::stringstream ss;
                ss << used / (1024 * 1024) << " / " << total / (1024 * 1024) << " MB";
                m_gpuInfo = ss.str();
                return; // Found a valid card
            }
        }
    }
    
    // 2. Fallback to OpenGL extensions
    if (glewIsExtensionSupported("GL_NVX_gpu_memory_info")) {
#define GL_GPU_MEM_INFO_DEDICATED_VIDMEM_NVX          0x9047
#define GL_GPU_MEM_INFO_CURRENT_AVAILABLE_VIDMEM_NVX  0x9049
        GLint dedicatedMemKb = 0;
        GLint curAvailKb = 0;
        glGetIntegerv(GL_GPU_MEM_INFO_DEDICATED_VIDMEM_NVX, &dedicatedMemKb);
        glGetIntegerv(GL_GPU_MEM_INFO_CURRENT_AVAILABLE_VIDMEM_NVX, &curAvailKb);
        
        std::stringstream ss;
        ss << "NV: " << (dedicatedMemKb - curAvailKb) / 1024 << " / " << dedicatedMemKb / 1024 << " MB";
        m_gpuInfo = ss.str();
    } else if (glewIsExtensionSupported("GL_ATI_meminfo")) {
#define GL_VBO_FREE_MEMORY_ATI                     0x87FB
        GLint freeMem[4]; 
        glGetIntegerv(GL_VBO_FREE_MEMORY_ATI, freeMem);
        std::stringstream ss;
        ss << "ATI Free: " << freeMem[0] / 1024 << " MB";
        m_gpuInfo = ss.str();
    }
}
