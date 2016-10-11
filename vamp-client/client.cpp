
#include "stub.h"

#include "vamp-capnp/VampnProto.h"

#include "vamp-support/AssignedPluginHandleMapper.h"

namespace piper {

class PiperClient : public PiperClientBase
{
public:
    
    virtual
    Vamp::Plugin::OutputList
    configure(PiperStubPlugin *plugin,
              Vamp::HostExt::PluginConfiguration config) {

        Vamp::HostExt::ConfigurationRequest request;
        request.plugin = plugin;
        request.configuration = config;

        ::capnp::MallocMessageBuilder message;
        RpcRequest::Builder builder = message.initRoot<RpcRequest>();

        VampnProto::buildRpcRequest_Configure(builder, request, m_mapper);

        //!!! now what?
    }
    
    
    virtual
    Vamp::Plugin::FeatureSet
    process(PiperStubPlugin *plugin,
            const float *const *inputBuffers,
            Vamp::RealTime timestamp) = 0;

    virtual Vamp::Plugin::FeatureSet
    finish(PiperStubPlugin *plugin) = 0;

private:
    AssignedPluginHandleMapper m_mapper;
};
    
}

