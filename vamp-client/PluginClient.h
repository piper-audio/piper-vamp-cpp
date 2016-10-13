
#ifndef PIPER_PLUGIN_CLIENT_H
#define PIPER_PLUGIN_CLIENT_H

#include <vamp-hostsdk/PluginConfiguration.h>

namespace piper {
namespace vampclient {

class PluginStub;

class PluginClient
{
public:
    virtual
    Vamp::Plugin::OutputList
    configure(PluginStub *plugin,
              Vamp::HostExt::PluginConfiguration config) = 0;
    
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
    reset(PluginStub *plugin,
          Vamp::HostExt::PluginConfiguration config) = 0;
};

}
}

#endif
