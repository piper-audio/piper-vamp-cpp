
#ifndef PIPER_LOADER_H
#define PIPER_LOADER_H

#include <vamp-hostsdk/Plugin.h>

namespace piper {
namespace vampclient {

class Loader
{
public:
    virtual
    Vamp::Plugin *
    load(std::string key, float inputSampleRate, int adapterFlags) = 0;
};

}
}

#endif
