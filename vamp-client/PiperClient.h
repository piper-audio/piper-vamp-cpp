
#ifndef PIPER_CLIENT_H
#define PIPER_CLIENT_H

#include <vamp-hostsdk/PluginConfiguration.h>

namespace piper { //!!! change

class PiperStubPlugin;

class PiperStubPluginClientInterface
{
    friend class PiperStubPlugin;
    
protected:
    virtual
    Vamp::Plugin::OutputList
    configure(PiperStubPlugin *plugin,
              Vamp::HostExt::PluginConfiguration config) = 0;
    
    virtual
    Vamp::Plugin::FeatureSet
    process(PiperStubPlugin *plugin,
            std::vector<std::vector<float> > inputBuffers,
            Vamp::RealTime timestamp) = 0;

    virtual Vamp::Plugin::FeatureSet
    finish(PiperStubPlugin *plugin) = 0;
};

}

#endif
