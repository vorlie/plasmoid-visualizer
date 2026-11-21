#include "SubprocessPipe.hpp"
#include "Utf8Paths.hpp"

#include <algorithm>
#include <cstring>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#endif

namespace {
constexpr size_t kMaxStderrBytes = 8192;

#ifdef _WIN32
// Quotes a single argument according to Microsoft CommandLineToArgvW rules.
std::wstring quoteWinArg(const std::wstring& arg) {
    const bool needsQuotes = arg.empty() ||
        arg.find_first_of(L" \t\n\v\"") != std::wstring::npos;
    if (!needsQuotes) return arg;

    std::wstring out;
    out.push_back(L'"');
    size_t backslashes = 0;
    for (wchar_t ch : arg) {
        if (ch == L'\\') {
            ++backslashes;
        } else if (ch == L'"') {
            out.append(backslashes * 2 + 1, L'\\');
            out.push_back(L'"');
            backslashes = 0;
        } else {
            if (backslashes > 0) {
                out.append(backslashes, L'\\');
                backslashes = 0;
            }
            out.push_back(ch);
        }
    }
    if (backslashes > 0) {
        out.append(backslashes * 2, L'\\');
    }
    out.push_back(L'"');
    return out;
}

std::wstring buildWinCommandLine(const std::vector<std::string>& argv) {
    std::wstring cmd;
    for (size_t i = 0; i < argv.size(); ++i) {
        if (i > 0) cmd.push_back(L' ');
        cmd += quoteWinArg(Utf8Paths::toWide(argv[i]));
    }
    return cmd;
}
#endif
}

SubprocessPipe::~SubprocessPipe() {
    terminateChild();
    closeAndWait();
}

bool SubprocessPipe::isOpen() const noexcept {
#ifdef _WIN32
    return m_processHandle != nullptr;
#else
    return m_pid > 0;
#endif
}

std::string SubprocessPipe::stderrOutput() const {
    std::lock_guard<std::mutex> lock(m_stderrMutex);
    return m_stderrBuffer;
}

void SubprocessPipe::startStderrThread() {
#ifdef _WIN32
    if (!m_stderrHandle) return;
    HANDLE handle = static_cast<HANDLE>(m_stderrHandle);
    m_stderrThread = std::thread([this, handle]() {
        char buf[512];
        DWORD bytesRead = 0;
        while (ReadFile(handle, buf, sizeof(buf), &bytesRead, nullptr) && bytesRead > 0) {
            std::lock_guard<std::mutex> lock(m_stderrMutex);
            m_stderrBuffer.append(buf, bytesRead);
            if (m_stderrBuffer.size() > kMaxStderrBytes) {
                m_stderrBuffer.erase(0, m_stderrBuffer.size() - kMaxStderrBytes);
            }
        }
    });
#else
    if (m_stderrFd < 0) return;
    const int fd = m_stderrFd;
    m_stderrThread = std::thread([this, fd]() {
        char buf[512];
        for (;;) {
            const ssize_t n = ::read(fd, buf, sizeof(buf));
            if (n > 0) {
                std::lock_guard<std::mutex> lock(m_stderrMutex);
                m_stderrBuffer.append(buf, static_cast<size_t>(n));
                if (m_stderrBuffer.size() > kMaxStderrBytes) {
                    m_stderrBuffer.erase(0, m_stderrBuffer.size() - kMaxStderrBytes);
                }
            } else if (n < 0 && errno == EINTR) {
                continue;
            } else {
                break;
            }
        }
    });
#endif
}

void SubprocessPipe::stopStderrThread() {
    if (m_stderrThread.joinable()) {
        m_stderrThread.join();
    }
#ifdef _WIN32
    if (m_stderrHandle) {
        CloseHandle(static_cast<HANDLE>(m_stderrHandle));
        m_stderrHandle = nullptr;
    }
#else
    if (m_stderrFd >= 0) {
        ::close(m_stderrFd);
        m_stderrFd = -1;
    }
#endif
}

bool SubprocessPipe::start(const std::vector<std::string>& argv, Mode mode, bool captureStderr) {
    closeAndWait();
    if (argv.empty() || argv[0].empty()) return false;

    m_mode = mode;
    m_terminated.store(false, std::memory_order_release);
    {
        std::lock_guard<std::mutex> lock(m_stderrMutex);
        m_stderrBuffer.clear();
    }

#ifdef _WIN32
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE pipeRead = nullptr;
    HANDLE pipeWrite = nullptr;
    if (!CreatePipe(&pipeRead, &pipeWrite, &sa, 0)) {
        return false;
    }

    HANDLE parentPipe = (mode == Mode::WriteStdin) ? pipeWrite : pipeRead;
    HANDLE childPipe = (mode == Mode::WriteStdin) ? pipeRead : pipeWrite;
    SetHandleInformation(parentPipe, HANDLE_FLAG_INHERIT, 0);

    HANDLE nulHandle = CreateFileW(
        L"NUL",
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        &sa,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);

    HANDLE stderrRead = nullptr;
    HANDLE stderrWrite = nullptr;
    if (captureStderr) {
        if (CreatePipe(&stderrRead, &stderrWrite, &sa, 0)) {
            SetHandleInformation(stderrRead, HANDLE_FLAG_INHERIT, 0);
        } else {
            stderrRead = nullptr;
            stderrWrite = nullptr;
        }
    }

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    if (mode == Mode::WriteStdin) {
        si.hStdInput = childPipe;
        si.hStdOutput = nulHandle;
    } else {
        si.hStdInput = nulHandle;
        si.hStdOutput = childPipe;
    }
    si.hStdError = stderrWrite ? stderrWrite : nulHandle;

    std::wstring cmdLine = buildWinCommandLine(argv);
    PROCESS_INFORMATION pi{};
    const BOOL created = CreateProcessW(
        nullptr,
        cmdLine.data(),
        nullptr,
        nullptr,
        TRUE,
        CREATE_NO_WINDOW,
        nullptr,
        nullptr,
        &si,
        &pi);

    // Always close child-side handles in parent so EOF propagates cleanly.
    CloseHandle(childPipe);
    if (stderrWrite) CloseHandle(stderrWrite);
    if (nulHandle && nulHandle != INVALID_HANDLE_VALUE) CloseHandle(nulHandle);

    if (!created) {
        CloseHandle(parentPipe);
        if (stderrRead) CloseHandle(stderrRead);
        return false;
    }

    CloseHandle(pi.hThread);
    m_processHandle = pi.hProcess;
    m_pipeHandle = parentPipe;
    m_stderrHandle = stderrRead;
    if (m_stderrHandle) startStderrThread();
    return true;
#else
    static const bool sigpipeIgnored = [] {
        ::signal(SIGPIPE, SIG_IGN);
        return true;
    }();
    (void)sigpipeIgnored;

    int dataPipe[2] = {-1, -1};
    if (::pipe(dataPipe) != 0) return false;

    int errPipe[2] = {-1, -1};
    if (captureStderr && ::pipe(errPipe) != 0) {
        ::close(dataPipe[0]);
        ::close(dataPipe[1]);
        return false;
    }

    const int devNull = ::open("/dev/null", O_RDWR);

    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);

    if (mode == Mode::WriteStdin) {
        posix_spawn_file_actions_adddup2(&actions, dataPipe[0], STDIN_FILENO);
        if (devNull >= 0) posix_spawn_file_actions_adddup2(&actions, devNull, STDOUT_FILENO);
    } else {
        if (devNull >= 0) posix_spawn_file_actions_adddup2(&actions, devNull, STDIN_FILENO);
        posix_spawn_file_actions_adddup2(&actions, dataPipe[1], STDOUT_FILENO);
    }
    if (captureStderr) {
        posix_spawn_file_actions_adddup2(&actions, errPipe[1], STDERR_FILENO);
    } else if (devNull >= 0) {
        posix_spawn_file_actions_adddup2(&actions, devNull, STDERR_FILENO);
    }

    posix_spawn_file_actions_addclose(&actions, dataPipe[0]);
    posix_spawn_file_actions_addclose(&actions, dataPipe[1]);
    if (captureStderr) {
        posix_spawn_file_actions_addclose(&actions, errPipe[0]);
        posix_spawn_file_actions_addclose(&actions, errPipe[1]);
    }
    if (devNull >= 0) {
        posix_spawn_file_actions_addclose(&actions, devNull);
    }

    std::vector<char*> cArgs;
    cArgs.reserve(argv.size() + 1);
    for (const auto& arg : argv) {
        cArgs.push_back(const_cast<char*>(arg.c_str()));
    }
    cArgs.push_back(nullptr);

    pid_t pid = -1;
    const int spawnResult = posix_spawnp(&pid, cArgs[0], &actions, nullptr, cArgs.data(), environ);
    posix_spawn_file_actions_destroy(&actions);

    if (devNull >= 0) ::close(devNull);

    if (spawnResult != 0) {
        ::close(dataPipe[0]);
        ::close(dataPipe[1]);
        if (captureStderr) {
            ::close(errPipe[0]);
            ::close(errPipe[1]);
        }
        return false;
    }

    if (mode == Mode::WriteStdin) {
        ::close(dataPipe[0]);
        m_pipeFd = dataPipe[1];
    } else {
        ::close(dataPipe[1]);
        m_pipeFd = dataPipe[0];
    }
    if (captureStderr) {
        ::close(errPipe[1]);
        m_stderrFd = errPipe[0];
    }
    m_pid = static_cast<int>(pid);
    if (m_stderrFd >= 0) startStderrThread();
    return true;
#endif
}

bool SubprocessPipe::writeAll(const void* data, size_t bytes) {
    if (m_mode != Mode::WriteStdin || !data) return false;
    const auto* cursor = static_cast<const uint8_t*>(data);
    size_t remaining = bytes;

#ifdef _WIN32
    if (!m_pipeHandle) return false;
    HANDLE handle = static_cast<HANDLE>(m_pipeHandle);
    while (remaining > 0) {
        const DWORD chunk = static_cast<DWORD>(std::min<size_t>(remaining, 1u << 24));
        DWORD written = 0;
        if (!WriteFile(handle, cursor, chunk, &written, nullptr) || written == 0) {
            return false;
        }
        cursor += written;
        remaining -= written;
    }
    return true;
#else
    if (m_pipeFd < 0) return false;
    while (remaining > 0) {
        const ssize_t written = ::write(m_pipeFd, cursor, remaining);
        if (written > 0) {
            cursor += written;
            remaining -= static_cast<size_t>(written);
        } else if (written < 0 && errno == EINTR) {
            continue;
        } else {
            return false;
        }
    }
    return true;
#endif
}

bool SubprocessPipe::readExact(void* data, size_t bytes) {
    if (m_mode != Mode::ReadStdout || !data) return false;
    auto* cursor = static_cast<uint8_t*>(data);
    size_t remaining = bytes;

#ifdef _WIN32
    if (!m_pipeHandle) return false;
    HANDLE handle = static_cast<HANDLE>(m_pipeHandle);
    while (remaining > 0) {
        const DWORD chunk = static_cast<DWORD>(std::min<size_t>(remaining, 1u << 24));
        DWORD readBytes = 0;
        if (!ReadFile(handle, cursor, chunk, &readBytes, nullptr) || readBytes == 0) {
            return false;
        }
        cursor += readBytes;
        remaining -= readBytes;
    }
    return true;
#else
    if (m_pipeFd < 0) return false;
    while (remaining > 0) {
        const ssize_t readBytes = ::read(m_pipeFd, cursor, remaining);
        if (readBytes > 0) {
            cursor += readBytes;
            remaining -= static_cast<size_t>(readBytes);
        } else if (readBytes < 0 && errno == EINTR) {
            continue;
        } else {
            return false;
        }
    }
    return true;
#endif
}

void SubprocessPipe::terminateChild() {
    m_terminated.store(true, std::memory_order_release);
#ifdef _WIN32
    if (m_processHandle) {
        TerminateProcess(static_cast<HANDLE>(m_processHandle), 1);
    }
#else
    if (m_pid > 0) {
        ::kill(static_cast<pid_t>(m_pid), SIGTERM);
    }
#endif
}

int SubprocessPipe::closeAndWait() {
    int exitCode = -1;
#ifdef _WIN32
    if (m_pipeHandle) {
        CloseHandle(static_cast<HANDLE>(m_pipeHandle));
        m_pipeHandle = nullptr;
    }
    if (m_processHandle) {
        HANDLE proc = static_cast<HANDLE>(m_processHandle);
        WaitForSingleObject(proc, INFINITE);
        DWORD code = 1;
        if (GetExitCodeProcess(proc, &code)) {
            exitCode = static_cast<int>(code);
        }
        CloseHandle(proc);
        m_processHandle = nullptr;
    }
#else
    if (m_pipeFd >= 0) {
        ::close(m_pipeFd);
        m_pipeFd = -1;
    }
    if (m_pid > 0) {
        int status = 0;
        while (::waitpid(static_cast<pid_t>(m_pid), &status, 0) < 0 && errno == EINTR) {}
        if (WIFEXITED(status)) {
            exitCode = WEXITSTATUS(status);
        } else if (WIFSIGNALED(status)) {
            exitCode = 128 + WTERMSIG(status);
        }
        m_pid = -1;
    }
#endif
    stopStderrThread();
    return exitCode;
}
