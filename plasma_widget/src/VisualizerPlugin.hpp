#ifndef VISUALIZER_PLUGIN_HPP
#define VISUALIZER_PLUGIN_HPP

#include <QQmlExtensionPlugin>

class VisualizerPlugin : public QQmlExtensionPlugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QQmlExtensionInterface_iid)

public:
    void registerTypes(const char* uri) override;
};

#endif // VISUALIZER_PLUGIN_HPP
