cmake_minimum_required(VERSION 3.16)
project(PlasmoidVisualizer LANGUAGES C CXX)

include(CTest)
include(FetchContent)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(PLASMOID_USE_FETCHCONTENT "Fetch GLFW and GLEW from source via CMake FetchContent" ON)

# Helper to apply strict warning flags to first-party targets only.
function(plasmoid_enable_warnings target_name)
    if(MSVC)
        target_compile_options(${target_name} PRIVATE /W4 /external:W0 /D_CRT_SECURE_NO_WARNINGS)
    else()
        target_compile_options(${target_name} PRIVATE -Wall -Wextra)
    endif()
endfunction()

# Find OpenGL
set(OpenGL_GL_PREFERENCE GLVND)
find_package(OpenGL REQUIRED)

if(WIN32)
    add_compile_definitions(WIN32_LEAN_AND_MEAN NOMINMAX _USE_MATH_DEFINES)
endif()

# Resolve GLFW & GLEW dependencies.
# - On Linux: prefer system packages (find_package) and fall back to FetchContent if enabled.
# - On Windows: prefer FetchContent (builds matched Debug/Release static libraries with zero
#   CRT mismatch and zero runtime DLL requirements), with an offline fallback to local DLL libs.
set(PLASMOID_GLFW_TARGET "")
set(PLASMOID_GLEW_TARGET "")
set(PLASMOID_RUNTIME_DLLS "")

if(NOT WIN32)
    find_package(glfw3 QUIET)
    if(TARGET glfw)
        set(PLASMOID_GLFW_TARGET glfw)
    endif()
    find_package(GLEW QUIET)
    if(TARGET GLEW::GLEW)
        set(PLASMOID_GLEW_TARGET GLEW::GLEW)
    endif()
endif()

if(PLASMOID_USE_FETCHCONTENT)
    if(NOT PLASMOID_GLFW_TARGET)
        set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
        set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(
            glfw
            URL https://github.com/glfw/glfw/archive/refs/tags/3.4.tar.gz
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        )
        FetchContent_MakeAvailable(glfw)
        set(PLASMOID_GLFW_TARGET glfw)
    endif()

    if(NOT PLASMOID_GLEW_TARGET)
        set(BUILD_UTILS OFF CACHE BOOL "" FORCE)
        set(BUILD_SHARED_LIBS_SAVED "${BUILD_SHARED_LIBS}")
        set(BUILD_SHARED_LIBS OFF)
        set(CMAKE_POLICY_VERSION_MINIMUM_SAVED "${CMAKE_POLICY_VERSION_MINIMUM}")
        set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
        set(_PLASMOID_GLEW_FETCH_ARGS
            URL https://github.com/nigels-com/glew/releases/download/glew-2.3.1/glew-2.3.1.tgz
            SOURCE_SUBDIR build/cmake
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        )
        if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.28")
            list(APPEND _PLASMOID_GLEW_FETCH_ARGS EXCLUDE_FROM_ALL)
        endif()
        FetchContent_Declare(glew ${_PLASMOID_GLEW_FETCH_ARGS})
        FetchContent_MakeAvailable(glew)
        set(BUILD_SHARED_LIBS "${BUILD_SHARED_LIBS_SAVED}")
        if(CMAKE_POLICY_VERSION_MINIMUM_SAVED)
            set(CMAKE_POLICY_VERSION_MINIMUM "${CMAKE_POLICY_VERSION_MINIMUM_SAVED}")
        else()
            unset(CMAKE_POLICY_VERSION_MINIMUM)
        endif()
        add_library(plasmoid_glew INTERFACE)
        target_link_libraries(plasmoid_glew INTERFACE glew_s)
        target_include_directories(plasmoid_glew SYSTEM INTERFACE "${glew_SOURCE_DIR}/include")
        target_compile_definitions(plasmoid_glew INTERFACE GLEW_STATIC)
        set(PLASMOID_GLEW_TARGET plasmoid_glew)
    endif()
else()
    set(GLEW_MANUAL_DIR "${CMAKE_SOURCE_DIR}/glew-2.3.1")
    set(GLFW_MANUAL_DIR "${CMAKE_SOURCE_DIR}/glfw-3.4.bin.WIN64")

    if(WIN32 AND EXISTS "${GLEW_MANUAL_DIR}")
        add_library(plasmoid_glew_local SHARED IMPORTED)
        set_target_properties(plasmoid_glew_local PROPERTIES
            IMPORTED_IMPLIB "${GLEW_MANUAL_DIR}/lib/Release/x64/glew32.lib"
            IMPORTED_LOCATION "${GLEW_MANUAL_DIR}/bin/Release/x64/glew32.dll"
            INTERFACE_INCLUDE_DIRECTORIES "${GLEW_MANUAL_DIR}/include"
        )
        set(PLASMOID_GLEW_TARGET plasmoid_glew_local)
        list(APPEND PLASMOID_RUNTIME_DLLS "${GLEW_MANUAL_DIR}/bin/Release/x64/glew32.dll")
    elseif(NOT PLASMOID_GLEW_TARGET)
        find_package(GLEW REQUIRED)
        set(PLASMOID_GLEW_TARGET GLEW::GLEW)
    endif()

    if(WIN32 AND EXISTS "${GLFW_MANUAL_DIR}")
        # Link the DLL import library instead of Release static glfw3.lib to prevent Debug CRT mismatch (LNK4098).
        add_library(plasmoid_glfw_local SHARED IMPORTED)
        set_target_properties(plasmoid_glfw_local PROPERTIES
            IMPORTED_IMPLIB "${GLFW_MANUAL_DIR}/lib-vc2022/glfw3dll.lib"
            IMPORTED_LOCATION "${GLFW_MANUAL_DIR}/lib-vc2022/glfw3.dll"
            INTERFACE_INCLUDE_DIRECTORIES "${GLFW_MANUAL_DIR}/include"
        )
        set(PLASMOID_GLFW_TARGET plasmoid_glfw_local)
        list(APPEND PLASMOID_RUNTIME_DLLS "${GLFW_MANUAL_DIR}/lib-vc2022/glfw3.dll")
    elseif(NOT PLASMOID_GLFW_TARGET)
        find_package(glfw3 CONFIG REQUIRED)
        set(PLASMOID_GLFW_TARGET glfw)
    endif()
endif()

# Third-party C/C++ libraries built without first-party strict warning flags.
add_library(plasmoid_third_party STATIC
    third_party/kissfft/kiss_fft.c
    third_party/tinyexpr/tinyexpr.c
    third_party/imgui/imgui.cpp
    third_party/imgui/imgui_draw.cpp
    third_party/imgui/imgui_widgets.cpp
    third_party/imgui/imgui_tables.cpp
    third_party/imgui/imgui_impl_glfw.cpp
    third_party/imgui/imgui_impl_opengl3.cpp
)
target_include_directories(plasmoid_third_party SYSTEM PUBLIC
    third_party
    third_party/kissfft
    third_party/imgui
    third_party/tinyexpr
    third_party/tomlplusplus
)
target_link_libraries(plasmoid_third_party PUBLIC
    OpenGL::GL
    ${PLASMOID_GLFW_TARGET}
    ${PLASMOID_GLEW_TARGET}
)
if(MSVC)
    target_compile_options(plasmoid_third_party PRIVATE /W0)
else()
    target_compile_options(plasmoid_third_party PRIVATE -w)
endif()

# First-party application sources
set(FIRST_PARTY_SOURCES
    src/app/main.cpp
    src/app/AppState.cpp
    src/app/AssetPaths.cpp
    src/app/CliOptions.cpp
    src/audio/AudioEngine.cpp
    src/audio/AnalysisEngine.cpp
    src/audio/OscMusicEditor.cpp
    src/config/ConfigManager.cpp
    src/config/ConfigLogic.cpp
    src/export/VideoRenderManager.cpp
    src/export/FfmpegArgs.cpp
    src/platform/SystemStats.cpp
    src/platform/SubprocessPipe.cpp
    src/platform/Utf8Paths.cpp
    src/rendering/Visualizer.cpp
    src/rendering/QuadRenderer.cpp
    src/rendering/TextRenderer.cpp
    src/rendering/SpectrumRenderer.cpp
    src/rendering/XYTraceRenderer.cpp
    src/rendering/AnimatedBackground.cpp
    src/rendering/VisualizerGeometry.cpp
    src/rendering/XYOscilloscopeEngine.cpp
    src/rendering/ShaderProgram.cpp
    src/rendering/Framebuffer.cpp
    src/rendering/Texture2D.cpp
    src/rendering/FontAtlas.cpp
    src/rendering/GaussianBlurRenderer.cpp
    src/rendering/BloomRenderer.cpp
    src/rendering/StbImplementations.cpp
    src/rendering/RenderManager.cpp
    src/rendering/OverlayPreset.cpp
    src/rendering/LyricsTimeline.cpp
    src/ui/UIManager.cpp
    src/ui/panels/AudioSettingsPanel.cpp
    src/ui/panels/BackgroundPanel.cpp
    src/ui/panels/DiagnosticsPanels.cpp
    src/ui/panels/LayerPanels.cpp
    src/ui/panels/LyricsEditorPanel.cpp
    src/ui/panels/PlaybackPanels.cpp
    src/ui/panels/PresetsPanel.cpp
)

add_executable(PlasmoidVisualizer ${FIRST_PARTY_SOURCES})
set_target_properties(PlasmoidVisualizer PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:PlasmoidVisualizer>"
)
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${FIRST_PARTY_SOURCES})

target_include_directories(PlasmoidVisualizer PRIVATE
    src
    src/app
    src/audio
    src/config
    src/export
    src/platform
    src/rendering
    src/ui
)
plasmoid_enable_warnings(PlasmoidVisualizer)

target_link_libraries(PlasmoidVisualizer PRIVATE
    plasmoid_third_party
    OpenGL::GL
    ${PLASMOID_GLFW_TARGET}
    ${PLASMOID_GLEW_TARGET}
)
if(WIN32)
    target_link_libraries(PlasmoidVisualizer PRIVATE winmm)
else()
    target_link_libraries(PlasmoidVisualizer PRIVATE m dl pthread)
endif()

# Copy runtime assets (and any dynamic runtime DLLs) beside the executable on build.
set(ASSET_SRC_DIR "${CMAKE_SOURCE_DIR}/assets")
if(EXISTS "${ASSET_SRC_DIR}")
    add_custom_command(TARGET PlasmoidVisualizer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:PlasmoidVisualizer>/assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${ASSET_SRC_DIR}" "$<TARGET_FILE_DIR:PlasmoidVisualizer>/assets"
        COMMENT "Copying runtime assets beside PlasmoidVisualizer executable"
    )
endif()
foreach(runtime_dll ${PLASMOID_RUNTIME_DLLS})
    if(EXISTS "${runtime_dll}")
        add_custom_command(TARGET PlasmoidVisualizer POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${runtime_dll}" "$<TARGET_FILE_DIR:PlasmoidVisualizer>"
            COMMENT "Copying runtime DLL ${runtime_dll}"
        )
    endif()
endforeach()

# Install rules for release packaging
install(TARGETS PlasmoidVisualizer DESTINATION .)
if(EXISTS "${ASSET_SRC_DIR}")
    install(DIRECTORY "${ASSET_SRC_DIR}" DESTINATION .)
endif()
foreach(runtime_dll ${PLASMOID_RUNTIME_DLLS})
    if(EXISTS "${runtime_dll}")
        install(FILES "${runtime_dll}" DESTINATION .)
    endif()
endforeach()

# Unit tests
if(BUILD_TESTING)
    if(MSVC)
        set_source_files_properties(
            third_party/kissfft/kiss_fft.c
            third_party/tinyexpr/tinyexpr.c
            PROPERTIES COMPILE_OPTIONS "/W0"
        )
    else()
        set_source_files_properties(
            third_party/kissfft/kiss_fft.c
            third_party/tinyexpr/tinyexpr.c
            PROPERTIES COMPILE_OPTIONS "-w"
        )
    endif()

    add_executable(VisualizerGeometryTests
        tests/VisualizerGeometryTests.cpp
        src/rendering/VisualizerGeometry.cpp
    )
    target_include_directories(VisualizerGeometryTests PRIVATE src/rendering)
    plasmoid_enable_warnings(VisualizerGeometryTests)
    add_test(NAME VisualizerGeometryTests COMMAND VisualizerGeometryTests)

    add_executable(XYOscilloscopeEngineTests
        tests/XYOscilloscopeEngineTests.cpp
        src/rendering/XYOscilloscopeEngine.cpp
    )
    target_include_directories(XYOscilloscopeEngineTests PRIVATE src/rendering)
    plasmoid_enable_warnings(XYOscilloscopeEngineTests)
    add_test(NAME XYOscilloscopeEngineTests COMMAND XYOscilloscopeEngineTests)

    add_executable(Utf8PathsTests
        tests/Utf8PathsTests.cpp
        src/platform/Utf8Paths.cpp
    )
    target_include_directories(Utf8PathsTests PRIVATE src/platform)
    plasmoid_enable_warnings(Utf8PathsTests)
    add_test(NAME Utf8PathsTests COMMAND Utf8PathsTests)

    add_executable(ConfigManagerTests
        tests/ConfigManagerTests.cpp
        src/config/ConfigManager.cpp
        src/rendering/LyricsTimeline.cpp
        src/platform/Utf8Paths.cpp
    )
    target_include_directories(ConfigManagerTests PRIVATE
        src/config
        src/audio
        src/rendering
        src/platform
    )
    target_include_directories(ConfigManagerTests SYSTEM PRIVATE
        third_party/kissfft
        third_party/tomlplusplus
    )
    plasmoid_enable_warnings(ConfigManagerTests)
    add_test(NAME ConfigManagerTests COMMAND ConfigManagerTests)

    add_executable(PresetManagerTests
        tests/PresetManagerTests.cpp
        src/config/ConfigManager.cpp
        src/rendering/LyricsTimeline.cpp
        src/platform/Utf8Paths.cpp
    )
    target_include_directories(PresetManagerTests PRIVATE
        src/config
        src/audio
        src/rendering
        src/platform
    )
    target_include_directories(PresetManagerTests SYSTEM PRIVATE
        third_party/kissfft
        third_party/tomlplusplus
    )
    target_compile_definitions(PresetManagerTests PRIVATE
        PLASMOID_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    )
    plasmoid_enable_warnings(PresetManagerTests)
    add_test(NAME PresetManagerTests COMMAND PresetManagerTests)

    add_executable(LyricsTimelineTests
        tests/LyricsTimelineTests.cpp
        src/rendering/LyricsTimeline.cpp
    )
    target_include_directories(LyricsTimelineTests PRIVATE src/rendering)
    plasmoid_enable_warnings(LyricsTimelineTests)
    add_test(NAME LyricsTimelineTests COMMAND LyricsTimelineTests)

    add_executable(AnalysisEngineTests
        tests/AnalysisEngineTests.cpp
        src/audio/AnalysisEngine.cpp
        third_party/kissfft/kiss_fft.c
    )
    target_include_directories(AnalysisEngineTests PRIVATE src/audio)
    target_include_directories(AnalysisEngineTests SYSTEM PRIVATE third_party/kissfft)
    plasmoid_enable_warnings(AnalysisEngineTests)
    if(NOT WIN32)
        target_link_libraries(AnalysisEngineTests PRIVATE m)
    endif()
    add_test(NAME AnalysisEngineTests COMMAND AnalysisEngineTests)

    add_executable(OscMusicEditorTests
        tests/OscMusicEditorTests.cpp
        src/audio/OscMusicEditor.cpp
        third_party/tinyexpr/tinyexpr.c
    )
    target_include_directories(OscMusicEditorTests PRIVATE src/audio)
    target_include_directories(OscMusicEditorTests SYSTEM PRIVATE third_party/tinyexpr)
    plasmoid_enable_warnings(OscMusicEditorTests)
    if(NOT WIN32)
        target_link_libraries(OscMusicEditorTests PRIVATE m)
    endif()
    add_test(NAME OscMusicEditorTests COMMAND OscMusicEditorTests)

    add_executable(FfmpegArgsTests
        tests/FfmpegArgsTests.cpp
        src/export/FfmpegArgs.cpp
        src/platform/Utf8Paths.cpp
    )
    target_include_directories(FfmpegArgsTests PRIVATE
        src/export
        src/config
        src/audio
        src/rendering
        src/platform
    )
    target_include_directories(FfmpegArgsTests SYSTEM PRIVATE third_party/kissfft)
    plasmoid_enable_warnings(FfmpegArgsTests)
    add_test(NAME FfmpegArgsTests COMMAND FfmpegArgsTests)

    add_executable(AudioRingBufferTests
        tests/AudioRingBufferTests.cpp
        src/audio/AudioEngine.cpp
        src/platform/Utf8Paths.cpp
    )
    target_include_directories(AudioRingBufferTests PRIVATE
        src/audio
        src/rendering
        src/platform
    )
    target_include_directories(AudioRingBufferTests SYSTEM PRIVATE third_party)
    plasmoid_enable_warnings(AudioRingBufferTests)
    if(NOT WIN32)
        target_link_libraries(AudioRingBufferTests PRIVATE m dl pthread)
    endif()
    add_test(NAME AudioRingBufferTests COMMAND AudioRingBufferTests)

    add_executable(CliOptionsTests
        tests/CliOptionsTests.cpp
        src/app/CliOptions.cpp
    )
    target_include_directories(CliOptionsTests PRIVATE src/app)
    plasmoid_enable_warnings(CliOptionsTests)
    add_test(NAME CliOptionsTests COMMAND CliOptionsTests)
endif()
