#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class SubprocessPipe {
public:
    enum class Mode {
        WriteStdin,
        ReadStdout
    };

    SubprocessPipe() = default;
    ~SubprocessPipe();

    SubprocessPipe(const SubprocessPipe&) = delete;
    SubprocessPipe& operator=(const SubprocessPipe&) = delete;

    // Launches executable with explicit argv (no shell). argv[0] is the executable name/path.
    bool start(const std::vector<std::string>& argv, Mode mode, bool captureStderr = false);

    // Writes all bytes to child stdin (Mode::WriteStdin). Returns false on broken pipe / error.
    bool writeAll(const void* data, size_t bytes);

    // Reads exactly `bytes` from child stdout (Mode::ReadStdout). Returns false on EOF / error.
    bool readExact(void* data, size_t bytes);

    // Signals the child process to terminate so any thread blocked in readExact() unblocks
    // without closing the parent pipe handle concurrently. Safe to call before joining reader thread.
    void terminateChild();

    // Closes pipes, joins stderr drainer, waits for child exit, and returns exit code (-1 on error).
    int closeAndWait();

    bool isOpen() const noexcept;
    std::string stderrOutput() const;

private:
    void startStderrThread();
    void stopStderrThread();

#ifdef _WIN32
    void* m_processHandle = nullptr;
    void* m_pipeHandle = nullptr;
    void* m_stderrHandle = nullptr;
#else
    int m_pid = -1;
    int m_pipeFd = -1;
    int m_stderrFd = -1;
#endif
    Mode m_mode = Mode::WriteStdin;
    std::atomic<bool> m_terminated{false};
    std::thread m_stderrThread;
    mutable std::mutex m_stderrMutex;
    std::string m_stderrBuffer;
};
