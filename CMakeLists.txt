cmake_minimum_required(VERSION 3.10)
project(PlasmoidVisualizer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find packages
find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)
find_package(GLEW REQUIRED)

# Include directories
include_directories(src include external external/kissfft external/imgui)

# Source files
file(GLOB_RECURSE SOURCES "src/*.cpp")
list(APPEND SOURCES 
    "external/kissfft/kiss_fft.c"
    "external/imgui/imgui.cpp"
    "external/imgui/imgui_draw.cpp"
    "external/imgui/imgui_widgets.cpp"
    "external/imgui/imgui_tables.cpp"
    "external/imgui/imgui_impl_glfw.cpp"
    "external/imgui/imgui_impl_opengl3.cpp"
)

add_executable(PlasmoidVisualizer ${SOURCES})

# Link libraries
target_link_libraries(PlasmoidVisualizer 
    OpenGL::GL 
    glfw 
    GLEW::GLEW 
    m 
    dl 
    pthread
)
