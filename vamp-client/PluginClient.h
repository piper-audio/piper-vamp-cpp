
#ifndef PIPER_PLUGIN_CLIENT_H
#define PIPER_PLUGIN_CLIENT_H

#include "vamp-support/PluginConfiguration.h"

namespace piper_vamp {
namespace client {

class PluginStub;

class PluginClient
{
public:
    virtual
    Vamp::Plugin::OutputList
    configure(PluginStub *plugin,
              PluginConfiguration config) = 0;
    
    virtual
    Vamp::Plugin::FeatureSet
    process(PluginStub *plugin,
            std::vector<std::vector<float> > inputBuffers,
            Vamp::RealTime timestamp) = 0;

    virtual
    Vamp::Plugin::FeatureSet
    finish(PluginStub *plugin) = 0;

    virtual
    void
    reset(PluginStub *plugin, PluginConfiguration config) = 0;
};

}
}

#endif
