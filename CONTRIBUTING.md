# Contributing to Plasmoid Visualizer

Thank you for contributing! Please follow the build, testing, and commit guidelines below so changes stay small, safe, and easy to review.

## Building & Testing Locally

### Windows (Visual Studio 2022)
```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
cmake --build build --config Debug --parallel
ctest --test-dir build -C Debug --output-on-failure
```

### Linux (GCC / Clang)
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

## Code Standards

- **Zero First-Party Warnings**: First-party targets compile with `/W4` (MSVC) and `-Wall -Wextra` (GCC/Clang). Fix warnings in first-party code rather than disabling them.
- **Leave Vendored Code Alone**: Do not modify files under `third_party/` unless updating an upstream dependency intentionally.
- **Real-Time Audio Safety**: Never lock mutexes, allocate heap memory, perform file/console I/O, or invoke blocking APIs inside `AudioEngine::dataCallback`.
- **Resource Lifetimes**: Prefer move-only RAII handles (`GlResources.hpp`, `std::unique_ptr` with custom deleters) over manual `glDelete*` / `free` cleanup.
- **Formatting**: Follow [.editorconfig](.editorconfig) (UTF-8, LF line endings for C++/CMake/Markdown, 4-space indentation).
- **Config Compatibility**: Keep `config.toml` keys backward-compatible and add or update a test in `tests/ConfigManagerTests.cpp` whenever changing `PersistentConfig`.

## Commit Message Conventions

Use concise, descriptive Conventional Commits:

```text
<type>(<scope>): <imperative summary>
```

Common types: `feat`, `fix`, `perf`, `refactor`, `test`, `build`, `ci`, `docs`, `chore`.

Examples:
- `fix(audio): replace mutex in callback with wait-free SPSC ring buffer`
- `perf(export): add double-buffered PBO readback and writer thread`
- `test(config): verify legacy XY key migration and numeric clamping`

Avoid vague messages like `wip`, `asd`, or `.`.
