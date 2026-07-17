cmake_minimum_required(VERSION 3.16)
project(PlasmoidVisualizer)

include(CTest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find OpenGL
set(OpenGL_GL_PREFERENCE GLVND)
find_package(OpenGL REQUIRED)

# Global definitions
if(WIN32)
    add_definitions(-DWIN32_LEAN_AND_MEAN -DNOMINMAX -D_USE_MATH_DEFINES)
endif()

if(BUILD_TESTING)
    add_executable(VisualizerGeometryTests
        tests/VisualizerGeometryTests.cpp
        src/rendering/VisualizerGeometry.cpp
    )
    target_include_directories(VisualizerGeometryTests PRIVATE src/rendering)
    add_test(NAME VisualizerGeometryTests COMMAND VisualizerGeometryTests)
endif()

# Source files
set(SOURCES
    src/app/main.cpp
    src/app/AppState.cpp
    src/app/AssetPaths.cpp
    src/audio/AudioEngine.cpp
    src/audio/AnalysisEngine.cpp
    src/audio/OscMusicEditor.cpp
    src/config/ConfigManager.cpp
    src/config/ConfigLogic.cpp
    src/export/VideoRenderManager.cpp
    src/platform/SystemStats.cpp
    src/rendering/Visualizer.cpp
    src/rendering/VisualizerGeometry.cpp
    src/rendering/ShaderProgram.cpp
    src/rendering/Framebuffer.cpp
    src/rendering/Texture2D.cpp
    src/rendering/FontAtlas.cpp
    src/rendering/BloomRenderer.cpp
    src/rendering/StbImplementations.cpp
    src/rendering/ParticleSystem.cpp
    src/rendering/RenderManager.cpp
    src/ui/UIManager.cpp
    src/ui/panels/AudioSettingsPanel.cpp
    src/ui/panels/BackgroundPanel.cpp
    src/ui/panels/DiagnosticsPanels.cpp
    src/ui/panels/LayerPanels.cpp
    src/ui/panels/PlaybackPanels.cpp
    third_party/kissfft/kiss_fft.c
    third_party/tinyexpr/tinyexpr.c
    third_party/imgui/imgui.cpp
    third_party/imgui/imgui_draw.cpp
    third_party/imgui/imgui_widgets.cpp
    third_party/imgui/imgui_tables.cpp
    third_party/imgui/imgui_impl_glfw.cpp
    third_party/imgui/imgui_impl_opengl3.cpp
)

add_executable(PlasmoidVisualizer ${SOURCES})
set_target_properties(PlasmoidVisualizer PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
)

# Keep Visual Studio's Solution Explorer aligned with the on-disk layout.
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${SOURCES})

# Include directories
target_include_directories(PlasmoidVisualizer PRIVATE 
    src
    src/app
    src/audio
    src/config
    src/export
    src/platform
    src/rendering
    src/ui
    include 
    third_party
    third_party/kissfft
    third_party/imgui
    third_party/tinyexpr
    third_party/tomlplusplus
)

# Manage local dependencies (GLM, GLEW, GLFW)
set(GLEW_MANUAL_DIR "${CMAKE_SOURCE_DIR}/glew-2.3.1")
set(GLFW_MANUAL_DIR "${CMAKE_SOURCE_DIR}/glfw-3.4.bin.WIN64")
set(GLM_MANUAL_DIR "${CMAKE_SOURCE_DIR}/glm")

if(EXISTS "${GLM_MANUAL_DIR}")
    message(STATUS "Integrating local GLM from ${CMAKE_SOURCE_DIR}")
    target_include_directories(PlasmoidVisualizer SYSTEM PRIVATE "${CMAKE_SOURCE_DIR}")
endif()

if(EXISTS "${GLEW_MANUAL_DIR}")
    message(STATUS "Integrating local GLEW from ${GLEW_MANUAL_DIR}")
    target_include_directories(PlasmoidVisualizer SYSTEM PRIVATE "${GLEW_MANUAL_DIR}/include")
    if(WIN32)
        set(GLEW_LIBRARIES_WIN "${GLEW_MANUAL_DIR}/lib/Release/x64/glew32.lib")
        set(GLEW_FOUND TRUE)
    endif()
endif()

if(EXISTS "${GLFW_MANUAL_DIR}")
    message(STATUS "Integrating local GLFW from ${GLFW_MANUAL_DIR}")
    target_include_directories(PlasmoidVisualizer SYSTEM PRIVATE "${GLFW_MANUAL_DIR}/include")
    if(WIN32)
        set(GLFW_LIBRARIES_WIN "${GLFW_MANUAL_DIR}/lib-vc2022/glfw3.lib")
        set(glfw3_FOUND TRUE)
    endif()
endif()

# Fallback find_package
if(NOT glew_FOUND AND NOT GLEW_FOUND)
    find_package(GLEW)
    if(GLEW_FOUND)
        set(GLEW_LIBRARIES_WIN ${GLEW_LIBRARIES})
    endif()
endif()

if(NOT glfw3_FOUND)
    find_package(glfw3 CONFIG)
    if(glfw3_FOUND)
        set(GLFW_LIBRARIES_WIN glfw)
    endif()
endif()

# Copy runtime assets into the build output.
set(ASSET_SRC_DIR ${CMAKE_SOURCE_DIR}/assets)
set(ASSET_DST_DIR ${CMAKE_BINARY_DIR}/assets)
if(EXISTS ${ASSET_SRC_DIR})
    add_custom_target(copy_assets ALL
        COMMAND ${CMAKE_COMMAND} -E make_directory ${ASSET_DST_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${ASSET_SRC_DIR} ${ASSET_DST_DIR}
        COMMENT "Copying runtime assets to build directory"
    )
    add_dependencies(PlasmoidVisualizer copy_assets)
endif()

# Install rules for release packaging
# The executable
install(TARGETS PlasmoidVisualizer DESTINATION .)

# Install runtime assets.
if(EXISTS ${ASSET_SRC_DIR})
    install(DIRECTORY ${ASSET_SRC_DIR} DESTINATION .)
endif()

# Install GLEW DLL on Windows
if(WIN32 AND EXISTS "${GLEW_MANUAL_DIR}")
    set(GLEW_DLL_PATH "${GLEW_MANUAL_DIR}/bin/Release/x64/glew32.dll")
    if(EXISTS "${GLEW_DLL_PATH}")
        install(FILES "${GLEW_DLL_PATH}" DESTINATION .)
    else()
        message(WARNING "GLEW DLL not found at ${GLEW_DLL_PATH}")
    endif()
endif()


# Link libraries
if(WIN32)
    target_link_libraries(PlasmoidVisualizer 
        OpenGL::GL 
        ${GLFW_LIBRARIES_WIN}
        ${GLEW_LIBRARIES_WIN}
    )
else()
    target_link_libraries(PlasmoidVisualizer 
        OpenGL::GL 
        glfw 
        GLEW::GLEW 
        m 
        dl 
        pthread
    )
endif()
