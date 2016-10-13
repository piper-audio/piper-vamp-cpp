
#ifndef PIPER_LOADER_H
#define PIPER_LOADER_H

#include "vamp-support/RequestResponse.h"

namespace piper_vamp {
namespace client {

class Loader
{
public:
    virtual ListResponse listPluginData() = 0;
    virtual LoadResponse loadPlugin(const LoadRequest &) = 0;
};

}
}

#endif
