cmake_minimum_required(VERSION 3.16)
project(VisualizerV2Plasma)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Quick Qml)

set(SOURCE_FILES
    src/VisualizerPlugin.cpp
    src/AudioEngineWrapper.cpp
    ../src/AudioEngine.cpp
    ../src/AnalysisEngine.cpp
    ../external/kissfft/kiss_fft.c
)

add_library(visualizerv2plugin SHARED ${SOURCE_FILES})

target_include_directories(visualizerv2plugin PRIVATE
    ../src
    ../include
    ../external
    ../external/kissfft
    ../external/kissfft/tools
)

target_link_libraries(visualizerv2plugin PRIVATE
    Qt6::Core
    Qt6::Quick
    Qt6::Qml
    pthread
    m
    ${CMAKE_DL_LIBS}
)

# Install location (adjust as needed for local testing)
install(TARGETS visualizerv2plugin DESTINATION ${CMAKE_CURRENT_SOURCE_DIR})
