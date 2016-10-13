
#ifndef PIPER_CLIENT_H
#define PIPER_CLIENT_H

#include <vamp-hostsdk/PluginConfiguration.h>

namespace piper { //!!! change

class PiperPluginStub;

class PiperLoaderInterface
{
public:
    virtual
    Vamp::Plugin *
    load(std::string key, float inputSampleRate, int adapterFlags) = 0;
};

class PiperPluginClientInterface
{
    friend class PiperPluginStub;
    
protected:
    virtual
    Vamp::Plugin::OutputList
    configure(PiperPluginStub *plugin,
              Vamp::HostExt::PluginConfiguration config) = 0;
    
    virtual
    Vamp::Plugin::FeatureSet
    process(PiperPluginStub *plugin,
            std::vector<std::vector<float> > inputBuffers,
            Vamp::RealTime timestamp) = 0;

    virtual Vamp::Plugin::FeatureSet
    finish(PiperPluginStub *plugin) = 0;

    virtual
    void
    reset(PiperPluginStub *plugin,
          Vamp::HostExt::PluginConfiguration config) = 0;
};

}

#endif
