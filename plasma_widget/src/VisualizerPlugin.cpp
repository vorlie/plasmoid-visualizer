#include "VisualizerPlugin.hpp"
#include "AudioEngineWrapper.hpp"
#include <qqml.h>

void VisualizerPlugin::registerTypes(const char* uri) {
    qmlRegisterType<AudioEngineWrapper>(uri, 1, 0, "AudioEngine");
}
