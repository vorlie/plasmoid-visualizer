cmake_minimum_required(VERSION 3.16)
project(PlasmoidVisualizer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find packages
set(OpenGL_GL_PREFERENCE GLVND)
find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)
find_package(GLEW REQUIRED)

# Include directories
include_directories(src include external external/kissfft external/imgui external/tinyexpr external/tomlplusplus)

# Source files
set(SOURCES
    src/main.cpp
    src/AudioEngine.cpp
    src/AnalysisEngine.cpp
    src/Visualizer.cpp
    src/ConfigManager.cpp
    src/ParticleSystem.cpp
    src/SystemStats.cpp
    src/OscMusicEditor.cpp
    src/AppState.cpp
    src/RenderManager.cpp
    src/ConfigLogic.cpp
    src/UIManager.cpp
    external/kissfft/kiss_fft.c
    external/tinyexpr/tinyexpr.c
    external/imgui/imgui.cpp
    external/imgui/imgui_draw.cpp
    external/imgui/imgui_widgets.cpp
    external/imgui/imgui_tables.cpp
    external/imgui/imgui_impl_glfw.cpp
    external/imgui/imgui_impl_opengl3.cpp
)

add_executable(PlasmoidVisualizer ${SOURCES})

# Copy shaders directory into the build output so the executable can find them at runtime
set(SHADER_SRC_DIR ${CMAKE_SOURCE_DIR}/shaders)
set(SHADER_DST_DIR ${CMAKE_BINARY_DIR}/shaders)
if(EXISTS ${SHADER_SRC_DIR})
    add_custom_target(copy_shaders ALL
        COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_DST_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${SHADER_SRC_DIR} ${SHADER_DST_DIR}
        COMMENT "Copying shaders to build directory"
    )
    add_dependencies(PlasmoidVisualizer copy_shaders)
endif()

# Install shaders with `make install` (installs the shaders directory under share/<project>)
if(EXISTS ${SHADER_SRC_DIR})
    install(DIRECTORY ${SHADER_SRC_DIR} DESTINATION share/${PROJECT_NAME})
endif()

# Link libraries
target_link_libraries(PlasmoidVisualizer 
    OpenGL::GL 
    glfw 
    GLEW::GLEW 
    m 
    dl 
    pthread
)
