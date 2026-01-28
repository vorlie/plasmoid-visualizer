cmake_minimum_required(VERSION 3.16)
project(PlasmoidVisualizer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find OpenGL
set(OpenGL_GL_PREFERENCE GLVND)
find_package(OpenGL REQUIRED)

# Global definitions
if(WIN32)
    add_definitions(-DWIN32_LEAN_AND_MEAN -DNOMINMAX -D_USE_MATH_DEFINES)
endif()

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
    src/VideoRenderManager.cpp
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

# Include directories
target_include_directories(PlasmoidVisualizer PRIVATE 
    src 
    include 
    external 
    external/kissfft 
    external/imgui 
    external/tinyexpr 
    external/tomlplusplus
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
