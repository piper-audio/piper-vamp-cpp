
#ifndef PIPER_LOADER_H
#define PIPER_LOADER_H

#include <vamp-hostsdk/RequestResponse.h>

namespace piper {
namespace vampclient {

class Loader
{
public:
    virtual
    Vamp::HostExt::ListResponse
    listPluginData() = 0;
    
    virtual
    Vamp::HostExt::LoadResponse
    loadPlugin(const Vamp::HostExt::LoadRequest &) = 0;
};

}
}

#endif
